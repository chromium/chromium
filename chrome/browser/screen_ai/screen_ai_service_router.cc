// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "mojo/public/mojom/base/file_path.mojom.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// If any value is added, please update `ComponentAvailability` in `enums.xml`.
enum class ComponentAvailability {
  kAvailable = 0,
  kUnavailableWithNetwork = 1,
  kUnavailableWithoutNetwork = 2,

  kMaxValue = kUnavailableWithoutNetwork,
};

bool IsModelFileContentReadable(base::File& file) {
  if (!file.IsValid()) {
    return false;
  }
  int file_size = file.GetLength();
  if (!file_size) {
    return false;
  }
  std::vector<uint8_t> buffer(file_size);
  return file.ReadAndCheck(0, base::make_span(buffer));
}

// The name of the file that contains the list of files that are downloaded with
// the component and are required to initialize the library.
const base::FilePath::CharType kMainContentExtractionFilesList[] =
    FILE_PATH_LITERAL("files_list_main_content_extraction.txt");
const base::FilePath::CharType kOcrFilesList[] =
    FILE_PATH_LITERAL("files_list_ocr.txt");

class ComponentFiles {
 public:
  explicit ComponentFiles(const base::FilePath& library_binary_path,
                          const base::FilePath::CharType* files_list_file_name);
  ComponentFiles(const ComponentFiles&) = delete;
  ComponentFiles& operator=(const ComponentFiles&) = delete;
  ~ComponentFiles();

  static std::unique_ptr<ComponentFiles> Load(
      const base::FilePath::CharType* files_list_file_name);

  base::flat_map<base::FilePath, base::File> model_files_;
  base::FilePath library_binary_path_;
};

ComponentFiles::ComponentFiles(
    const base::FilePath& library_binary_path,
    const base::FilePath::CharType* files_list_file_name)
    : library_binary_path_(library_binary_path) {
  base::FilePath component_folder = library_binary_path.DirName();

  // Get the files list.
  std::string file_content;
  if (!base::ReadFileToString(component_folder.Append(files_list_file_name),
                              &file_content)) {
    VLOG(0) << "Could not read list of files for " << files_list_file_name;
    return;
  }
  std::vector<std::string> files_list = base::SplitString(
      file_content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (files_list.empty()) {
    VLOG(0) << "Could not parse files list for " << files_list_file_name;
    return;
  }

  for (auto& relative_file_path : files_list) {
    // Ignore comment lines.
    if (relative_file_path.empty() || relative_file_path[0] == '#') {
      continue;
    }

#if BUILDFLAG(IS_WIN)
    base::FilePath relative_path(base::UTF8ToWide(relative_file_path));
#else
    base::FilePath relative_path(relative_file_path);
#endif
    const base::FilePath full_path = component_folder.Append(relative_path);
    model_files_[relative_path] =
        base::File(full_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!IsModelFileContentReadable(model_files_[relative_path])) {
      VLOG(0) << "Could not open " << full_path;
      model_files_.clear();
      return;
    }
  }
}

ComponentFiles::~ComponentFiles() {
  if (model_files_.empty()) {
    return;
  }

  // Transfer ownership of the file handles to a thread that may block, and let
  // them get destroyed there.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](base::flat_map<base::FilePath, base::File> model_files) {},
          std::move(model_files_)));
}

std::unique_ptr<ComponentFiles> ComponentFiles::Load(
    const base::FilePath::CharType* files_list_file_name) {
  return std::make_unique<ComponentFiles>(
      screen_ai::ScreenAIInstallState::GetInstance()
          ->get_component_binary_path(),
      files_list_file_name);
}

void RecordComponentAvailability(bool available) {
  bool network = !content::GetNetworkConnectionTracker()->IsOffline();
  base::UmaHistogramEnumeration(
      "Accessibility.ScreenAI.Component.Available2",
      available
          ? ComponentAvailability::kAvailable
          : (network ? ComponentAvailability::kUnavailableWithNetwork
                     : ComponentAvailability::kUnavailableWithoutNetwork));
}

}  // namespace

