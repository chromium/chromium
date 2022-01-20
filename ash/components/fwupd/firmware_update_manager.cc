// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/fwupd/firmware_update_manager.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/fwupd_download_client.h"
#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"
#include "dbus/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {
// State of the fwupd daemon. Enum defined here:
// https://github.com/fwupd/fwupd/blob/4389f9f913588edae7243a8dbed88ce3788c8bc2/libfwupd/fwupd-enums.h
enum class FwupdStatus {
  kUnknown,
  kIdle,
  kLoading,
  kDecompressing,
  kDeviceRestart,
  kDeviceWrite,
  kDeviceVerify,
  kScheduling,
  kDownloading,
  kDeviceRead,
  kDeviceErase,
  kWaitingForAuth,
  kDeviceBusy,
  kShutdown,
};

static constexpr auto FwupdStatusStringMap =
    base::MakeFixedFlatMap<FwupdStatus, const char*>(
        {{FwupdStatus::kUnknown, "Unknown state"},
         {FwupdStatus::kIdle, "Idle state"},
         {FwupdStatus::kLoading, "Loading a resource"},
         {FwupdStatus::kDecompressing, "Decompressing firmware"},
         {FwupdStatus::kDeviceRestart, "Restarting the device"},
         {FwupdStatus::kDeviceWrite, "Writing to a device"},
         {FwupdStatus::kDeviceVerify, "Verifying (reading) a device"},
         {FwupdStatus::kScheduling, "Scheduling an offline update"},
         {FwupdStatus::kDownloading, "A file is downloading"},
         {FwupdStatus::kDeviceRead, "Reading from a device"},
         {FwupdStatus::kDeviceErase, "Erasing a device"},
         {FwupdStatus::kWaitingForAuth, "Waiting for authentication"},
         {FwupdStatus::kDeviceBusy, "The device is busy"},
         {FwupdStatus::kShutdown, "The daemon is shutting down"}});

const char* GetFwupdStatusString(FwupdStatus enum_val) {
  DCHECK(base::Contains(FwupdStatusStringMap, enum_val));
  return FwupdStatusStringMap.at(enum_val);
}

const char kBaseRootPath[] = "firmware-updates";
const char kCachePath[] = "cache";
const char kCabFileExtension[] = ".cab";
const char kFirmwareMirrorPrefix[] =
    "https://storage.googleapis.com/chromeos-localmirror/lvfs/";
const char kAllowedFilepathChars[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+-._";

FirmwareUpdateManager* g_instance = nullptr;

base::ScopedFD OpenFileAndGetFileDescriptor(base::FilePath download_path) {
  base::File dest_file(download_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!dest_file.IsValid() || !base::PathExists(download_path)) {
    LOG(ERROR) << "Invalid destination file at path: " << download_path;
    return base::ScopedFD();
  }

  return base::ScopedFD(dest_file.TakePlatformFile());
}

bool CreateDirIfNotExists(const base::FilePath& path) {
  return base::DirectoryExists(path) || base::CreateDirectory(path);
}

firmware_update::mojom::FirmwareUpdatePtr CreateUpdate(
    const chromeos::FwupdUpdate& update_details,
    const std::string& device_id,
    const std::string& device_name) {
  auto update = firmware_update::mojom::FirmwareUpdate::New();
  update->device_id = device_id;
  update->device_name = base::UTF8ToUTF16(device_name);
  update->device_version = update_details.version;
  update->device_description = base::UTF8ToUTF16(update_details.description);
  update->priority =
      firmware_update::mojom::UpdatePriority(update_details.priority);
  update->filepath = update_details.filepath;
  return update;
}

constexpr net::NetworkTrafficAnnotationTag kFwupdFirmwareUpdateNetworkTag =
    net::DefineNetworkTrafficAnnotation("fwupd_firmware_update", R"(
        semantics {
          sender: "FWUPD firmware update"
          description:
            "Get the firmware update patch file from url and store it in the "
            "the device cache. This is used to update a specific peripheral's "
            "firmware."

          trigger:
            "Triggered by the user when they explicitly use the Firmware Update"
            " UI to update their peripheral."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
             "This feature is used when the user updates their firmware."
          policy_exception_justification:
             "This request is made based on the user decision to update "
             "firmware."
        })");

std::unique_ptr<network::SimpleURLLoader> CreateSimpleURLLoader(GURL url) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kFwupdFirmwareUpdateNetworkTag);
}

