// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/fwupd/firmware_update_manager.h"

#include <utility>

#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/base_paths.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"
#include "dbus/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

namespace {

const char kBaseRootPath[] = "firmware-updates";
const char kCachePath[] = "cache";
const char kCabFileExtension[] = ".cab";

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

// TODO(jimmyxgong): Stub function, implement when firmware version ID is
// available.
std::string GetFilenameFromDevice(const std::string& device_id, int release) {
  return device_id + std::string(kCabFileExtension);
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
  return update;
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

  if (HasPendingUpdates()) {
    NotifyUpdateListObservers();
  }
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
                                         int release,
                                         base::OnceCallback<void()> callback) {
  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_TEMP, &root_dir));
  const base::FilePath cache_path =
      root_dir.Append(FILE_PATH_LITERAL(kBaseRootPath))
          .Append(FILE_PATH_LITERAL(kCachePath));

  base::OnceClosure dir_created_callback =
      base::BindOnce(&FirmwareUpdateManager::OnCacheDirectoryCreated,
                     weak_ptr_factory_.GetWeakPtr(), cache_path, device_id,
                     release, std::move(callback));

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& path) {
            if (!CreateDirIfNotExists(path)) {
              LOG(ERROR) << "Cannot create firmware update directory, "
                         << " may be created already.";
            }
          },
          cache_path),
      std::move(dir_created_callback));
}

void FirmwareUpdateManager::OnCacheDirectoryCreated(
    const base::FilePath& cache_path,
    const std::string& device_id,
    int release,
    base::OnceCallback<void()> callback) {
  const base::FilePath patch_path =
      cache_path.Append(GetFilenameFromDevice(device_id, release));

  // TODO(jimmyxgong): Determine if this options map can be static or will need
  // to remain dynamic.
  // Fwupd Install Dbus flags, flag documentation can be found in
  // https://github.com/fwupd/fwupd/blob/main/libfwupd/fwupd-enums.h#L749.
  std::map<std::string, bool> options = {{"none", false},
                                         {"force", true},
                                         {"allow-older", true},
                                         {"allow-reinstall", true}};

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&OpenFileAndGetFileDescriptor, patch_path),
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

  chromeos::FwupdClient::Get()->InstallUpdate(
      device_id, std::move(file_descriptor), options);

  std::move(callback).Run();
}

void FirmwareUpdateManager::OnDeviceListResponse(
    chromeos::FwupdDeviceList* devices) {
  DCHECK(devices);
  DCHECK(!HasPendingUpdates());

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
  ++on_install_update_response_count_for_testing_;
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

}  // namespace ash
