// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_ANNOTATED_SHELL_INTEGRATION_SETTER_H_
#define CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_ANNOTATED_SHELL_INTEGRATION_SETTER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/default_browser/default_browser_setter.h"
#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble_controller.h"

namespace default_browser {

// Runs the shell integration flow and, while it runs, points a bubble at the
// "Set default" button of the Settings window it opens. Reports the flow's
// result. Only selected on Windows 11, where the bubble exists; see
// GetDefaultBrowserSetterType().
class DefaultBrowserAnnotatedShellIntegrationSetter
    : public DefaultBrowserSetter {
 public:
  // Outcomes for the UMA histogram
  // "DefaultBrowser.AnnotatedShellIntegration.Outcome", recorded once per
  // guidance session. The flow's own result is recorded by
  // DefaultBrowserController.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(DefaultBrowserAnnotatedShellIntegrationOutcome)
  enum class GuidanceOutcome {
    // The shell integration flow finished while the guidance was running.
    kFlowFinishedDefaultSet = 0,
    kFlowFinishedDefaultNotSet = 1,
    // The guidance ended on its own; see
    // AnnotatedSetterBubbleController::Outcome.
    kSettingsWindowClosed = 2,
    kSettingsWindowNotFound = 3,
    kButtonNotFound = 4,
    kDismissed = 5,
    // The setter was destroyed while the guidance was running.
    kAbandoned = 6,
    kMaxValue = kAbandoned,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/ui/enums.xml:DefaultBrowserAnnotatedShellIntegrationOutcome)

  // The bubble only exists on Windows, so a setter created this way must not
  // be executed elsewhere.
  DefaultBrowserAnnotatedShellIntegrationSetter();
  // For testing.
  DefaultBrowserAnnotatedShellIntegrationSetter(
      std::unique_ptr<DefaultBrowserSetter> shell_integration_setter,
      AnnotatedSetterBubbleController::Factory bubble_controller_factory);

  DefaultBrowserAnnotatedShellIntegrationSetter(
      const DefaultBrowserAnnotatedShellIntegrationSetter&) = delete;
  DefaultBrowserAnnotatedShellIntegrationSetter& operator=(
      const DefaultBrowserAnnotatedShellIntegrationSetter&) = delete;

  ~DefaultBrowserAnnotatedShellIntegrationSetter() override;

  // DefaultBrowserSetter:
  DefaultBrowserSetterType GetType() const override;
  void Execute(DefaultBrowserSetterCompletionCallback on_complete,
               const ExecuteParams& params) override;

 private:
  void OnGuidanceComplete(AnnotatedSetterBubbleController::Outcome outcome);
  void OnShellIntegrationComplete(
      DefaultBrowserSetterCompletionCallback on_complete,
      DefaultBrowserState state);
  // Records `outcome` and stops the guidance, if it is running.
  void EndGuidance(GuidanceOutcome outcome);

  const std::unique_ptr<DefaultBrowserSetter> shell_integration_setter_;
  const AnnotatedSetterBubbleController::Factory bubble_controller_factory_;
  std::unique_ptr<AnnotatedSetterBubbleController> bubble_controller_;

  base::WeakPtrFactory<DefaultBrowserAnnotatedShellIntegrationSetter>
      weak_ptr_factory_{this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_ANNOTATED_SHELL_INTEGRATION_SETTER_H_