int GetResponseCode(network::SimpleURLLoader* simple_loader) {
  if (simple_loader->ResponseInfo() && simple_loader->ResponseInfo()->headers)
    return simple_loader->ResponseInfo()->headers->response_code();
  else
    return -1;
}

// TODO(michaelcheco): Determine if more granular states are needed.
firmware_update::mojom::UpdateState GetUpdateState(FwupdStatus fwupd_status) {
  switch (fwupd_status) {
    case FwupdStatus::kUnknown:
      return firmware_update::mojom::UpdateState::kUnknown;
    case FwupdStatus::kIdle:
    case FwupdStatus::kLoading:
    case FwupdStatus::kDecompressing:
    case FwupdStatus::kDeviceVerify:
    case FwupdStatus::kScheduling:
    case FwupdStatus::kDownloading:
    case FwupdStatus::kDeviceRead:
    case FwupdStatus::kDeviceErase:
    case FwupdStatus::kWaitingForAuth:
    case FwupdStatus::kDeviceBusy:
    case FwupdStatus::kShutdown:
      return firmware_update::mojom::UpdateState::kIdle;
    case FwupdStatus::kDeviceRestart:
      return firmware_update::mojom::UpdateState::kRestarting;
    case FwupdStatus::kDeviceWrite:
      return firmware_update::mojom::UpdateState::kUpdating;
  }
}

bool IsValidFirmwarePatchFile(const base::FilePath& filepath) {
  // Check if the extension is .cab.
  std::string extension = filepath.Extension();
  if (extension != kCabFileExtension) {
    return false;
  }

  // Check for invalid characters in filepath.
  return base::ContainsOnlyChars(filepath.value(), kAllowedFilepathChars);
}

}  // namespace

FirmwareUpdateManager::FirmwareUpdateManager()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  DCHECK(chromeos::FwupdClient::Get());
  chromeos::FwupdClient::Get()->AddObserver(this);

  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

FirmwareUpdateManager::~FirmwareUpdateManager() {
  DCHECK_EQ(this, g_instance);
  chromeos::FwupdClient::Get()->RemoveObserver(this);
  g_instance = nullptr;
}

// static
FirmwareUpdateManager* FirmwareUpdateManager::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void FirmwareUpdateManager::NotifyUpdateListObservers() {
  for (auto& observer : update_list_observers_) {
    observer->OnUpdateListChanged(mojo::Clone(updates_));
  }
}

bool FirmwareUpdateManager::HasPendingUpdates() {
  return !devices_pending_update_.empty();
}

void FirmwareUpdateManager::ObservePeripheralUpdates(
    mojo::PendingRemote<firmware_update::mojom::UpdateObserver> observer) {
  update_list_observers_.Add(std::move(observer));
  if (!HasPendingUpdates()) {
    RequestAllUpdates();
  }
}

// TODO(michaelcheco): Handle the case where the app is closed during an
// install.
void FirmwareUpdateManager::ResetInstallState() {
  install_controller_receiver_.reset();
  update_progress_observer_.reset();
}

void FirmwareUpdateManager::PrepareForUpdate(
    const std::string& device_id,
    PrepareForUpdateCallback callback) {
  DCHECK(!device_id.empty());

  mojo::PendingRemote<firmware_update::mojom::InstallController>
      pending_remote = install_controller_receiver_.BindNewPipeAndPassRemote();
  install_controller_receiver_.set_disconnect_handler(base::BindOnce(
      &FirmwareUpdateManager::ResetInstallState, base::Unretained(this)));
  std::move(callback).Run(std::move(pending_remote));
}

void FirmwareUpdateManager::FetchInProgressUpdate(
    FetchInProgressUpdateCallback callback) {
  std::move(callback).Run(mojo::Clone(inflight_update_));
}

// Query all updates for all devices.
void FirmwareUpdateManager::RequestAllUpdates() {
  DCHECK(!HasPendingUpdates());
  RequestDevices();
}

void FirmwareUpdateManager::RequestDevices() {
  chromeos::FwupdClient::Get()->RequestDevices();
}

