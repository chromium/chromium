// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/test_extension_action_api_observer.h"

namespace extensions {

TestExtensionActionAPIObserver::TestExtensionActionAPIObserver(
    content::BrowserContext* context,
    const ExtensionId& extension_id)
    : extension_id_(extension_id), scoped_observer_(this) {
  scoped_observer_.Add(ExtensionActionAPI::Get(context));
}

TestExtensionActionAPIObserver::~TestExtensionActionAPIObserver() = default;

void TestExtensionActionAPIObserver::Wait() {
  run_loop_.Run();
}

void TestExtensionActionAPIObserver::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  if (extension_action->extension_id() == extension_id_) {
    last_web_contents_ = web_contents;
    run_loop_.QuitWhenIdle();
  }
}

}  // namespace extensions
