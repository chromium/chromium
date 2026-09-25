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
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
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
  if (base::FeatureList::IsEnabled(
          chrome::android::kOnDemandBackgroundTabContextCaptureOptimization)) {
    int overall_timeout =
        chrome::android::
            kOnDemandBackgroundTabContextCaptureOverallTimeoutSeconds.Get();
    if (overall_timeout > 0) {
      overall_timeout_timer_.Start(
          FROM_HERE, base::Seconds(overall_timeout),
          base::BindOnce(&TabContextCaptureRequest::UnableToCapture,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // There is no delay if the document is already loaded, or if the tab is the
  // currently active/foreground tab and the optimization variation is enabled.
  // It may not have done a paint yet; however, there is no guarantee that
  // signal will ever arrive and there is no easy way to check. If the page is
  // loaded (or active) it is better to just try to capture immediately than
  // delaying to the maximum delay.
  if (ShouldSkipDelayForActiveTab() ||
      web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame() ||
      IsBackgroundTabDomContentLoaded() ||
      web_contents()->GetContentsMimeType() == pdf::kPDFMimeType ||
      !base::FeatureList::IsEnabled(
          chrome::android::kOnDemandBackgroundTabContextCapture)) {
    TriggerCapture();
  } else {
    base::TimeDelta fallback_delay = base::Seconds(30);
    if (base::FeatureList::IsEnabled(
            chrome::android::
                kOnDemandBackgroundTabContextCaptureOptimization)) {
      fallback_delay = base::Seconds(
          chrome::android::
              kOnDemandBackgroundTabContextCaptureInitialFallbackDelaySeconds
                  .Get());
    }
    // Ensure capture always triggers within a reasonable time even if the
    // page load never completes.
    ScheduleCapture(fallback_delay);
  }
}

void TabContextCaptureRequest::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame() ||
      !ShouldUseDomContentLoadedForBackgroundTab()) {
    return;
  }
  // Annotated page content is extracted from the DOM rather than from painted
  // pixels, so the parsed document is sufficient. Waiting for onload on
  // subresource-heavy pages costs seconds for no additional content.
  TriggerCapture();
}

void TabContextCaptureRequest::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (ShouldSkipDelayForActiveTab() ||
      ShouldUseDomContentLoadedForBackgroundTab()) {
    TriggerCapture();
    return;
  }
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
  overall_timeout_timer_.Stop();
  if (callback_) {
    std::move(callback_).Run(nullptr);
    DeleteSoon();
  }
  // else there is no callback and DeleteSoon will already have been called.
}

void TabContextCaptureRequest::TriggerCapture() {
  if (is_capturing_ || !callback_) {
    // Already capturing or the callback was already invoked, we are done.
    return;
  }
  scheduled_capture_.Cancel();
  is_capturing_ = true;
  // Stop observing once capture is triggered to avoid receiving redundant
  // load completion events while capture is in flight.
  Observe(nullptr);

  if (!weak_tab_) {
    UnableToCapture();
    return;
  }
  tab_contextualization_controller_->GetPageContext(
      base::BindOnce(&TabContextCaptureRequest::OnPageContextRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TabContextCaptureRequest::OnPageContextRetrieved(
    std::unique_ptr<lens::ContextualInputData> data) {
  overall_timeout_timer_.Stop();
  if (callback_) {
    std::move(callback_).Run(std::move(data));
    DeleteSoon();
  }
}

void TabContextCaptureRequest::DeleteSoon() {
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
}

bool TabContextCaptureRequest::ShouldSkipDelayForActiveTab() const {
  if (!weak_tab_ || !weak_tab_->IsActivated()) {
    return false;
  }
  return base::FeatureList::IsEnabled(
             chrome::android::
                 kOnDemandBackgroundTabContextCaptureOptimization) &&
         chrome::android::
             kOnDemandBackgroundTabContextCaptureSkipDelayForActiveTab.Get();
}

bool TabContextCaptureRequest::ShouldUseDomContentLoadedForBackgroundTab()
    const {
  // The active tab is handled by ShouldSkipDelayForActiveTab(), which captures
  // even earlier because its document is already live.
  if (!weak_tab_ || weak_tab_->IsActivated()) {
    return false;
  }
  return base::FeatureList::IsEnabled(
             chrome::android::
                 kOnDemandBackgroundTabContextCaptureOptimization) &&
         chrome::android::
             kOnDemandBackgroundTabContextCaptureBackgroundTabUseDomContentLoaded
                 .Get();
}

bool TabContextCaptureRequest::IsBackgroundTabDomContentLoaded() const {
  // The picker starts loading a tab when it is selected rather than when
  // "Done" is tapped, so DOMContentLoaded has often already fired by the time
  // this request starts observing.
  if (!ShouldUseDomContentLoadedForBackgroundTab()) {
    return false;
  }
  // The committed document's DOMContentLoaded is stale if it is about to be
  // replaced, e.g. a load cancelled between DOMContentLoaded and onload that
  // was marked for reload.
  if (web_contents()->HasUncommittedNavigationInPrimaryMainFrame() ||
      web_contents()->GetController().NeedsReload()) {
    return false;
  }
  return web_contents()->GetPrimaryMainFrame()->IsDOMContentLoaded();
}
