// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/launcher.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/manifest_handlers/action_handlers_handler.h"
#include "extensions/test/extension_test_message_listener.h"

namespace app_runtime = extensions::api::app_runtime;

using ActionHandlersBrowserTest = extensions::ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ActionHandlersBrowserTest, LaunchAppWithNewNote) {
  // Load the app. Make sure to wait until it is done loading.
  ExtensionTestMessageListener loader("loaded", false);
  base::FilePath path =
      test_data_dir_.AppendASCII("action_handlers").AppendASCII("new_note");
  const extensions::Extension* app = LoadExtension(path);
  ASSERT_TRUE(app);
  EXPECT_TRUE(extensions::ActionHandlersInfo::HasActionHandler(
      app, app_runtime::ACTION_TYPE_NEW_NOTE));
  EXPECT_TRUE(loader.WaitUntilSatisfied());

  // Fire a "new_note" action type, assert that app has received it.
  ExtensionTestMessageListener new_note("hasNewNote = true", false);
  auto action_data = std::make_unique<app_runtime::ActionData>();
  action_data->action_type = app_runtime::ActionType::ACTION_TYPE_NEW_NOTE;
  apps::LaunchPlatformAppWithAction(profile(), app, std::move(action_data),
                                    base::FilePath());
  EXPECT_TRUE(new_note.WaitUntilSatisfied());
}
