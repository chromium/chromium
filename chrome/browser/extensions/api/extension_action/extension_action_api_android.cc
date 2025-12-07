// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"

#include "base/notimplemented.h"
#include "base/notreached.h"

namespace extensions {

ActionOpenPopupFunction::ActionOpenPopupFunction() = default;
ActionOpenPopupFunction::~ActionOpenPopupFunction() = default;

ExtensionFunction::ResponseAction ActionOpenPopupFunction::Run() {
  // TODO(crbug.com/419057482): Support this on desktop Android when we have a
  // cross-platform representation of windows and tabs. Ideally, merge it with
  // the non-Android implementation.
  return RespondNow(Error("openPopup is not yet supported on desktop Android"));
}

void ActionOpenPopupFunction::OnShowPopupComplete(ExtensionHost* popup_host) {
  NOTREACHED();
}

// browserAction is manifest V2 and below only, but Android only supports
// manifest V3 and above. Use stubs here with NOTREACHED() just to keep the
// shared header file less ifdef-y.
BrowserActionOpenPopupFunction::BrowserActionOpenPopupFunction() = default;
BrowserActionOpenPopupFunction::~BrowserActionOpenPopupFunction() = default;

ExtensionFunction::ResponseAction BrowserActionOpenPopupFunction::Run() {
  NOTREACHED();
}

void BrowserActionOpenPopupFunction::OnBrowserContextShutdown() {
  NOTREACHED();
}

void BrowserActionOpenPopupFunction::OpenPopupTimedOut() {
  NOTREACHED();
}

void BrowserActionOpenPopupFunction::OnExtensionHostCompletedFirstLoad(
    content::BrowserContext* browser_context,
    ExtensionHost* host) {
  NOTREACHED();
}

}  // namespace extensions
