// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/test_icon_image_observer.h"

#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

TestIconImageObserver::TestIconImageObserver() {}
TestIconImageObserver::~TestIconImageObserver() = default;

void TestIconImageObserver::Wait(IconImage* icon) {
  if (!icon->did_complete_initial_load()) {
    // Tricky: The icon might not actually be visible in the viewport yet (e.g.,
    // if it's for an extension that is buried in the menu). Force the icon to
    // load by requesting a bitmap.
    icon->image_skia().bitmap();
    observation_.Observe(icon);
    run_loop_.Run();
  }
}

void TestIconImageObserver::OnExtensionIconImageChanged(IconImage* icon) {
  DCHECK(icon->did_complete_initial_load());
  run_loop_.Quit();
}

void TestIconImageObserver::WaitForIcon(IconImage* icon) {
  TestIconImageObserver().Wait(icon);
}
void TestIconImageObserver::WaitForExtensionActionIcon(
    const Extension* extension,
    content::BrowserContext* context) {
  DCHECK(extension);
  auto* action_manager = ExtensionActionManager::Get(context);
  ExtensionAction* action = action_manager->GetExtensionAction(*extension);

  DCHECK(action);
  DCHECK(action->default_icon_image());
  WaitForIcon(action->default_icon_image());
}

}  // namespace extensions