void FirmwareUpdateManager::RequestUpdates(const std::string& device_id) {
  chromeos::FwupdClient::Get()->RequestUpdates(device_id);
}

// TODO(jimmyxgong): Currently only looks for the local cache for the update
// file. This needs to update to fetch the update file from a server and
// download it to the local cache.
void FirmwareUpdateManager::StartInstall(const std::string& device_id,
                                         const base::FilePath& filepath,
                                         base::OnceCallback<void()> callback) {
  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &root_dir));
  const base::FilePath cache_path =
      root_dir.Append(FILE_PATH_LITERAL(kBaseRootPath))
          .Append(FILE_PATH_LITERAL(kCachePath));

  base::OnceClosure dir_created_callback =
      base::BindOnce(&FirmwareUpdateManager::CreateLocalPatchFile,
                     weak_ptr_factory_.GetWeakPtr(), cache_path, device_id,
                     filepath, std::move(callback));

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path) {
            if (!CreateDirIfNotExists(path)) {
              LOG(ERROR) << "Cannot create firmware update directory, "
                         << "may be created already.";
            }
          },
          cache_path),
      std::move(dir_created_callback));
}

void FirmwareUpdateManager::CreateLocalPatchFile(
    const base::FilePath& cache_path,
    const std::string& device_id,
    const base::FilePath& filepath,
    base::OnceCallback<void()> callback) {
  const base::FilePath patch_path = cache_path.Append(filepath.value());

  // Create the patch file.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& patch_path) {
            // TODO(michaelcheco): Verify that creating the empty file is
            // necessary.
            const bool write_file_success =
                base::WriteFile(patch_path, /*data=*/"");
            if (!write_file_success) {
              LOG(ERROR) << "Writing into the file: " << patch_path
                         << " failed.";
            }
            return write_file_success;
          },
          patch_path),
      base::BindOnce(&FirmwareUpdateManager::DownloadFileToInternal,
                     weak_ptr_factory_.GetWeakPtr(), patch_path, device_id,
                     filepath, std::move(callback)));
}

void FirmwareUpdateManager::DownloadFileToInternal(
    const base::FilePath& patch_path,
    const std::string& device_id,
    const base::FilePath& filepath,
    base::OnceCallback<void()> callback,
    bool write_file_success) {
  if (!write_file_success) {
    return;
  }
  const std::string url = std::string(kFirmwareMirrorPrefix) + filepath.value();
  GURL download_url(fake_url_for_testing_.empty() ? url
                                                  : fake_url_for_testing_);

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      CreateSimpleURLLoader(download_url);
  DCHECK(FwupdDownloadClient::Get());

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      FwupdDownloadClient::Get()->GetURLLoaderFactory();
  // Save the pointer before moving `simple_loader` in the following call to
  // `DownloadToFile()`.
  auto* loader_ptr = simple_loader.get();

  loader_ptr->DownloadToFile(
      loader_factory.get(),
      base::BindOnce(&FirmwareUpdateManager::OnUrlDownloadedToFile,
                     weak_ptr_factory_.GetWeakPtr(), device_id,
                     std::move(simple_loader), std::move(callback)),
      patch_path);
}

void FirmwareUpdateManager::OnUrlDownloadedToFile(
    const std::string& device_id,
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    base::OnceCallback<void()> callback,
    base::FilePath download_path) {
  if (simple_loader->NetError() != net::OK) {
    LOG(ERROR) << "Downloading to file failed with error code: "
               << GetResponseCode(simple_loader.get()) << " with network error "
               << simple_loader->NetError();
    std::move(callback).Run();
    return;
  }

  // TODO(jimmyxgong): Determine if this options map can be static or will need
  // to remain dynamic.
  // Fwupd Install Dbus flags, flag documentation can be found in
  // https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h#L749.
  std::map<std::string, bool> options = {{"none", false},
                                         {"force", true},
                                         {"allow-older", true},
                                         {"allow-reinstall", true}};

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&OpenFileAndGetFileDescriptor, download_path),
      base::BindOnce(&FirmwareUpdateManager::InstallUpdate,
                     weak_ptr_factory_.GetWeakPtr(), device_id,
                     std::move(options), std::move(callback)));
}