namespace screen_ai {

ScreenAIServiceRouter::ScreenAIServiceRouter() = default;
ScreenAIServiceRouter::~ScreenAIServiceRouter() = default;

std::optional<bool> ScreenAIServiceRouter::GetServiceState(Service service) {
  switch (service) {
    case Service::kOCR:
      if (ocr_service_.is_bound()) {
        return true;
      } else if (features::IsScreenAIOCREnabled()) {
        return std::nullopt;
      } else {
        return false;
      }

    case Service::kMainContentExtraction:
      if (main_content_extraction_service_.is_bound()) {
        return true;
      } else if (features::IsScreenAIMainContentExtractionEnabled()) {
        return std::nullopt;
      } else {
        return false;
      }
  }
}

void ScreenAIServiceRouter::GetServiceStateAsync(
    Service service,
    ServiceStateCallback callback) {
  auto service_state = GetServiceState(service);
  if (service_state) {
    // Either service is already initialized or disabled.
    std::move(callback).Run(*service_state);
    RecordComponentAvailability(true);
    return;
  }

  pending_state_requests_[service].emplace_back(std::move(callback));

  auto* install_state = ScreenAIInstallState::GetInstance();

  // If download has previously failed, reset it.
  if (install_state->get_state() ==
      ScreenAIInstallState::State::kDownloadFailed) {
    install_state->SetState(ScreenAIInstallState::State::kNotDownloaded);
  }

  // Observe component state if not already observed, otherwise trigger
  // download. (Adding observer also triggers download.)
  if (!component_ready_observer_.IsObserving()) {
    component_ready_observer_.Observe(install_state);
  } else {
    install_state->DownloadComponent();
  }
}

std::set<ScreenAIServiceRouter::Service>
ScreenAIServiceRouter::GetAllPendingStatusServices() {
  std::set<Service> services;
  for (const auto& it : pending_state_requests_) {
    services.insert(it.first);
  }
  return services;
}

void ScreenAIServiceRouter::StateChanged(ScreenAIInstallState::State state) {
  switch (state) {
    case ScreenAIInstallState::State::kNotDownloaded:
      ABSL_FALLTHROUGH_INTENDED;

    case ScreenAIInstallState::State::kDownloading:
      return;

    case ScreenAIInstallState::State::kDownloadFailed: {
      std::set<Service> all_services = GetAllPendingStatusServices();
      for (Service service : all_services) {
        CallPendingStatusRequests(service, false);
      }
      RecordComponentAvailability(false);
      break;
    }

    case ScreenAIInstallState::State::kDownloaded: {
      std::set<Service> all_services = GetAllPendingStatusServices();
      for (Service service : all_services) {
        InitializeServiceIfNeeded(service);
      }
      RecordComponentAvailability(true);
      break;
    }
  }

  // No need to observe after library is downloaded or download has failed.
  component_ready_observer_.Reset();
}

void ScreenAIServiceRouter::OnScreenAIServiceDisconnected() {
  screen_ai_service_factory_.reset();
  std::set<Service> all_services = GetAllPendingStatusServices();
  for (Service service : all_services) {
    CallPendingStatusRequests(service, false);
  }
}

void ScreenAIServiceRouter::CallPendingStatusRequests(Service service,
                                                      bool successful) {
  if (!base::Contains(pending_state_requests_, service)) {
    return;
  }

  std::vector<ServiceStateCallback> requests;
  pending_state_requests_[service].swap(requests);
  pending_state_requests_.erase(service);

  for (auto& callback : requests) {
    std::move(callback).Run(successful);
  }
}

void ScreenAIServiceRouter::BindScreenAIAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver) {
  InitializeServiceIfNeeded(Service::kOCR);

  if (ocr_service_.is_bound()) {
    ocr_service_->BindAnnotator(std::move(receiver));
  }
}

void ScreenAIServiceRouter::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver) {
  InitializeServiceIfNeeded(Service::kMainContentExtraction);

  if (main_content_extraction_service_.is_bound()) {
    main_content_extraction_service_->BindMainContentExtractor(
        std::move(receiver));
  }
}

