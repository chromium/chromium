// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace {

// The name of the file that contains the list of files that are downloaded with
// the component and are required to initialize the library.
const base::FilePath::CharType kMainContentExtractionFilesList[] =
    FILE_PATH_LITERAL("files_list_main_content_extraction.txt");

class ComponentFiles {
 public:
  explicit ComponentFiles(const base::FilePath& library_binary_path,
                          const base::FilePath::CharType* files_list_file_name);
  ComponentFiles(const ComponentFiles&) = delete;
  ComponentFiles& operator=(const ComponentFiles&) = delete;
  ~ComponentFiles() = default;

  static std::unique_ptr<ComponentFiles> Load(
      const base::FilePath::CharType* files_list_file_name);

  base::flat_map<std::string, base::File> model_files_;
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

  // TODO(b/297824387): Consider fixing file path separators for Windows when
  // files with sub-directories are added.
  for (auto& relative_file_path : files_list) {
    const base::FilePath full_path =
#if BUILDFLAG(IS_WIN)
        component_folder.Append(base::UTF8ToWide(relative_file_path));
#else
        component_folder.Append(relative_file_path);
#endif
    model_files_[relative_file_path] =
        base::File(full_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }
}

std::unique_ptr<ComponentFiles> ComponentFiles::Load(
    const base::FilePath::CharType* files_list_file_name) {
  return std::make_unique<ComponentFiles>(
      screen_ai::ScreenAIInstallState::GetInstance()
          ->get_component_binary_path(),
      files_list_file_name);
}

}  // namespace

namespace screen_ai {

ScreenAIServiceRouter::ScreenAIServiceRouter() = default;
ScreenAIServiceRouter::~ScreenAIServiceRouter() = default;

void ScreenAIServiceRouter::BindScreenAIAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver) {
  InitializeOCRIfNeeded();

  if (ocr_service_.is_bound()) {
    ocr_service_->BindAnnotator(std::move(receiver));
  }
}

void ScreenAIServiceRouter::BindScreenAIAnnotatorClient(
    mojo::PendingRemote<mojom::ScreenAIAnnotatorClient> remote) {
  InitializeOCRIfNeeded();

  if (ocr_service_.is_bound()) {
    ocr_service_->BindAnnotatorClient(std::move(remote));
  }
}

void ScreenAIServiceRouter::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver) {
  InitializeMainContentExtractionIfNeeded();

  if (main_content_extraction_service_.is_bound()) {
    main_content_extraction_service_->BindMainContentExtractor(
        std::move(receiver));
  }
}

void ScreenAIServiceRouter::LaunchIfNotRunning() {
  ScreenAIInstallState::GetInstance()->SetLastUsageTime();

  if (screen_ai_service_factory_.is_bound() ||
      screen_ai::ScreenAIInstallState::GetInstance()->get_state() ==
          screen_ai::ScreenAIInstallState::State::kFailed) {
    return;
  }

  auto* screen_ai_install = ScreenAIInstallState::GetInstance();
  // Callers of the service should ensure that the component is downloaded
  // before promising it to the users and triggering its launch.
  // TODO(crbug.com/1443345): Add tests to cover this case.
  CHECK(screen_ai_install->IsComponentAvailable())
      << "ScreenAI service launch triggered when component is not "
         "available.";

#if BUILDFLAG(IS_WIN)
  base::FilePath library_path = screen_ai_install->get_component_binary_path();
  std::vector<base::FilePath> preload_libraries = {library_path};
#endif  // BUILDFLAG(IS_WIN)

  content::ServiceProcessHost::Launch(
      screen_ai_service_factory_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Screen AI Service")
#if BUILDFLAG(IS_WIN)
          .WithPreloadedLibraries(
              preload_libraries,
              content::ServiceProcessHostPreloadLibraries::GetPassKey())
#endif  // BUILDFLAG(IS_WIN)
          .Pass());
}

void ScreenAIServiceRouter::InitializeOCRIfNeeded() {
  if (ocr_service_.is_bound() ||
      screen_ai::ScreenAIInstallState::GetInstance()->get_state() ==
          screen_ai::ScreenAIInstallState::State::kFailed) {
    return;
  }

  LaunchIfNotRunning();

  if (!screen_ai_service_factory_.is_bound()) {
    return;
  }

  screen_ai_service_factory_->InitializeOCR(
      screen_ai::ScreenAIInstallState::GetInstance()
          ->get_component_binary_path(),
      ocr_service_.BindNewPipeAndPassReceiver(),
      base::BindOnce(&ScreenAIServiceRouter::SetLibraryLoadState,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScreenAIServiceRouter::InitializeMainContentExtractionIfNeeded() {
  if (main_content_extraction_service_.is_bound() ||
      screen_ai::ScreenAIInstallState::GetInstance()->get_state() ==
          screen_ai::ScreenAIInstallState::State::kFailed) {
    return;
  }

  LaunchIfNotRunning();

  if (!screen_ai_service_factory_.is_bound()) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ComponentFiles::Load, kMainContentExtractionFilesList),
      base::BindOnce(
          &ScreenAIServiceRouter::InitializeMainContentExtraction,
          weak_ptr_factory_.GetWeakPtr(),
          main_content_extraction_service_.BindNewPipeAndPassReceiver()));
}

void ScreenAIServiceRouter::InitializeMainContentExtraction(
    mojo::PendingReceiver<mojom::MainContentExtractionService> receiver,
    std::unique_ptr<ComponentFiles> component_files) {
  if (component_files->model_files_.empty()) {
    ScreenAIServiceRouter::SetLibraryLoadState(false);
    return;
  }

  screen_ai_service_factory_->InitializeMainContentExtraction(
      component_files->library_binary_path_,
      std::move(component_files->model_files_), std::move(receiver),
      base::BindOnce(&ScreenAIServiceRouter::SetLibraryLoadState,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScreenAIServiceRouter::SetLibraryLoadState(bool successful) {
  screen_ai::ScreenAIInstallState::GetInstance()->SetState(
      successful ? screen_ai::ScreenAIInstallState::State::kReady
                 : screen_ai::ScreenAIInstallState::State::kFailed);
}

}  // namespace screen_ai
