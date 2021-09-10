// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_controller.h"

#include <unistd.h>

#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/lacros/lacros_extension_apps_publisher.h"
#include "chrome/browser/lacros/lacros_extension_apps_utility.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using LacrosExtensionAppsControllerTest = extensions::ExtensionBrowserTest;

// Test that launching an app causing it to appear in the shelf. Closing the app
// removes it from the shelf.
IN_PROC_BROWSER_TEST_F(LacrosExtensionAppsControllerTest, ShowsInShelf) {
  // If ash is does not contain the relevant test controller functionality, then
  // there's nothing to do for this test.
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kDoesItemExistInShelfMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Create the controller and publisher.
  LacrosExtensionAppsPublisher publisher;
  publisher.Initialize();
  LacrosExtensionAppsController controller;
  controller.Initialize(publisher.publisher());

  // No item should exist in the shelf before the window is launched.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("platform_apps/minimal"));
  std::string app_id =
      lacros_extension_apps_utility::MuxId(profile(), extension);
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);

  // There should be no app windows.
  ASSERT_TRUE(
      extensions::AppWindowRegistry::Get(profile())->app_windows().empty());

  // Launch the app via LacrosExtensionAppsController.
  crosapi::mojom::LaunchParamsPtr launch_params =
      crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = apps::mojom::LaunchSource::kFromTest;
  controller.Launch(std::move(launch_params), base::DoNothing());

  // Wait for item to exist in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/true);

  // Close all app windows.
  for (extensions::AppWindow* app_window :
       extensions::AppWindowRegistry::Get(profile())->app_windows()) {
    app_window->GetBaseWindow()->Close();
  }

  // Wait for item to stop existing in shelf.
  browser_test_util::WaitForShelfItem(app_id, /*exists=*/false);
}

}  // namespace
