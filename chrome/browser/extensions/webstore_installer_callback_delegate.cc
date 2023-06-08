// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_installer_callback_delegate.h"

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/extensions/webstore_installer.h"

namespace extensions {

WebstoreInstallerCallbackDelegate::WebstoreInstallerCallbackDelegate(
    SuccessCallback success_callback,
    FailureCallback failure_callback)
    : success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)) {
  CHECK(success_callback_);
  CHECK(failure_callback_);
}

WebstoreInstallerCallbackDelegate::~WebstoreInstallerCallbackDelegate() =
    default;

void WebstoreInstallerCallbackDelegate::OnExtensionInstallSuccess(
    const std::string& id) {
  CHECK(success_callback_);
  std::move(success_callback_).Run(id);
}

void WebstoreInstallerCallbackDelegate::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    FailureReason reason) {
  CHECK(failure_callback_);
  std::move(failure_callback_).Run(id, error, reason);
}

}  // namespace extensions