void ScreenAIServiceRouter::LaunchIfNotRunning() {
  ScreenAIInstallState::GetInstance()->SetLastUsageTime();
  if (screen_ai_service_factory_.is_bound()) {
    return;
  }

  auto* state_instance = ScreenAIInstallState::GetInstance();

  // Callers of the service should ensure that the component is downloaded
  // before promising it to the users and triggering its launch.
  // If it is not done, the calling feature will receive no reply when it tries
  // to use this service.
  // If the below check fails, look in to the client feature that is triggering
  // the service and ensure it checks for service readiness before triggering
  // it.
  if (!state_instance->IsComponentAvailable()) {
    LOG(ERROR) << "ScreenAI service launch triggered when component is not "
                  "available.";
    screen_ai::ScreenAIInstallState::State install_state =
        state_instance->get_state();
    base::debug::Alias(&install_state);
    base::debug::DumpWithoutCrashing();
    return;
  }

  base::FilePath binary_path = state_instance->get_component_binary_path();
#if BUILDFLAG(IS_WIN)
  std::vector<base::FilePath> preload_libraries = {binary_path};
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::vector<std::string> extra_switches = {
      base::StringPrintf("--%s=%s", screen_ai::GetBinaryPathSwitch(),
                         binary_path.MaybeAsASCII().c_str())};
#endif  // BUILDFLAG(IS_WIN)

  content::ServiceProcessHost::Launch(
      screen_ai_service_factory_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Screen AI Service")
#if BUILDFLAG(IS_WIN)
          .WithPreloadedLibraries(
              preload_libraries,
              content::ServiceProcessHostPreloadLibraries::GetPassKey())
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
          .WithExtraCommandLineSwitches(extra_switches)
#endif  // BUILDFLAG(IS_WIN)
          .Pass());

  screen_ai_service_factory_.set_disconnect_handler(
      base::BindOnce(&ScreenAIServiceRouter::OnScreenAIServiceDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScreenAIServiceRouter::InitializeServiceIfNeeded(Service service) {
  std::optional<bool> service_state = GetServiceState(service);
  if (service_state) {
    // Either service is already initialized or disabled.
    CallPendingStatusRequests(service, *service_state);
    return;
  }

  base::TimeTicks request_start_time = base::TimeTicks::Now();
  LaunchIfNotRunning();

  if (!screen_ai_service_factory_.is_bound()) {
    SetLibraryLoadState(service, request_start_time, false);
    return;
  }

  switch (service) {
    case Service::kMainContentExtraction:
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::BindOnce(&ComponentFiles::Load,
                         kMainContentExtractionFilesList),
          base::BindOnce(
              &ScreenAIServiceRouter::InitializeMainContentExtraction,
              weak_ptr_factory_.GetWeakPtr(), request_start_time,
              main_content_extraction_service_.BindNewPipeAndPassReceiver()));
      main_content_extraction_service_.reset_on_disconnect();
      break;

    case Service::kOCR:
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::BindOnce(&ComponentFiles::Load, kOcrFilesList),
          base::BindOnce(&ScreenAIServiceRouter::InitializeOCR,
                         weak_ptr_factory_.GetWeakPtr(), request_start_time,
                         ocr_service_.BindNewPipeAndPassReceiver()));
      ocr_service_.reset_on_disconnect();
      break;
  }
}

void ScreenAIServiceRouter::InitializeOCR(
    base::TimeTicks request_start_time,
    mojo::PendingReceiver<mojom::OCRService> receiver,
    std::unique_ptr<ComponentFiles> component_files) {
  if (component_files->model_files_.empty() ||
      !screen_ai_service_factory_.is_bound()) {
    ScreenAIServiceRouter::SetLibraryLoadState(Service::kOCR,
                                               request_start_time, false);
    return;
  }

  CHECK(features::IsScreenAIOCREnabled());
  screen_ai_service_factory_->InitializeOCR(
      component_files->library_binary_path_,
      std::move(component_files->model_files_), std::move(receiver),
      base::BindOnce(&ScreenAIServiceRouter::SetLibraryLoadState,
                     weak_ptr_factory_.GetWeakPtr(), Service::kOCR,
                     request_start_time));
}

void ScreenAIServiceRouter::InitializeMainContentExtraction(
    base::TimeTicks request_start_time,
    mojo::PendingReceiver<mojom::MainContentExtractionService> receiver,
    std::unique_ptr<ComponentFiles> component_files) {
  if (component_files->model_files_.empty() ||
      !screen_ai_service_factory_.is_bound()) {
    ScreenAIServiceRouter::SetLibraryLoadState(Service::kMainContentExtraction,
                                               request_start_time, false);
    return;
  }

  CHECK(features::IsScreenAIMainContentExtractionEnabled());
  screen_ai_service_factory_->InitializeMainContentExtraction(
      component_files->library_binary_path_,
      std::move(component_files->model_files_), std::move(receiver),
      base::BindOnce(&ScreenAIServiceRouter::SetLibraryLoadState,
                     weak_ptr_factory_.GetWeakPtr(),
                     Service::kMainContentExtraction, request_start_time));
}

void ScreenAIServiceRouter::SetLibraryLoadState(
    Service service,
    base::TimeTicks request_start_time,
    bool successful) {
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - request_start_time;
  base::UmaHistogramBoolean("Accessibility.ScreenAI.Service.Initialization",
                            successful);
  base::UmaHistogramTimes(
      successful ? "Accessibility.ScreenAI.Service.InitializationTime.Success"
                 : "Accessibility.ScreenAI.Service.InitializationTime.Failure",
      elapsed_time);

  CallPendingStatusRequests(service, successful);

  if (successful) {
    return;
  }
  switch (service) {
    case Service::kOCR:
      ocr_service_.reset();
      break;
    case Service::kMainContentExtraction:
      main_content_extraction_service_.reset();
      break;
  }
}

bool ScreenAIServiceRouter::IsConnectionBoundForTesting(Service service) {
  switch (service) {
    case Service::kMainContentExtraction:
      return main_content_extraction_service_.is_bound();
    case Service::kOCR:
      return ocr_service_.is_bound();
  }
}

bool ScreenAIServiceRouter::IsProcessRunningForTesting() {
  return screen_ai_service_factory_.is_bound();
}

}  // namespace screen_ai
