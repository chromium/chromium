// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_TAB_CONTEXT_CAPTURE_REQUEST_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_TAB_CONTEXT_CAPTURE_REQUEST_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"

namespace lens {
class TabContextualizationController;
struct ContextualInputData;
}  // namespace lens

namespace tabs {
class TabInterface;
}  // namespace tabs

// Helper class to defer capturing the tab context until it is ready.
class TabContextCaptureRequest : content::WebContentsObserver {
 public:
  TabContextCaptureRequest(
      lens::TabContextualizationController* tab_contextualization_controller,
      tabs::TabInterface* tab,
      base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
          callback);
  ~TabContextCaptureRequest() override;

  TabContextCaptureRequest(const TabContextCaptureRequest&) = delete;
  TabContextCaptureRequest& operator=(const TabContextCaptureRequest&) = delete;

  // Starts the capture process. Capture is certain to trigger within a
  // reasonable time limit (< 1 minute).
  void Start();

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void WebContentsDestroyed() override;

 private:
  void ScheduleCapture(const base::TimeDelta& delay);
  void UnableToCapture();
  void TriggerCapture();
  void DeleteSoon();

  // The current scheduled capture, if any.
  base::CancelableOnceClosure scheduled_capture_;

  // This is assumed to exist so long as `weak_tab_` is valid.
  raw_ptr<lens::TabContextualizationController>
      tab_contextualization_controller_;

  // Used to guard against trying to capture after the tab is gone.
  base::WeakPtr<tabs::TabInterface> weak_tab_;

  // The presence of this callback guards against repeated captures.
  base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
      callback_;

  base::WeakPtrFactory<TabContextCaptureRequest> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_TAB_CONTEXT_CAPTURE_REQUEST_H_