void FirmwareUpdateManager::InstallUpdate(
    const std::string& device_id,
    chromeos::FirmwareInstallOptions options,
    base::OnceCallback<void()> callback,
    base::ScopedFD file_descriptor) {
  if (!file_descriptor.is_valid()) {
    LOG(ERROR) << "Invalid file descriptor.";
    std::move(callback).Run();
    return;
  }

  DCHECK(inflight_update_.is_null());
  for (const auto& update : updates_) {
    if (update->device_id == device_id) {
      inflight_update_ = mojo::Clone(update);
      break;
    }
  }

  chromeos::FwupdClient::Get()->InstallUpdate(
      device_id, std::move(file_descriptor), options);

  std::move(callback).Run();
}

void FirmwareUpdateManager::OnDeviceListResponse(
    chromeos::FwupdDeviceList* devices) {
  DCHECK(devices);
  DCHECK(!HasPendingUpdates());
  // Clear all cached updates prior to fetching the new update list.
  updates_.clear();

  // Fire the observer with an empty list if there are no devices in the
  // response.
  if (devices->empty()) {
    NotifyUpdateListObservers();
    return;
  }

  for (const auto& device : *devices) {
    devices_pending_update_[device.id] = device;
    RequestUpdates(device.id);
  }
}

void FirmwareUpdateManager::OnUpdateListResponse(
    const std::string& device_id,
    chromeos::FwupdUpdateList* updates) {
  DCHECK(updates);
  DCHECK(base::Contains(devices_pending_update_, device_id));

  // If there are updates, then choose the first one.
  if (!updates->empty()) {
    auto device_name = devices_pending_update_[device_id].device_name;
    // Create a complete FirmwareUpdate and add to updates_.
    updates_.push_back(CreateUpdate(updates->front(), device_id, device_name));
  }

  // Remove the pending device.
  devices_pending_update_.erase(device_id);

  // Fire the observer if there are no devices pending updates.
  if (!HasPendingUpdates()) {
    NotifyUpdateListObservers();
  }
}

void FirmwareUpdateManager::OnInstallResponse(bool success) {
  auto state = success ? firmware_update::mojom::UpdateState::kSuccess
                       : firmware_update::mojom::UpdateState::kFailed;
  // Success or Fail states are both considered 100% done.
  auto update = ash::firmware_update::mojom::InstallationProgress::New(
      /**percentage=*/100, state);

  // If the firmware update app is closed, the observer is no longer bound.
  if (update_progress_observer_.is_bound()) {
    update_progress_observer_->OnStatusChanged(std::move(update));
  }

  // Any updates are completed at this point, reset all cached.
  ResetInstallState();
  inflight_update_.reset();
}

void FirmwareUpdateManager::BindInterface(
    mojo::PendingReceiver<firmware_update::mojom::UpdateProvider>
        pending_receiver) {
  // Clear any bound receiver, since this service is a singleton and is bound
  // to the firmware updater UI it's possible that the app can be closed and
  // reopened multiple times resulting in multiple attempts to bind to this
  // receiver.
  receiver_.reset();

  receiver_.Bind(std::move(pending_receiver));
}

void FirmwareUpdateManager::OnPropertiesChangedResponse(
    chromeos::FwupdProperties* properties) {
  if (!properties || !update_progress_observer_.is_bound()) {
    return;
  }
  const auto status = FwupdStatus(properties->status.value());
  const auto percentage = properties->percentage.value();
  VLOG(1) << "fwupd: OnPropertiesChangedResponse called with Status: "
          << GetFwupdStatusString(static_cast<FwupdStatus>(status))
          << " | Percentage: " << percentage;
  update_progress_observer_->OnStatusChanged(
      ash::firmware_update::mojom::InstallationProgress::New(
          percentage, GetUpdateState(status)));
}

void FirmwareUpdateManager::BeginUpdate(const std::string& device_id,
                                        const base::FilePath& filepath) {
  DCHECK(!filepath.empty());

  if (!IsValidFirmwarePatchFile(filepath)) {
    return;
  }

  StartInstall(device_id, filepath, /**callback=*/base::DoNothing());
}

void FirmwareUpdateManager::AddObserver(
    mojo::PendingRemote<firmware_update::mojom::UpdateProgressObserver>
        observer) {
  update_progress_observer_.reset();
  update_progress_observer_.Bind(std::move(observer));
}
}  // namespace ash
