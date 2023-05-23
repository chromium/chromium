// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/service_process_host_passkeys.h"

namespace {

// TODO(https://crbug.com/1443341): Move file names into a shared constants
// file before adding more files.
class ComponentModelFiles {
 public:
  explicit ComponentModelFiles(const base::FilePath& library_binary_path);
  ComponentModelFiles(const ComponentModelFiles&) = delete;
  ComponentModelFiles& operator=(const ComponentModelFiles&) = delete;
  ~ComponentModelFiles() = default;

  static std::unique_ptr<ComponentModelFiles> LoadComponentFiles();

  base::FilePath library_binary_path_;
  base::File screen2x_model_config_;
  base::File screen2x_model_;
};

ComponentModelFiles::ComponentModelFiles(
    const base::FilePath& library_binary_path)
    : library_binary_path_(library_binary_path),
      screen2x_model_config_(library_binary_path.DirName().Append(
                                 FILE_PATH_LITERAL("screen2x_config.pbtxt")),
                             base::File::FLAG_OPEN | base::File::FLAG_READ),
      screen2x_model_(library_binary_path.DirName().Append(
                          FILE_PATH_LITERAL("screen2x_model.tflite")),
                      base::File::FLAG_OPEN | base::File::FLAG_READ) {}

std::unique_ptr<ComponentModelFiles> ComponentModelFiles::LoadComponentFiles() {
  return std::make_unique<ComponentModelFiles>(
      screen_ai::ScreenAIInstallState::GetInstance()
          ->get_component_binary_path());
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
  if (!screen_ai_install->IsComponentAvailable()) {
    // TODO(crbug.com/1278249): Consider creating the pipe now and binding the
    // other end when component is downloaded. The main challenge is that at
    // this point we don't know the component folder for opening the binary and
    // model files during library initialization.
    VLOG(0) << "ScreenAI service launch triggered when component is not "
               "available.";
    return;
  }
#if BUILDFLAG(IS_WIN)
  base::FilePath library_path = screen_ai_install->get_component_binary_path();
  std::vector<base::FilePath> preload_libraries = {library_path};
#endif  // BUILDFLAG(IS_WIN)

  // TODO(https://crbug.com/1443341): Make sure the library is sandboxed and
  // loaded from the same folder and component updater doesn't download a new
  // version during sandbox creation.
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
      base::BindOnce(&ComponentModelFiles::LoadComponentFiles),
      base::BindOnce(
          &ScreenAIServiceRouter::InitializeMainContentExtraction,
          weak_ptr_factory_.GetWeakPtr(),
          main_content_extraction_service_.BindNewPipeAndPassReceiver()));
}

void ScreenAIServiceRouter::InitializeMainContentExtraction(
    mojo::PendingReceiver<mojom::MainContentExtractionService> receiver,
    std::unique_ptr<ComponentModelFiles> model_files) {
  screen_ai_service_factory_->InitializeMainContentExtraction(
      std::move(model_files->screen2x_model_config_),
      std::move(model_files->screen2x_model_),
      model_files->library_binary_path_, std::move(receiver),
      base::BindOnce(&ScreenAIServiceRouter::SetLibraryLoadState,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScreenAIServiceRouter::SetLibraryLoadState(bool successful) {
  // TODO(crbug.com/1278249): Update so that "Ready" state would be kept
  // separately for OCR and MainContentExtraction services.
  screen_ai::ScreenAIInstallState::GetInstance()->SetState(
      successful ? screen_ai::ScreenAIInstallState::State::kReady
                 : screen_ai::ScreenAIInstallState::State::kFailed);
}

}  // namespace screen_ai
