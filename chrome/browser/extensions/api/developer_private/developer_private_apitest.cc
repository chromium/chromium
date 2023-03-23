// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
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
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class DeveloperPrivateApiTest : public ExtensionApiTest {
 protected:
  std::unique_ptr<api::developer_private::ExtensionInfo> GetExtensionInfo(
      const Extension& extension) {
    auto get_info_function =
        base::MakeRefCounted<api::DeveloperPrivateGetExtensionInfoFunction>();
    std::unique_ptr<base::Value> result =
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            get_info_function.get(),
            content::JsReplace(R"([$1])", extension.id()), browser());
    if (!result) {
      ADD_FAILURE() << "No result back when getting extension info";
      return nullptr;
    }
    std::unique_ptr<api::developer_private::ExtensionInfo> info =
        api::developer_private::ExtensionInfo::FromValueDeprecated(*result);
    if (!info)
      ADD_FAILURE() << "Problem creating ExtensionInfo from result data";
    return info;
  }
};

IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest, Basics) {
  // Load up some extensions so that we can query their info and adjust their
  // setings in the API test.
  base::FilePath base_dir = test_data_dir_.AppendASCII("developer");
  EXPECT_TRUE(LoadExtension(base_dir.AppendASCII("hosted_app")));
  EXPECT_TRUE(InstallExtension(base_dir.AppendASCII("packaged_app"), 1,
                               mojom::ManifestLocation::kInternal));
  LoadExtension(base_dir.AppendASCII("simple_extension"));

  ASSERT_TRUE(RunExtensionTest("developer/test",
                               {.launch_as_platform_app = true},
                               {.load_as_component = true}));
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
  auto info = GetExtensionInfo(*app);

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
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateOpenDevToolsFunction>();
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
  auto info = GetExtensionInfo(*extension);

  // The embedded options page should show up.
  ASSERT_EQ(1u, info->views.size());
  const api::developer_private::ExtensionView& view = info->views[0];
  ASSERT_EQ(api::developer_private::VIEW_TYPE_EXTENSION_GUEST, view.type);

  // Inspect the embedded options page.
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateOpenDevToolsFunction>();
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

IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest,
                       InspectInactiveServiceWorkerBackground) {
  ResultCatcher result_catcher;
  // Load an extension that is service worker-based.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("service_worker")
                        .AppendASCII("worker_based_background")
                        .AppendASCII("inspect"),
                    // Wait for the registration to be stored since we'll stop
                    // the worker.
                    {.wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  // Stop the service worker.
  browsertest_util::StopServiceWorkerForExtensionGlobalScope(profile(),
                                                             extension->id());

  // Get the info about the extension, including the inspectable views.
  auto info = GetExtensionInfo(*extension);

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
  auto info = GetExtensionInfo(*extension);

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

// TODO(crbug.com/1395713): The test is flaky on MSAN and Linux. Re-enable it.
#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_LINUX)
#define MAYBE_InspectSplitModeServiceWorkerBackgrounds \
  DISABLED_InspectSplitModeServiceWorkerBackgrounds
#else
#define MAYBE_InspectSplitModeServiceWorkerBackgrounds \
  InspectSplitModeServiceWorkerBackgrounds
#endif
IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest,
                       MAYBE_InspectSplitModeServiceWorkerBackgrounds) {
  ResultCatcher result_catcher;
  // Load an extension that is service worker based, split mode and enabled in
  // incognito.
  static constexpr char kManifest[] =
      R"({
           "name": "Split mode worker test",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "worker.js"},
           "incognito": "split"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"),
                     "chrome.test.notifyPass();");
  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  // Get the info about the extension, including the inspectable views.
  auto info = GetExtensionInfo(*extension);

  // With no incognito window open, there should be a single worker based
  // background page for the extension.
  ASSERT_EQ(1u, info->views.size());
  int main_render_process_id = -1;
  {
    const api::developer_private::ExtensionView& view = info->views[0];
    EXPECT_EQ(
        api::developer_private::VIEW_TYPE_EXTENSION_SERVICE_WORKER_BACKGROUND,
        view.type);
    EXPECT_NE(-1, view.render_process_id);
    main_render_process_id = view.render_process_id;
    EXPECT_FALSE(view.incognito);
  }

  // Now open up an incognito browser window page and check the inspectable
  // views again. Waiting for the result catcher will wait for the incognito
  // service worker to have become active.
  Browser* inconito_browser = CreateIncognitoBrowser(browser()->profile());
  ASSERT_TRUE(inconito_browser);
  ASSERT_TRUE(result_catcher.GetNextResult());
  info = GetExtensionInfo(*extension);
  // The views should now have 2 entries, one for the main worker which will be
  // the same as before and a new one for the incognito worker.
  ASSERT_EQ(2u, info->views.size());
  EXPECT_NE(info->views[0].incognito, info->views[1].incognito);
  int incognito_render_process_id = -1;
  for (auto& view : info->views) {
    EXPECT_EQ(
        api::developer_private::VIEW_TYPE_EXTENSION_SERVICE_WORKER_BACKGROUND,
        view.type);
    EXPECT_NE(-1, view.render_process_id);
    if (view.incognito) {
      EXPECT_NE(main_render_process_id, view.render_process_id);
      incognito_render_process_id = view.render_process_id;
    } else {
      EXPECT_EQ(main_render_process_id, view.render_process_id);
    }
  }

  // Open a devtools window for both the primary and incognito worker.
  std::string kOpenDevToolsParams =
      R"([{"renderViewId": -1,
           "renderProcessId": $1,
           "isServiceWorker": true,
           "extensionId": $2,
           "incognito": $3
         }])";
  DevToolsWindow* main_devtools_window = nullptr;
  {
    DevToolsWindowCreationObserver devtools_window_created_observer;
    auto dev_tools_function =
        base::MakeRefCounted<api::DeveloperPrivateOpenDevToolsFunction>();
    extension_function_test_utils::RunFunction(
        dev_tools_function.get(),
        content::JsReplace(kOpenDevToolsParams, main_render_process_id,
                           extension->id().c_str(), /*incognito:*/ false),
        browser(), api_test_utils::NONE);
    devtools_window_created_observer.WaitForLoad();
    main_devtools_window = devtools_window_created_observer.devtools_window();
  }
  DevToolsWindow* incognito_devtools_window = nullptr;
  {
    DevToolsWindowCreationObserver devtools_window_created_observer;
    auto dev_tools_function =
        base::MakeRefCounted<api::DeveloperPrivateOpenDevToolsFunction>();
    extension_function_test_utils::RunFunction(
        dev_tools_function.get(),
        content::JsReplace(kOpenDevToolsParams, incognito_render_process_id,
                           extension->id().c_str(), /*incognito:*/ true),
        browser(), api_test_utils::NONE);
    devtools_window_created_observer.WaitForLoad();
    incognito_devtools_window =
        devtools_window_created_observer.devtools_window();
  }

  // Both windows should have opened and should not point to the same window.
  ASSERT_TRUE(main_devtools_window);
  ASSERT_TRUE(incognito_devtools_window);
  ASSERT_NE(main_devtools_window, incognito_devtools_window);
}

