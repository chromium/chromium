// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace extensions {

using DeveloperPrivateApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest, Basics) {
  // Load up some extensions so that we can query their info and adjust their
  // setings in the API test.
  base::FilePath base_dir = test_data_dir_.AppendASCII("developer");
  EXPECT_TRUE(LoadExtension(base_dir.AppendASCII("hosted_app")));
  EXPECT_TRUE(InstallExtension(
      base_dir.AppendASCII("packaged_app"), 1, Manifest::INTERNAL));
  LoadExtension(base_dir.AppendASCII("simple_extension"));

  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "developer/test", kFlagLoadAsComponent));
}

// Tests opening the developer tools for an app window.
IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest, InspectAppWindowView) {
  base::FilePath dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &dir);
  dir = dir.AppendASCII("extensions")
            .AppendASCII("platform_apps")
            .AppendASCII("minimal");

  // Load and launch a platform app.
  const Extension* app = LoadAndLaunchApp(dir);

  // Get the info about the app, including the inspectable views.
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateGetExtensionInfoFunction());
  std::unique_ptr<base::Value> result(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), base::StringPrintf("[\"%s\"]", app->id().c_str()),
          browser()));
  ASSERT_TRUE(result);
  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      api::developer_private::ExtensionInfo::FromValue(*result);
  ASSERT_TRUE(info);

  // There should be two inspectable views - the background page and the app
  // window.  Find the app window.
  ASSERT_EQ(2u, info->views.size());
  const api::developer_private::ExtensionView* window_view = nullptr;
  for (const auto& view : info->views) {
    if (view.type == api::developer_private::VIEW_TYPE_APP_WINDOW) {
      window_view = &view;
      break;
    }
  }
  ASSERT_TRUE(window_view);

  // Inspect the app window.
  function = new api::DeveloperPrivateOpenDevToolsFunction();
  extension_function_test_utils::RunFunction(
      function.get(),
      base::StringPrintf("[{\"renderViewId\": %d, \"renderProcessId\": %d}]",
                         window_view->render_view_id,
                         window_view->render_process_id),
      browser(), api_test_utils::NONE);

  // Verify that dev tools opened.
  std::list<AppWindow*> app_windows =
      AppWindowRegistry::Get(profile())->GetAppWindowsForApp(app->id());
  ASSERT_EQ(1u, app_windows.size());
  EXPECT_TRUE(DevToolsWindow::GetInstanceForInspectedWebContents(
      (*app_windows.begin())->web_contents()));
}

IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest, InspectEmbeddedOptionsPage) {
  base::FilePath dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &dir);
  // Load an extension that only has an embedded options_ui page.
  const Extension* extension = LoadExtension(dir.AppendASCII("extensions")
                                                 .AppendASCII("delayed_install")
                                                 .AppendASCII("v1"));
  ASSERT_TRUE(extension);

  // Open the embedded options page.
  ASSERT_TRUE(ExtensionTabUtil::OpenOptionsPage(extension, browser()));
  WaitForExtensionNotIdle(extension->id());

  // Get the info about the extension, including the inspectable views.
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateGetExtensionInfoFunction());
  std::unique_ptr<base::Value> result(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(),
          base::StringPrintf("[\"%s\"]", extension->id().c_str()), browser()));
  ASSERT_TRUE(result);
  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      api::developer_private::ExtensionInfo::FromValue(*result);
  ASSERT_TRUE(info);

  // The embedded options page should show up.
  ASSERT_EQ(1u, info->views.size());
  const api::developer_private::ExtensionView& view = info->views[0];
  ASSERT_EQ(api::developer_private::VIEW_TYPE_EXTENSION_GUEST, view.type);

  // Inspect the embedded options page.
  function = new api::DeveloperPrivateOpenDevToolsFunction();
  extension_function_test_utils::RunFunction(
      function.get(),
      base::StringPrintf("[{\"renderViewId\": %d, \"renderProcessId\": %d}]",
                         view.render_view_id, view.render_process_id),
      browser(), api_test_utils::NONE);

  // Verify that dev tools opened.
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      view.render_process_id, view.render_view_id);
  ASSERT_TRUE(rfh);
  content::WebContents* wc = content::WebContents::FromRenderFrameHost(rfh);
  ASSERT_TRUE(wc);
  EXPECT_TRUE(DevToolsWindow::GetInstanceForInspectedWebContents(wc));
}

}  // namespace extensions
