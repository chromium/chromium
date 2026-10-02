// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_CONTROLLER_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DIALOG_CONTROLLER_BASE_H_

#include "components/enterprise/connectors/core/common.h"

namespace enterprise_connectors {

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
