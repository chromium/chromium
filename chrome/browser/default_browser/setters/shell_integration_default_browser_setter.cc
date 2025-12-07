// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/setters/shell_integration_default_browser_setter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/shell_integration.h"

namespace default_browser {

ShellIntegrationDefaultBrowserSetter::ShellIntegrationDefaultBrowserSetter() =
    default;
ShellIntegrationDefaultBrowserSetter::~ShellIntegrationDefaultBrowserSetter() =
    default;

DefaultBrowserSetterType ShellIntegrationDefaultBrowserSetter::GetType() const {
  return DefaultBrowserSetterType::kShellIntegration;
}

void ShellIntegrationDefaultBrowserSetter::Execute(
    DefaultBrowserSetterCompletionCallback on_complete) {
  on_complete_callback_ = std::move(on_complete);
  worker_ = base::MakeRefCounted<shell_integration::DefaultBrowserWorker>();

  worker_->StartSetAsDefault(
      base::BindOnce(&ShellIntegrationDefaultBrowserSetter::OnComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ShellIntegrationDefaultBrowserSetter::OnComplete(
    DefaultBrowserState default_browser_state) {
  worker_.reset();
  std::move(on_complete_callback_).Run(default_browser_state);
}

}  // namespace default_browser
