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
const base::FilePath::CharType kOcrFilesList[] =
    FILE_PATH_LITERAL("files_list_ocr.txt");
}  // namespace

namespace screen_ai {

ScreenAIServiceHandlerOCR::ScreenAIServiceHandlerOCR() = default;
ScreenAIServiceHandlerOCR::~ScreenAIServiceHandlerOCR() = default;

std::string ScreenAIServiceHandlerOCR::GetServiceName() const {
  return "OCR";
}

bool ScreenAIServiceHandlerOCR::IsConnectionBound() const {
  return service_.is_bound();
}
bool ScreenAIServiceHandlerOCR::IsServiceEnabled() const {
  return features::IsScreenAIOCREnabled();
}
void ScreenAIServiceHandlerOCR::ResetConnection() {
  service_.reset();
}

void ScreenAIServiceHandlerOCR::OnDisconnected(bool crashed) {
  std::string prefix = GetMetricFullName("MemoryBefore.");
  prefix += crashed ? "Crash." : "Shutdown.";
  if (memory_stats_before_launch_.pressure_available) {
    base::UmaHistogramEnumeration(prefix + "Pressure",
                                  memory_stats_before_launch_.pressure_level);
  }
  base::UmaHistogramCounts100000(prefix + "Total",
                                 memory_stats_before_launch_.total_memory);
  base::UmaHistogramCounts100000(prefix + "Available",
                                 memory_stats_before_launch_.available_memory);
}

void ScreenAIServiceHandlerOCR::BindService(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver) {
  InitializeServiceIfNeeded();

  if (service_.is_bound()) {
    service_->BindAnnotator(std::move(receiver));
  }
}

void ScreenAIServiceHandlerOCR::PerformPrelaunchSteps() {
  // Keep memory stats for metrics after shutdown or crash.
  memory_stats_before_launch_.total_memory =
      base::SysInfo::AmountOfPhysicalMemoryMB();
  memory_stats_before_launch_.available_memory = static_cast<int>(
      base::SysInfo::AmountOfAvailablePhysicalMemory() / (1024 * 1024));

  const auto* const memory_monitor = base::MemoryPressureMonitor::Get();
  if (memory_monitor) {
    memory_stats_before_launch_.pressure_available = true;
    memory_stats_before_launch_.pressure_level =
        memory_monitor->GetCurrentPressureLevel();
  } else {
    memory_stats_before_launch_.pressure_available = false;
  }
}

void ScreenAIServiceHandlerOCR::LoadModelFilesAndInitialize(
    base::TimeTicks request_start_time) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ComponentFiles::Load, kOcrFilesList),
      base::BindOnce(&ScreenAIServiceHandlerOCR::InitializeService,
                     weak_ptr_factory_.GetWeakPtr(), request_start_time,
                     service_.BindNewPipeAndPassReceiver()));
  service_.reset_on_disconnect();
}

void ScreenAIServiceHandlerOCR::InitializeService(
    base::TimeTicks request_start_time,
    mojo::PendingReceiver<mojom::OCRService> receiver,
    std::unique_ptr<ComponentFiles> component_files) {
  if (component_files->model_files_.empty() ||
      !screen_ai_service_factory().is_bound()) {
    ScreenAIServiceHandlerOCR::SetLibraryLoadState(request_start_time, false);
    return;
  }

  CHECK(features::IsScreenAIOCREnabled());
  screen_ai_service_factory()->InitializeOCR(
      component_files->library_binary_path_,
      std::move(component_files->model_files_), std::move(receiver),
      base::BindOnce(&ScreenAIServiceHandlerOCR::SetLibraryLoadState,
                     weak_ptr_factory_.GetWeakPtr(), request_start_time));
}

}  // namespace screen_ai
