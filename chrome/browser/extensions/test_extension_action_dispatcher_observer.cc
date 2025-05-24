// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_action_dispatcher_observer.h"

#include "base/memory/raw_ptr.h"
#include "extensions/browser/extension_action.h"

namespace extensions {

TestExtensionActionDispatcherObserver::TestExtensionActionDispatcherObserver(
    content::BrowserContext* context,
    const ExtensionId& extension_id)
    : extension_id_(extension_id) {
  scoped_observation_.Observe(ExtensionActionDispatcher::Get(context));
}

TestExtensionActionDispatcherObserver::TestExtensionActionDispatcherObserver(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::set<raw_ptr<content::WebContents, SetExperimental>>&
        contents_to_observe)
    : TestExtensionActionDispatcherObserver(context, extension_id) {
  contents_to_observe_ = contents_to_observe;
}

TestExtensionActionDispatcherObserver::
    ~TestExtensionActionDispatcherObserver() = default;

void TestExtensionActionDispatcherObserver::Wait() {
  run_loop_.Run();
}

void TestExtensionActionDispatcherObserver::OnExtensionActionUpdated(
    ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  if (extension_action->extension_id() == extension_id_) {
    last_web_contents_ = web_contents;
    contents_to_observe_.erase(web_contents);

    if (contents_to_observe_.empty()) {
      run_loop_.QuitWhenIdle();
    }
  }
}

}  // namespace extensions
