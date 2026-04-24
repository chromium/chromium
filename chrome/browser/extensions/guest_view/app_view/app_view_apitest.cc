// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/launcher.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/extensions/chrome_extensions_browser_client.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/guest_view/app_view/chrome_app_view_guest_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/test/extension_test_message_listener.h"

static_assert(BUILDFLAG(IS_CHROMEOS));

namespace extensions {
namespace {

class MockAppDelegate : public ChromeAppDelegate {
 public:
  explicit MockAppDelegate(Profile* profile)
      : ChromeAppDelegate(profile, /*keep_alive=*/false) {
    EXPECT_EQ(instance_, nullptr);
    instance_ = this;
  }
  ~MockAppDelegate() override { instance_ = nullptr; }

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      const extensions::Extension* extension) override {
    media_access_requested_ = true;
    if (media_access_request_quit_closure_) {
      std::move(media_access_request_quit_closure_).Run();
    }
  }

  void WaitForRequestMediaPermission() {
    if (media_access_requested_) {
      return;
    }
    base::RunLoop run_loop;
    media_access_request_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  static MockAppDelegate* Get() { return instance_; }

 private:
  bool media_access_requested_ = false;
  base::OnceClosure media_access_request_quit_closure_;

  static MockAppDelegate* instance_;
};

MockAppDelegate* MockAppDelegate::instance_ = nullptr;

class MockAppViewGuestDelegate : public ChromeAppViewGuestDelegate {
 public:
  MockAppViewGuestDelegate() = default;

  extensions::AppDelegate* CreateAppDelegate(
      content::BrowserContext* browser_context) override {
    return new MockAppDelegate(Profile::FromBrowserContext(browser_context));
  }
};

class MockExtensionsAPIClient : public ChromeExtensionsAPIClient {
 public:
  MockExtensionsAPIClient() = default;

  std::unique_ptr<extensions::AppViewGuestDelegate> CreateAppViewGuestDelegate()
      const override {
    return std::make_unique<MockAppViewGuestDelegate>();
  }
};

class AppViewApiTest : public ExtensionBrowserTest,
                       public testing::WithParamInterface<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "MPArch" : "InnerWebContents";
  }

  AppViewApiTest() {
    scoped_feature_list_.InitWithFeatureState(features::kGuestViewMPArch,
                                              GetParam());
  }

 protected:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    test_guest_view_manager_ = factory_.GetOrCreateTestGuestViewManager(
        profile(),
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
  }

  void TearDownOnMainThread() override {
    test_guest_view_manager_ = nullptr;
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* GetFirstAppWindowWebContents() {
    const AppWindowRegistry::AppWindowList& app_window_list =
        AppWindowRegistry::Get(profile())->app_windows();
    EXPECT_EQ(1U, app_window_list.size());
    return (*app_window_list.begin())->web_contents();
  }

  const Extension* LoadApp(const std::string& app_location) {
    return LoadExtension(test_data_dir_.AppendASCII(app_location));
  }

  void RunTest(const std::string& test_name,
               const std::string& app_location,
               const std::string& app_to_embed) {
    const Extension* app_embedder = LoadApp(app_location);
    ASSERT_TRUE(app_embedder);
    const Extension* app_embedded = LoadApp(app_to_embed);
    ASSERT_TRUE(app_embedded);

    apps::LaunchPlatformApp(profile(), app_embedder,
                            AppLaunchSource::kSourceUntracked);

    ExtensionTestMessageListener launch_listener("LAUNCHED");
    ASSERT_TRUE(launch_listener.WaitUntilSatisfied());

    ExtensionTestMessageListener done_listener("TEST_PASSED");
    done_listener.set_failure_message("TEST_FAILED");
    ASSERT_TRUE(content::ExecJs(
        GetFirstAppWindowWebContents(),
        base::StringPrintf("runTest('%s', '%s')", test_name.c_str(),
                           app_embedded->id().c_str())))
        << "Unable to start test.";
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

  guest_view::TestGuestViewManager* test_guest_view_manager() const {
    return test_guest_view_manager_;
  }

 private:
  guest_view::TestGuestViewManagerFactory factory_;
  raw_ptr<guest_view::TestGuestViewManager> test_guest_view_manager_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         AppViewApiTest,
                         testing::Bool(),
                         AppViewApiTest::DescribeParams);

// Tests that <appview> correctly processes parameters passed on connect.
IN_PROC_BROWSER_TEST_P(AppViewApiTest, TestAppViewGoodDataShouldSucceed) {
  RunTest("testAppViewGoodDataShouldSucceed", "app_view/apitest",
          "app_view/apitest/skeleton");
  // Note that the callback of the appview connect method runs after guest
  // creation, but not necessarily after attachment. So we now ensure that the
  // guest successfully attaches and loads.
  EXPECT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(
      test_guest_view_manager()->WaitForSingleGuestViewCreated()));
}

// Tests that <appview> can handle media permission requests.
IN_PROC_BROWSER_TEST_P(AppViewApiTest, TestAppViewMediaRequest) {
  // TODO(crbug.com/40202416): Implement for MPArch.
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    GTEST_SKIP() << "MPArch implementation skipped. https://crbug.com/40202416";
  }

  // In browser_tests, the ExtensionsBrowserClient is always a
  // ChromeExtensionsBrowserClient. Null it first to delete the old one
  // before creating the new one.
  static_cast<ChromeExtensionsBrowserClient*>(ExtensionsBrowserClient::Get())
      ->SetAPIClientForTest(nullptr);
  static_cast<ChromeExtensionsBrowserClient*>(ExtensionsBrowserClient::Get())
      ->SetAPIClientForTest(std::make_unique<MockExtensionsAPIClient>());

  RunTest("testAppViewMediaRequest", "app_view/apitest",
          "app_view/apitest/media_request");

  MockAppDelegate::Get()->WaitForRequestMediaPermission();
}

// Tests that <appview> correctly processes parameters passed on connect.
// This test should fail to connect because the embedded app (skeleton) will
// refuse the data passed by the embedder app and deny the request.
IN_PROC_BROWSER_TEST_P(AppViewApiTest, TestAppViewRefusedDataShouldFail) {
  RunTest("testAppViewRefusedDataShouldFail", "app_view/apitest",
          "app_view/apitest/skeleton");
}

// Tests that <appview> is able to navigate to another installed app.
IN_PROC_BROWSER_TEST_P(AppViewApiTest,
                       TestAppViewWithUndefinedDataShouldSucceed) {
  RunTest("testAppViewWithUndefinedDataShouldSucceed", "app_view/apitest",
          "app_view/apitest/skeleton");
  EXPECT_TRUE(test_guest_view_manager()->WaitUntilAttachedAndLoaded(
      test_guest_view_manager()->WaitForSingleGuestViewCreated()));
}

IN_PROC_BROWSER_TEST_P(AppViewApiTest, TestAppViewNoEmbedRequestListener) {
  RunTest("testAppViewNoEmbedRequestListener", "app_view/apitest",
          "app_view/apitest/no_embed_request_listener");
}

}  // namespace
}  // namespace extensions
