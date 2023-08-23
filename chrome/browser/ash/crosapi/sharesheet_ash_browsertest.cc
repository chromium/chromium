// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/sharesheet_ash.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_base.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/components/sharesheet/constants.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "components/exo/window_properties.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/aura/window.h"

namespace {

// The native window for each Lacros browser window has a unique generated id.
std::string GenerateUniqueId() {
  return std::string("org.chromium.test.") +
         base::UnguessableToken::Create().ToString();
}

bool IsIntentAcceptedByApp(const crosapi::mojom::IntentPtr& intent,
                           Profile* profile,
                           const std::string& app_id) {
  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      apps::AppServiceProxyFactory::GetForProfile(profile)->GetAppsForIntent(
          apps_util::CreateAppServiceIntentFromCrosapi(intent, profile));
  return base::Contains(intent_launch_info, app_id,
                        &apps::IntentLaunchInfo::app_id);
}

// Returns an Intent accepted by Sample System Web App.
crosapi::mojom::IntentPtr CreateIconIntent(Profile* profile) {
  constexpr char kIconMimeType[] = "image/x-xbitmap";

  base::FilePath file_path;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::File file = base::CreateAndOpenTemporaryFileInDir(
        file_manager::util::GetShareCacheFilePath(profile), &file_path);
    const std::string kData(12, '*');
    EXPECT_TRUE(base::CreateDirectory(file_path.DirName()));
    EXPECT_TRUE(base::WriteFile(file_path, kData));
  }

  crosapi::mojom::IntentPtr intent = crosapi::mojom::Intent::New();
  intent->action = apps_util::kIntentActionSend;
  intent->mime_type = kIconMimeType;
  intent->files = std::vector<crosapi::mojom::IntentFilePtr>();
  intent->files->emplace_back(
      crosapi::mojom::IntentFile::New(file_path, kIconMimeType));
  return intent;
}

// Returns an Intent rejected by Sample System Web App.
crosapi::mojom::IntentPtr CreateTextIntent(Profile* profile) {
  crosapi::mojom::IntentPtr intent = crosapi::mojom::Intent::New();
  intent->action = apps_util::kIntentActionSend;
  intent->mime_type = "text/plain";
  intent->share_text = "Hello";
  return intent;
}

sharesheet::SharesheetResult ShowBubble(const std::string& window_id,
                                        crosapi::mojom::IntentPtr intent,
                                        Profile* profile) {
  crosapi::SharesheetAsh* const sharesheet_ash =
      crosapi::CrosapiManager::Get()->crosapi_ash()->sharesheet_ash();
  sharesheet_ash->MaybeSetProfile(profile);
  sharesheet::SharesheetResult result =
      sharesheet::SharesheetResult::kErrorAlreadyOpen;
  base::RunLoop run_loop;
  sharesheet_ash->ShowBubble(
      window_id, sharesheet::LaunchSource::kWebShare, std::move(intent),
      base::BindLambdaForTesting(
          [&result, &run_loop](sharesheet::SharesheetResult sharesheet_result) {
            result = sharesheet_result;
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

}  // namespace

class SharesheetAshBrowserTest : public ash::SystemWebAppIntegrationTest {
 public:
  SharesheetAshBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        ash::standalone_browser::GetFeatureRefs(), {});
  }
  ~SharesheetAshBrowserTest() override = default;

  // SystemWebAppIntegrationTest:
  void SetUpOnMainThread() override {
    SystemWebAppIntegrationTest::SetUpOnMainThread();
    WaitForTestSystemAppInstall();

    // When Lacros web apps are enabled, SWAs use kSystemWeb app type.
    apps::AppTypeInitializationWaiter(browser()->profile(),
                                      apps::AppType::kSystemWeb)
        .Await();

    // The Sample System Web App will be automatically selected from the
    // Sharesheet bubble.
    sharesheet::SharesheetService::SetSelectedAppForTesting(
        base::UTF8ToUTF16(base::StringPiece{web_app::kSampleSystemWebAppId}));

    ASSERT_TRUE(crosapi::browser_util::IsLacrosEnabled());
  }
  void TearDownOnMainThread() override {
    sharesheet::SharesheetService::SetSelectedAppForTesting(std::u16string());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SharesheetAshBrowserTest, Success) {
  const std::string window_id = GenerateUniqueId();
  {
    aura::Window* window = browser()->window()->GetNativeWindow();
    window->SetProperty(exo::kApplicationIdKey, window_id);
    EXPECT_EQ(crosapi::GetShellSurfaceWindow(window_id), window);
  }

  ui_test_utils::AllBrowserTabAddedWaiter waiter;

  crosapi::mojom::IntentPtr intent = CreateIconIntent(profile());
  EXPECT_TRUE(
      IsIntentAcceptedByApp(intent, profile(), web_app::kSampleSystemWebAppId));
  sharesheet::SharesheetResult sharesheet_result =
      ShowBubble(window_id, std::move(intent), profile());
  EXPECT_EQ(sharesheet_result, sharesheet::SharesheetResult::kSuccess);

  // Sharesheet launches the Sample System Web App.
  content::WebContents* const contents = waiter.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(contents));
  EXPECT_EQ(contents->GetLastCommittedURL(),
            GetStartUrl(ash::SystemWebAppType::SAMPLE).Resolve("share.html"));
}

IN_PROC_BROWSER_TEST_P(SharesheetAshBrowserTest, Cancel) {
  const std::string window_id = GenerateUniqueId();
  {
    aura::Window* window = browser()->window()->GetNativeWindow();
    window->SetProperty(exo::kApplicationIdKey, window_id);
    EXPECT_EQ(crosapi::GetShellSurfaceWindow(window_id), window);
  }

  // When the Intent MIME type does not match the Sample app, the result is as
  // if the user cancelled.
  crosapi::mojom::IntentPtr intent = CreateTextIntent(profile());
  EXPECT_FALSE(
      IsIntentAcceptedByApp(intent, profile(), web_app::kSampleSystemWebAppId));
  sharesheet::SharesheetResult sharesheet_result =
      ShowBubble(window_id, std::move(intent), profile());
  EXPECT_EQ(sharesheet_result, sharesheet::SharesheetResult::kCancel);
}

IN_PROC_BROWSER_TEST_P(SharesheetAshBrowserTest, WindowClosed) {
  // No open window has this id.
  const std::string window_id = GenerateUniqueId();

  crosapi::mojom::IntentPtr intent = CreateIconIntent(profile());
  EXPECT_TRUE(
      IsIntentAcceptedByApp(intent, profile(), web_app::kSampleSystemWebAppId));
  sharesheet::SharesheetResult sharesheet_result =
      ShowBubble(window_id, std::move(intent), profile());
  EXPECT_EQ(sharesheet_result,
            sharesheet::SharesheetResult::kErrorWindowClosed);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_P(
    SharesheetAshBrowserTest,
    ::testing::Values(
        TestProfileParam({TestProfileType::kRegular,
                          web_app::test::CrosapiParam::kEnabled}),
        TestProfileParam({TestProfileType::kGuest,
                          web_app::test::CrosapiParam::kEnabled})));
