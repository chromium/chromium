// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/setters/default_browser_annotated_shell_integration_setter.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/setters/shell_integration_default_browser_setter.h"
#include "chrome/browser/ui/views/default_browser/annotated_setter_bubble_controller_win.h"

namespace default_browser {

DefaultBrowserAnnotatedShellIntegrationSetter::
    DefaultBrowserAnnotatedShellIntegrationSetter()
    : DefaultBrowserAnnotatedShellIntegrationSetter(
          std::make_unique<ShellIntegrationDefaultBrowserSetter>(),
          base::BindRepeating(&AnnotatedSetterBubbleControllerWin::Create)) {}

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
    ~DefaultBrowserAnnotatedShellIntegrationSetter() = default;

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
  bubble_controller_.reset();
}

void DefaultBrowserAnnotatedShellIntegrationSetter::OnShellIntegrationComplete(
    DefaultBrowserSetterCompletionCallback on_complete,
    DefaultBrowserState state) {
  bubble_controller_.reset();
  std::move(on_complete).Run(state);
}

}  // namespace default_browser
