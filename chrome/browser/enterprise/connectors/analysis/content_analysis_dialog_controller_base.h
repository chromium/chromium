// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_CONTROLLER_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_CONTROLLER_BASE_H_

#include <memory>

#include "components/enterprise/connectors/core/common.h"

namespace content {
class WebContents;
}  // namespace content

namespace enterprise_connectors {

class ContentAnalysisDelegateBase;

// Platform-agnostic interface for the UI shown while content analysis is
// pending and once a verdict is available. `ContentAnalysisDelegate` only
// interacts with the dialog through this interface so that each platform can
// provide its own implementation.
//
// Implementations own the `ContentAnalysisDelegateBase` passed to them, start
// showing any pending UI on construction, and are responsible for deleting
// themselves. Since they may be called from within `ContentAnalysisDelegate`,
// they must never delete themselves synchronously from `ShowResult()`.
class ContentAnalysisDialogControllerBase {
 public:
  // Creates the controller for the current platform. The returned controller
  // owns `delegate` and deletes itself.
  // TODO(crbug.com/428696170): Implement for Android. Until then, this is only
  // defined on desktop.
  static ContentAnalysisDialogControllerBase* Create(
      std::unique_ptr<ContentAnalysisDelegateBase> delegate,
      bool is_cloud,
      content::WebContents* web_contents,
      DeepScanAccessPoint access_point,
      int files_count,
      FinalContentAnalysisResult final_result);

  ContentAnalysisDialogControllerBase(
      const ContentAnalysisDialogControllerBase&) = delete;
  ContentAnalysisDialogControllerBase& operator=(
      const ContentAnalysisDialogControllerBase&) = delete;

  // Updates the UI with the final `result` of the analysis. The controller may
  // schedule itself for deletion if nothing else needs to be shown.
  virtual void ShowResult(FinalContentAnalysisResult result) = 0;

  // Cancels any UI and schedules the controller for deletion.
  virtual void CancelDialogAndDelete() = 0;

 protected:
  ContentAnalysisDialogControllerBase() = default;

  // Implementations delete themselves, so external code should never delete a
  // controller through this interface.
  virtual ~ContentAnalysisDialogControllerBase() = default;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_CONTROLLER_BASE_H_
