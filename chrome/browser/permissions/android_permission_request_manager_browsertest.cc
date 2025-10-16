// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"
#include "url/gurl.h"

namespace {
using ::testing::Invoke;

class AndroidPermissionRequestManagerBrowserTest : public AndroidBrowserTest {
 public:
  AndroidPermissionRequestManagerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        content_settings::features::kApproximateGeolocationPermission);
  }

  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  permissions::PermissionRequestManager* GetPermissionRequestManager() {
    return permissions::PermissionRequestManager::FromWebContents(
        web_contents());
  }
  content::RenderFrameHost* GetActiveMainFrame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* profile() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AndroidPermissionRequestManagerBrowserTest,
                       RequestGeolocationAndApproximateLocationGranted) {
  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents(), initial_url));

  blink::mojom::PermissionDescriptorPtr geolocation_permission_descriptor =
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              blink::PermissionType::GEOLOCATION);
  content::PermissionResult approx_only_permission_result(
      blink::mojom::PermissionStatus::GRANTED,
      content::PermissionStatusSource::UNSPECIFIED,
      GeolocationSetting({.approximate = PermissionOption::kAllowed,
                          .precise = PermissionOption::kDenied}));
  {
    auto* request_manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    request_manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
    request_manager->set_auto_response_prompt_options_for_test(
        GeolocationPromptOptions{.selected_precise = false});

    base::RunLoop run_loop;
    base::MockOnceCallback<void(content::PermissionResult)> callback;
    EXPECT_CALL(callback, Run(approx_only_permission_result)).WillOnce([&] {
      run_loop.Quit();
    });

    content::PermissionController* permission_controller =
        profile()->GetPermissionController();
    permission_controller->RequestPermissionFromCurrentDocument(
        web_contents()->GetPrimaryMainFrame(),
        content::PermissionRequestDescription(
            geolocation_permission_descriptor.Clone(),
            /*user_gesture=*/true),
        callback.Get());

    run_loop.Run();
  }

  // Now request the permission again. This should not trigger another prompt
  // but it should keep returning the granted permission.
  {
    auto* request_manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    request_manager->set_auto_response_for_test(
        permissions::PermissionRequestManager::AutoResponseType::NONE);

    base::RunLoop run_loop;
    base::MockOnceCallback<void(content::PermissionResult)> callback;
    EXPECT_CALL(callback, Run(approx_only_permission_result)).WillOnce([&] {
      run_loop.Quit();
    });

    content::PermissionController* permission_controller =
        profile()->GetPermissionController();
    EXPECT_EQ(permission_controller->GetPermissionResultForCurrentDocument(
                  geolocation_permission_descriptor.Clone(),
                  web_contents()->GetPrimaryMainFrame()),
              approx_only_permission_result);
    permission_controller->RequestPermissionFromCurrentDocument(
        web_contents()->GetPrimaryMainFrame(),
        content::PermissionRequestDescription(
            geolocation_permission_descriptor.Clone(),
            /*user_gesture=*/true),
        callback.Get());

    run_loop.Run();
  }
}

}  // anonymous namespace