// Test that offscreen documents show up in the list of inspectable views and
// can be inspected.
IN_PROC_BROWSER_TEST_F(DeveloperPrivateApiTest, InspectOffscreenDocument) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     "<html>offscreen</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());

  // Create an offscreen document and wait for it to load.
  std::unique_ptr<OffscreenDocumentHost> offscreen_document;
  GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  {
    ExtensionHostTestHelper offscreen_waiter(profile(), extension->id());
    offscreen_waiter.RestrictToType(mojom::ViewType::kOffscreenDocument);
    offscreen_document = std::make_unique<OffscreenDocumentHost>(
        *extension,
        ProcessManager::Get(profile())
            ->GetSiteInstanceForURL(offscreen_url)
            .get(),
        offscreen_url);
    offscreen_document->CreateRendererSoon();
    offscreen_waiter.WaitForHostCompletedFirstLoad();
  }

  // Get the list of inspectable views for the extension.
  auto info = GetExtensionInfo(*extension);

  // The only inspectable view should be the offscreen document. Validate the
  // metadata.
  ASSERT_EQ(1u, info->views.size());
  const api::developer_private::ExtensionView& view = info->views[0];
  EXPECT_EQ(api::developer_private::VIEW_TYPE_OFFSCREEN_DOCUMENT, view.type);
  content::WebContents* offscreen_contents =
      offscreen_document->host_contents();
  EXPECT_EQ(offscreen_url.spec(), view.url);
  EXPECT_EQ(offscreen_document->render_process_host()->GetID(),
            view.render_process_id);
  EXPECT_EQ(offscreen_contents->GetPrimaryMainFrame()->GetRoutingID(),
            view.render_view_id);
  EXPECT_FALSE(view.incognito);
  EXPECT_FALSE(view.is_iframe);

  // The document shouldn't currently be under inspection.
  EXPECT_FALSE(
      DevToolsWindow::GetInstanceForInspectedWebContents(offscreen_contents));

  // Call the API function to inspect the offscreen document.
  auto dev_tools_function =
      base::MakeRefCounted<api::DeveloperPrivateOpenDevToolsFunction>();
  extension_function_test_utils::RunFunction(
      dev_tools_function.get(),
      content::JsReplace(
          R"([{"renderViewId": $1,
               "renderProcessId": $2,
               "extensionId": $3
            }])",
          view.render_view_id, view.render_process_id, extension->id()),
      browser(), api_test_utils::NONE);

  // Validate that the devtools window is now shown.
  DevToolsWindow* dev_tools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(offscreen_contents);
  ASSERT_TRUE(dev_tools_window);

  // Tidy up.
  DevToolsWindowTesting::CloseDevToolsWindowSync(dev_tools_window);
}

}  // namespace extensions
