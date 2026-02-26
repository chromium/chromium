// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/tab_context_capture_request.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "components/lens/contextual_input.h"
#include "components/pdf/common/constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

TabContextCaptureRequest::TabContextCaptureRequest(
    lens::TabContextualizationController* tab_contextualization_controller,
    tabs::TabInterface* tab,
    base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
        callback)
    : content::WebContentsObserver(tab->GetContents()),
      scheduled_capture_(base::DoNothing()),
      tab_contextualization_controller_(tab_contextualization_controller),
      weak_tab_(tab->GetWeakPtr()),
      callback_(std::move(callback)) {}

TabContextCaptureRequest::~TabContextCaptureRequest() = default;

void TabContextCaptureRequest::Start() {
  // There is no delay if the document is already loaded. It may not have done
  // a paint yet; however, there is no guarantee that signal will ever arrive
  // and there is no easy way to check. If the page is loaded it is better to
  // just try to capture immediately than delaying to the maximum delay.
  if (web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame() ||
      web_contents()->GetContentsMimeType() == pdf::kPDFMimeType ||
      !base::FeatureList::IsEnabled(
          chrome::android::kOnDemandBackgroundTabContextCapture)) {
    TriggerCapture();
  } else {
    // Ensure capture always triggers within a reasonable time even if the
    // page load never completes.
    ScheduleCapture(base::Seconds(30));
  }
}

void TabContextCaptureRequest::DocumentOnLoadCompletedInPrimaryMainFrame() {
  // Allow a short time for the page to paint after loading.
  ScheduleCapture(base::Seconds(5));
}

void TabContextCaptureRequest::WebContentsDestroyed() {
  UnableToCapture();
}

void TabContextCaptureRequest::ScheduleCapture(const base::TimeDelta& delay) {
  scheduled_capture_.Reset(
      base::BindOnce(&TabContextCaptureRequest::TriggerCapture,
                     weak_ptr_factory_.GetWeakPtr()));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, scheduled_capture_.callback(), delay);
}

void TabContextCaptureRequest::UnableToCapture() {
  scheduled_capture_.Cancel();
  if (callback_) {
    std::move(callback_).Run(nullptr);
    DeleteSoon();
  }
  // else there is no callback and DeleteSoon will already have been called.
}

void TabContextCaptureRequest::TriggerCapture() {
  if (!callback_) {
    // The callback was already invoked we are done.
    return;
  }
  if (!weak_tab_) {
    UnableToCapture();
    return;
  }
  tab_contextualization_controller_->GetPageContext(std::move(callback_));
  DeleteSoon();
}

void TabContextCaptureRequest::DeleteSoon() {
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
}
