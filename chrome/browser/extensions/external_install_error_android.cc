// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_install_error_android.h"

#include "base/notimplemented.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// ExternalInstallErrorAndroid
// Android implementation of ExternalInstallError
// TODO(crbug.com/): Implement external install error UI and CWS integration on
// desktop android.
ExternalInstallErrorAndroid::ExternalInstallErrorAndroid(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    AlertType alert_type,
    ExternalInstallManager* manager) {}

ExternalInstallErrorAndroid::~ExternalInstallErrorAndroid() = default;

void ExternalInstallErrorAndroid::OnInstallPromptDone(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  NOTIMPLEMENTED();
}

void ExternalInstallErrorAndroid::DidOpenBubbleView() {
  NOTIMPLEMENTED();
}

void ExternalInstallErrorAndroid::DidCloseBubbleView() {
  NOTIMPLEMENTED();
}

const Extension* ExternalInstallErrorAndroid::GetExtension() const {
  NOTIMPLEMENTED();
  return nullptr;
}

const ExtensionId& ExternalInstallErrorAndroid::extension_id() const {
  NOTIMPLEMENTED();
  return extension_id_;
}
ExternalInstallError::AlertType ExternalInstallErrorAndroid::alert_type()
    const {
  NOTIMPLEMENTED();
  return alert_type_;
}

ExtensionInstallPrompt::Prompt*
ExternalInstallErrorAndroid::GetPromptForTesting() const {
  NOTIMPLEMENTED();
  return nullptr;
}
}  // namespace extensions
