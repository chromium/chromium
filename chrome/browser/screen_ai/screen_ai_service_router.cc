// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
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
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"
#include "mojo/public/mojom/base/file_path.mojom.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace {

// Maximum time to wait for service initialization.
// TODO(crbug.com/40947650): Update based on collected metrics.
constexpr base::TimeDelta kInitializationTimeout = base::Seconds(10);

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
  ~ComponentFiles() = default;

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
    if (!model_files_[relative_path].IsValid()) {
      VLOG(0) << "Could not open " << full_path;
      model_files_.clear();
      return;
    }
  }
}

std::unique_ptr<ComponentFiles> ComponentFiles::Load(
    const base::FilePath::CharType* files_list_file_name) {
  return std::make_unique<ComponentFiles>(
      screen_ai::ScreenAIInstallState::GetInstance()
          ->get_component_binary_path(),
      files_list_file_name);
}

void RecordComponentAvailablity(bool available) {
  base::UmaHistogramBoolean("Accessibility.ScreenAI.Component.Available",
                            available);
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
    RecordComponentAvailablity(true);
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
      RecordComponentAvailablity(false);
      break;
    }

    case ScreenAIInstallState::State::kDownloaded: {
      std::set<Service> all_services = GetAllPendingStatusServices();
      for (Service service : all_services) {
        InitializeServiceIfNeeded(service);
      }
      RecordComponentAvailablity(true);
      break;
    }
  }

  // No need to observe after library is downloaded or download has failed.
  component_ready_observer_.Reset();
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

void ScreenAIServiceRouter::BindScreenAIAnnotatorClient(
    mojo::PendingRemote<mojom::ScreenAIAnnotatorClient> remote) {
  InitializeServiceIfNeeded(Service::kOCR);

  if (ocr_service_.is_bound()) {
    ocr_service_->BindAnnotatorClient(std::move(remote));
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
  auto* state_instance = ScreenAIInstallState::GetInstance();
  screen_ai::ScreenAIInstallState::State install_state =
      state_instance->get_state();

  if (screen_ai_service_factory_.is_bound()) {
    return;
  }

  // TODO(crbug.com/41493789): Remove when Reading Mode does not enable OCR
  // without checking component availability.
  if (install_state != screen_ai::ScreenAIInstallState::State::kDownloaded) {
    return;
  }

  // Callers of the service should ensure that the component is downloaded
  // before promising it to the users and triggering its launch.
  CHECK(state_instance->IsComponentAvailable())
      << "ScreenAI service launch triggered when component is not "
         "available.";

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
}

void ScreenAIServiceRouter::InitializeServiceIfNeeded(Service service) {
  std::optional<bool> service_state = GetServiceState(service);
  if (service_state) {
    // Either service is already initialized or disabled.
    CallPendingStatusRequests(service, *service_state);
    return;
  }

  int request_id = CreateRequestIdAndSetTimeOut(service);
  LaunchIfNotRunning();

  if (!screen_ai_service_factory_.is_bound()) {
    SetLibraryLoadState(request_id, false);
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
              weak_ptr_factory_.GetWeakPtr(), request_id,
              main_content_extraction_service_.BindNewPipeAndPassReceiver()));
      break;

    case Service::kOCR:
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
          base::BindOnce(&ComponentFiles::Load, kOcrFilesList),
          base::BindOnce(&ScreenAIServiceRouter::InitializeOCR,
                         weak_ptr_factory_.GetWeakPtr(), request_id,
                         ocr_service_.BindNewPipeAndPassReceiver()));
      break;
  }
}

void ScreenAIServiceRouter::InitializeOCR(
    int request_id,
    mojo::PendingReceiver<mojom::OCRService> receiver,
    std::unique_ptr<ComponentFiles> component_files) {
  if (component_files->model_files_.empty()) {
    ScreenAIServiceRouter::SetLibraryLoadState(request_id, false);
    return;
  }

  CHECK(features::IsScreenAIOCREnabled());
  screen_ai_service_factory_->InitializeOCR(
      component_files->library_binary_path_,
      std::move(component_files->model_files_), std::move(receiver),
      base::BindOnce(&ScreenAIServiceRouter::SetLibraryLoadState,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void ScreenAIServiceRouter::InitializeMainContentExtraction(
    int request_id,
    mojo::PendingReceiver<mojom::MainContentExtractionService> receiver,
    std::unique_ptr<ComponentFiles> component_files) {
  if (component_files->model_files_.empty()) {
    ScreenAIServiceRouter::SetLibraryLoadState(request_id, false);
    return;
  }

  CHECK(features::IsScreenAIMainContentExtractionEnabled());
  screen_ai_service_factory_->InitializeMainContentExtraction(
      component_files->library_binary_path_,
      std::move(component_files->model_files_), std::move(receiver),
      base::BindOnce(&ScreenAIServiceRouter::SetLibraryLoadState,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

int ScreenAIServiceRouter::CreateRequestIdAndSetTimeOut(Service service) {
  int request_id = ++last_request_id_;
  pending_initialization_requests_[request_id] =
      std::make_pair(service, base::TimeTicks::Now());

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScreenAIServiceRouter::SetLibraryLoadState,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     /*successful=*/false),
      kInitializationTimeout);

  return request_id;
}

void ScreenAIServiceRouter::SetLibraryLoadState(int request_id,
                                                bool successful) {
  // Verify that `request_id` is not handled before. This function can be called
  // by the initialization callback or the timeout task.
  auto request = pending_initialization_requests_.find(request_id);
  if (request == pending_initialization_requests_.end()) {
    return;
  }

  Service service = request->second.first;
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - request->second.second;
  pending_initialization_requests_.erase(request);

  base::UmaHistogramBoolean("Accessibility.ScreenAI.Service.Initialization",
                            successful);
  base::UmaHistogramTimes(
      successful ? "Accessibility.ScreenAI.Service.InitializationTime.Success"
                 : "Accessibility.ScreenAI.Service.InitializationTime.Failure",
      elapsed_time);

  CallPendingStatusRequests(service, successful);
}

bool ScreenAIServiceRouter::IsConnectionBoundForTesting(Service service) {
  switch (service) {
    case Service::kMainContentExtraction:
      return main_content_extraction_service_.is_bound();
    case Service::kOCR:
      return ocr_service_.is_bound();
  }
}

}  // namespace screen_ai
