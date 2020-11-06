// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/test/result_catcher.h"

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

  ASSERT_TRUE(RunPlatformAppTestWithFlags("developer/test", kFlagNone,
                                          kFlagLoadAsComponent));
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

// Crashes on Linux only.  http://crbug.com/1134506
#if defined(OS_LINUX)
#define MAYBE_InspectInactiveServiceWorkerBackground \
  DISABLED_InspectInactiveServiceWorkerBackground
#else
#define MAYBE_InspectInactiveServiceWorkerBackground \
  InspectInactiveServiceWorkerBackground
#endif

IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest,
                       MAYBE_InspectInactiveServiceWorkerBackground) {
  ResultCatcher result_catcher;
  // Load an extension that is service worker based.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("service_worker")
                        .AppendASCII("worker_based_background")
                        .AppendASCII("inspect"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  service_worker_test_utils::TestRegistrationObserver registration_observer(
      browser()->profile());
  registration_observer.WaitForRegistrationStored();

  // Stop the service worker.
  {
    base::RunLoop run_loop;
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(profile());
    content::ServiceWorkerContext* context =
        storage_partition->GetServiceWorkerContext();
    content::StopServiceWorkerForScope(context, extension->url(),
                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  // Get the info about the extension, including the inspectable views.
  auto get_info_function =
      base::MakeRefCounted<api::DeveloperPrivateGetExtensionInfoFunction>();
  std::unique_ptr<base::Value> result(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_info_function.get(),
          base::StringPrintf("[\"%s\"]", extension->id().c_str()), browser()));
  ASSERT_TRUE(result);
  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      api::developer_private::ExtensionInfo::FromValue(*result);
  ASSERT_TRUE(info);

  // There should be a worker based background for the extension.
  ASSERT_EQ(1u, info->views.size());
  const api::developer_private::ExtensionView& view = info->views[0];
  EXPECT_EQ(
      api::developer_private::VIEW_TYPE_EXTENSION_SERVICE_WORKER_BACKGROUND,
      view.type);
  // The service worker should be inactive (indicated by -1 for
  // the process id).
  EXPECT_EQ(-1, view.render_process_id);

  // Inspect the inactive service worker background.
  DevToolsWindowCreationObserver devtools_window_created_observer;
  auto dev_tools_function =
      base::MakeRefCounted<api::DeveloperPrivateOpenDevToolsFunction>();
  extension_function_test_utils::RunFunction(dev_tools_function.get(),
                                             base::StringPrintf(
                                                 R"([{"renderViewId": -1,
                                                      "renderProcessId": -1,
                                                      "isServiceWorker": true,
                                                      "extensionId": "%s"
                                                   }])",
                                                 extension->id().c_str()),
                                             browser(), api_test_utils::NONE);
  devtools_window_created_observer.WaitForLoad();

  // Verify that dev tool window opened.
  scoped_refptr<content::DevToolsAgentHost> service_worker_host;
  content::DevToolsAgentHost::List targets =
      content::DevToolsAgentHost::GetOrCreateAll();
  for (const scoped_refptr<content::DevToolsAgentHost>& host : targets) {
    if (host->GetType() == content::DevToolsAgentHost::kTypeServiceWorker &&
        host->GetURL() ==
            extension->GetResourceURL(
                BackgroundInfo::GetBackgroundServiceWorkerScript(extension))) {
      EXPECT_FALSE(service_worker_host);
      service_worker_host = host;
    }
  }

  ASSERT_TRUE(service_worker_host);
  EXPECT_TRUE(DevToolsWindow::FindDevToolsWindow(service_worker_host.get()));
}

IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest,
                       InspectActiveServiceWorkerBackground) {
  ResultCatcher result_catcher;
  // Load an extension that is service worker based.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("service_worker")
                        .AppendASCII("worker_based_background")
                        .AppendASCII("inspect"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  // Get the info about the extension, including the inspectable views.
  auto get_info_function =
      base::MakeRefCounted<api::DeveloperPrivateGetExtensionInfoFunction>();
  std::unique_ptr<base::Value> result(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          get_info_function.get(),
          base::StringPrintf("[\"%s\"]", extension->id().c_str()), browser()));
  ASSERT_TRUE(result);
  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      api::developer_private::ExtensionInfo::FromValue(*result);
  ASSERT_TRUE(info);

  // There should be a worker based background for the extension.
  ASSERT_EQ(1u, info->views.size());
  const api::developer_private::ExtensionView& view = info->views[0];
  EXPECT_EQ(
      api::developer_private::VIEW_TYPE_EXTENSION_SERVICE_WORKER_BACKGROUND,
      view.type);
  EXPECT_NE(-1, view.render_process_id);

  // Inspect the service worker page.
  auto dev_tools_function =
      base::MakeRefCounted<api::DeveloperPrivateOpenDevToolsFunction>();
  extension_function_test_utils::RunFunction(
      dev_tools_function.get(),
      base::StringPrintf(
          R"([{"renderViewId": -1,
               "renderProcessId": %d,
               "isServiceWorker": true,
               "extensionId": "%s"
            }])",
          info->views[0].render_process_id, extension->id().c_str()),
      browser(), api_test_utils::NONE);

  // Find the service worker background host.
  content::DevToolsAgentHost::List targets =
      content::DevToolsAgentHost::GetOrCreateAll();
  scoped_refptr<content::DevToolsAgentHost> service_worker_host;
  for (const scoped_refptr<content::DevToolsAgentHost>& host : targets) {
    if (host->GetType() == content::DevToolsAgentHost::kTypeServiceWorker &&
        host->GetURL() ==
            extension->GetResourceURL(
                BackgroundInfo::GetBackgroundServiceWorkerScript(extension))) {
      EXPECT_FALSE(service_worker_host);
      service_worker_host = host;
    }
  }

  // Verify that dev tools opened.
  ASSERT_TRUE(service_worker_host);
  EXPECT_TRUE(DevToolsWindow::FindDevToolsWindow(service_worker_host.get()));
}

}  // namespace extensions
