// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

namespace {

// A key into the StateStore; we don't use any results, but need to know when
// it's initialized.
const char kBrowserActionStorageKey[] = "browser_action";
// The name of the extension we add.
const char kExtensionName[] = "Default Persistence Test Extension";

void QuitMessageLoop(content::MessageLoopRunner* runner,
                     std::unique_ptr<base::Value> value) {
  runner->Quit();
}

// We need to wait for the state store to initialize and respond to requests
// so we can see if the preferences persist. Do this by posting our own request
// to the state store, which should be handled after all others.
void WaitForStateStore(Profile* profile, const std::string& extension_id) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  ExtensionSystem::Get(profile)->state_store()->GetExtensionValue(
      extension_id, kBrowserActionStorageKey,
      base::Bind(&QuitMessageLoop, base::RetainedRef(runner)));
  runner->Run();
}

}  // namespace

// Setup for the test by loading an extension, which should set the browser
// action background to blue.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       PRE_BrowserActionDefaultPersistence) {
  ExtensionTestMessageListener listener("Background Color Set",
                                        false /* won't send custom reply */);

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("default_persistence"));
  ASSERT_TRUE(extension);
  ASSERT_EQ(kExtensionName, extension->name());
  WaitForStateStore(profile(), extension->id());

  // Make sure we've given the extension enough time to set the background color
  // in chrome.runtime.onInstalled.
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  ExtensionAction* extension_action =
      ExtensionActionManager::Get(profile())->GetExtensionAction(*extension);
  ASSERT_TRUE(extension_action);
  EXPECT_EQ(SK_ColorBLUE, extension_action->GetBadgeBackgroundColor(0));
}

// When Chrome restarts, the Extension will immediately update the browser
// action, but will not modify the badge background color. Thus, the background
// should remain blue (persisting the default set in onInstalled()).
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, BrowserActionDefaultPersistence) {
  // Find the extension (it's a shame we don't have an ID for this, but it
  // was generated in the last test).
  const Extension* extension = NULL;
  const ExtensionSet& extension_set =
      ExtensionRegistry::Get(profile())->enabled_extensions();
  for (ExtensionSet::const_iterator iter = extension_set.begin();
       iter != extension_set.end();
       ++iter) {
    if ((*iter)->name() == kExtensionName) {
      extension = iter->get();
      break;
    }
  }
  ASSERT_TRUE(extension) << "Could not find extension in registry.";

  ExtensionAction* extension_action =
      ExtensionActionManager::Get(profile())->GetExtensionAction(*extension);
  ASSERT_TRUE(extension_action);

  // If the extension hasn't already set the badge text, then we should wait for
  // it to do so.
  if (extension_action->GetExplicitlySetBadgeText(0) != "Hello") {
    ExtensionTestMessageListener listener("Badge Text Set",
                                          false /* won't send custom reply */);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // If this log becomes frequent, this test is losing its effectiveness, and
  // we need to find a more invasive way of ensuring the test's StateStore
  // initializes after extensions get their onStartup event.
  if (ExtensionSystem::Get(profile())->state_store()->IsInitialized())
    LOG(WARNING) << "State store already initialized; test guaranteed to pass.";

  // Wait for the StateStore to load, and fetch the defaults.
  WaitForStateStore(profile(), extension->id());

  // Ensure the BrowserAction's badge background is still blue.
  EXPECT_EQ(SK_ColorBLUE, extension_action->GetBadgeBackgroundColor(0));
}

}  // namespace extensions
