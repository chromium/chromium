// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/setters/default_browser_annotated_shell_integration_setter.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/setters/shell_integration_default_browser_setter.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble_controller_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace default_browser {

namespace {

// The bubble annotates the Windows Settings window, so only Windows has a
// controller for it. GetDefaultBrowserSetterType() never selects this setter
// elsewhere.
std::unique_ptr<AnnotatedSetterBubbleController> CreateBubbleController(
    base::TimeDelta find_timeout,
    bool can_pin_to_taskbar,
    AnnotatedSetterBubbleController::CompletionCallback on_complete) {
#if BUILDFLAG(IS_WIN)
  return AnnotatedSetterBubbleControllerWin::Create(
      find_timeout, can_pin_to_taskbar, std::move(on_complete));
#else
  NOTREACHED();
#endif  // BUILDFLAG(IS_WIN)
}

DefaultBrowserAnnotatedShellIntegrationSetter::GuidanceOutcome
ToGuidanceOutcome(AnnotatedSetterBubbleController::Outcome outcome) {
  using GuidanceOutcome =
      DefaultBrowserAnnotatedShellIntegrationSetter::GuidanceOutcome;
  using Outcome = AnnotatedSetterBubbleController::Outcome;
  switch (outcome) {
    case Outcome::kSettingsWindowClosed:
      return GuidanceOutcome::kSettingsWindowClosed;
    case Outcome::kSettingsWindowNotFound:
      return GuidanceOutcome::kSettingsWindowNotFound;
    case Outcome::kButtonNotFound:
      return GuidanceOutcome::kButtonNotFound;
    case Outcome::kDismissed:
      return GuidanceOutcome::kDismissed;
  }
  NOTREACHED();
}

}  // namespace

DefaultBrowserAnnotatedShellIntegrationSetter::
    DefaultBrowserAnnotatedShellIntegrationSetter()
    : DefaultBrowserAnnotatedShellIntegrationSetter(
          std::make_unique<ShellIntegrationDefaultBrowserSetter>(),
          base::BindRepeating(&CreateBubbleController)) {}

DefaultBrowserAnnotatedShellIntegrationSetter::
    DefaultBrowserAnnotatedShellIntegrationSetter(
        std::unique_ptr<DefaultBrowserSetter> shell_integration_setter,
        AnnotatedSetterBubbleController::Factory bubble_controller_factory)
    : shell_integration_setter_(std::move(shell_integration_setter)),
      bubble_controller_factory_(std::move(bubble_controller_factory)) {
  CHECK(shell_integration_setter_);
  CHECK(bubble_controller_factory_);
}

DefaultBrowserAnnotatedShellIntegrationSetter::
    ~DefaultBrowserAnnotatedShellIntegrationSetter() {
  EndGuidance(GuidanceOutcome::kAbandoned);
}

DefaultBrowserSetterType
DefaultBrowserAnnotatedShellIntegrationSetter::GetType() const {
  return DefaultBrowserSetterType::kAnnotatedShellIntegration;
}

void DefaultBrowserAnnotatedShellIntegrationSetter::Execute(
    DefaultBrowserSetterCompletionCallback on_complete,
    const ExecuteParams& params) {
  // A concurrent Execute() shares the running guidance.
  if (!bubble_controller_) {
    bubble_controller_ = bubble_controller_factory_.Run(
        kFindSettingsTimeout.Get(), params.can_pin_to_taskbar,
        base::BindOnce(
            &DefaultBrowserAnnotatedShellIntegrationSetter::OnGuidanceComplete,
            weak_ptr_factory_.GetWeakPtr()));
  }
  shell_integration_setter_->Execute(
      base::BindOnce(&DefaultBrowserAnnotatedShellIntegrationSetter::
                         OnShellIntegrationComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_complete)),
      params);
}

void DefaultBrowserAnnotatedShellIntegrationSetter::OnGuidanceComplete(
    AnnotatedSetterBubbleController::Outcome outcome) {
  EndGuidance(ToGuidanceOutcome(outcome));
}

void DefaultBrowserAnnotatedShellIntegrationSetter::OnShellIntegrationComplete(
    DefaultBrowserSetterCompletionCallback on_complete,
    DefaultBrowserState state) {
  EndGuidance(state == DefaultBrowserState::IS_DEFAULT
                  ? GuidanceOutcome::kFlowFinishedDefaultSet
                  : GuidanceOutcome::kFlowFinishedDefaultNotSet);
  std::move(on_complete).Run(state);
}

void DefaultBrowserAnnotatedShellIntegrationSetter::EndGuidance(
    GuidanceOutcome outcome) {
  if (!bubble_controller_) {
    return;
  }
  base::UmaHistogramEnumeration(
      "DefaultBrowser.AnnotatedShellIntegration.Outcome", outcome);
  bubble_controller_.reset();
}

}  // namespace default_browser
