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
// result. Windows 11 only; see GetDefaultBrowserSetterType().
class DefaultBrowserAnnotatedShellIntegrationSetter
    : public DefaultBrowserSetter {
 public:
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

  const std::unique_ptr<DefaultBrowserSetter> shell_integration_setter_;
  const AnnotatedSetterBubbleController::Factory bubble_controller_factory_;
  std::unique_ptr<AnnotatedSetterBubbleController> bubble_controller_;

  base::WeakPtrFactory<DefaultBrowserAnnotatedShellIntegrationSetter>
      weak_ptr_factory_{this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_DEFAULT_BROWSER_SETTERS_DEFAULT_BROWSER_ANNOTATED_SHELL_INTEGRATION_SETTER_H_
