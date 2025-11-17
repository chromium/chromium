// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/default_browser/default_browser_setter.h"

namespace default_browser {

DefaultBrowserController::DefaultBrowserController(
    std::unique_ptr<DefaultBrowserSetter> setter,
    DefaultBrowserEntrypointType ui_entrypoint)
    : setter_(std::move(setter)) {}

DefaultBrowserController::~DefaultBrowserController() = default;

DefaultBrowserSetterType DefaultBrowserController::GetSetterType() const {
  return setter_->GetType();
}

void DefaultBrowserController::OnShown() {
  // TODO(crbug.com/460618680): Record metrics.
}

void DefaultBrowserController::OnAccepted(
    DefaultBrowserControllerCompletionCallback completion_callback) {
  // TODO(crbug.com/460618680): Record metrics.
  completion_callback_ = std::move(completion_callback);
  setter_->Execute(
      base::BindOnce(&DefaultBrowserController::OnSetterExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DefaultBrowserController::OnIgnored() {
  // TODO(crbug.com/460618680): Record metrics.
}

void DefaultBrowserController::OnDismissed() {
  // TODO(crbug.com/460618680): Record metrics.
}

void DefaultBrowserController::OnSetterExecutionComplete(
    DefaultBrowserState default_browser_state) {
  // TODO(crbug.com/460618680): Record metrics.
  std::move(completion_callback_).Run(default_browser_state);
}

}  // namespace default_browser
