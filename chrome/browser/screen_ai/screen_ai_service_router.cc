// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_service_router.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
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

// static
// LINT.IfChange(SuggestedWaitTimeBeforeReAttempt)
base::TimeDelta ScreenAIServiceRouter::SuggestedWaitTimeBeforeReAttempt(
    uint32_t reattempt_number) {
  return base::Minutes(reattempt_number * reattempt_number);
}
// LINT.ThenChange(//chrome/browser/ash/app_list/search/local_image_search/image_annotation_worker.cc:SuggestedWaitTimeBeforeReAttempt)

ScreenAIServiceHandlerBase* ScreenAIServiceRouter::GetHandler(Service service) {
  switch (service) {
    case Service::kMainContentExtraction:
      if (!mce_handler_) {
        mce_handler_ =
            std::make_unique<ScreenAIServiceHandlerMainContentExtraction>();
      }
      return mce_handler_.get();

    case Service::kOCR:
      if (!ocr_handler_) {
        ocr_handler_ = std::make_unique<ScreenAIServiceHandlerOCR>();
      }
      return ocr_handler_.get();
  }
}

void ScreenAIServiceRouter::GetServiceStateAsync(
    Service service,
    ServiceStateCallback callback) {
  std::optional<bool> service_state =
      GetHandler(service)->GetServiceStateAsync(std::move(callback));

  // If `service_state` has value, either the service is already initialized or
  // disabled. In both cases we can can assume the component was ready.
  // Otherwise its download should be triggered.
  if (service_state) {
    RecordComponentAvailability(true);
    return;
  }

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

void ScreenAIServiceRouter::StateChanged(ScreenAIInstallState::State state) {
  bool available = true;
  switch (state) {
    case ScreenAIInstallState::State::kNotDownloaded:
    case ScreenAIInstallState::State::kDownloading:
      return;

    case ScreenAIInstallState::State::kDownloadFailed:
      available = false;
      ABSL_FALLTHROUGH_INTENDED;
    case ScreenAIInstallState::State::kDownloaded:
      if (mce_handler_) {
        mce_handler_->OnLibraryAvailablityChanged(available);
      }
      if (ocr_handler_) {
        ocr_handler_->OnLibraryAvailablityChanged(available);
      }
      RecordComponentAvailability(available);
      break;
  }

  // No need to observe after library is downloaded or download has failed.
  component_ready_observer_.Reset();
}

void ScreenAIServiceRouter::BindScreenAIAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> receiver) {
  // Ensure handler exists.
  GetHandler(Service::kOCR);
  ocr_handler_->BindService(std::move(receiver));
}

void ScreenAIServiceRouter::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor> receiver) {
  // Ensure handler exists.
  GetHandler(Service::kMainContentExtraction);
  mce_handler_->BindService(std::move(receiver));
}

bool ScreenAIServiceRouter::IsConnectionBoundForTesting(Service service) {
  switch (service) {
    case Service::kMainContentExtraction:
      return mce_handler_ &&
             mce_handler_->IsConnectionBoundForTesting();  // IN-TEST
    case Service::kOCR:
      return ocr_handler_ &&
             ocr_handler_->IsConnectionBoundForTesting();  // IN-TEST
  }
}

bool ScreenAIServiceRouter::IsProcessRunningForTesting(Service service) {
  switch (service) {
    case Service::kMainContentExtraction:
      return mce_handler_ &&
             mce_handler_->IsProcessRunningForTesting();  // IN-TEST
    case Service::kOCR:
      return ocr_handler_ &&
             ocr_handler_->IsProcessRunningForTesting();  // IN-TEST
  }
}

}  // namespace screen_ai
