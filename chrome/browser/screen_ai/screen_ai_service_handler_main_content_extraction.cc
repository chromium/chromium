// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/screen_ai/screen_ai_service_handler_base.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
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
// The name of the file that contains the list of files that are downloaded with
// the component and are required to initialize the library.
const base::FilePath::CharType kMainContentExtractionFilesList[] =
    FILE_PATH_LITERAL("files_list_main_content_extraction.txt");
}  // namespace

namespace screen_ai {

ScreenAIServiceHandlerMainContentExtraction::
    ScreenAIServiceHandlerMainContentExtraction() = default;
ScreenAIServiceHandlerMainContentExtraction::
    ~ScreenAIServiceHandlerMainContentExtraction() = default;

std::string ScreenAIServiceHandlerMainContentExtraction::GetServiceName()
    const {
  return "MainContentExtraction";
}

bool ScreenAIServiceHandlerMainContentExtraction::IsConnectionBound() const {
  return service_.is_bound();
}
bool ScreenAIServiceHandlerMainContentExtraction::IsServiceEnabled() const {
  return features::IsScreenAIMainContentExtractionEnabled();
}
void ScreenAIServiceHandlerMainContentExtraction::ResetConnection() {
  service_.reset();
}
void ScreenAIServiceHandlerMainContentExtraction::BindService(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver) {
  InitializeServiceIfNeeded();

  if (service_.is_bound()) {
    service_->BindMainContentExtractor(std::move(receiver));
  }
}

void ScreenAIServiceHandlerMainContentExtraction::LoadModelFilesAndInitialize(
    base::TimeTicks request_start_time) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ComponentFiles::Load, kMainContentExtractionFilesList),
      base::BindOnce(
          &ScreenAIServiceHandlerMainContentExtraction::InitializeService,
          weak_ptr_factory_.GetWeakPtr(), request_start_time,
          service_.BindNewPipeAndPassReceiver()));
  service_.reset_on_disconnect();
}

void ScreenAIServiceHandlerMainContentExtraction::InitializeService(
    base::TimeTicks request_start_time,
    mojo::PendingReceiver<mojom::MainContentExtractionService> receiver,
    std::unique_ptr<ComponentFiles> component_files) {
  if (component_files->model_files_.empty() ||
      !screen_ai_service_factory().is_bound()) {
    ScreenAIServiceHandlerMainContentExtraction::SetLibraryLoadState(
        request_start_time, false);
    return;
  }

  CHECK(features::IsScreenAIMainContentExtractionEnabled());
  screen_ai_service_factory()->InitializeMainContentExtraction(
      component_files->library_binary_path_,
      std::move(component_files->model_files_), std::move(receiver),
      base::BindOnce(
          &ScreenAIServiceHandlerMainContentExtraction::SetLibraryLoadState,
          weak_ptr_factory_.GetWeakPtr(), request_start_time));
}

}  // namespace screen_ai
