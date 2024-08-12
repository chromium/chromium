// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "components/location/android/location_settings_dialog_outcome.h"
#include "components/location/android/mock_location_settings.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#endif

#if BUILDFLAG(IS_ANDROID)
namespace {
constexpr char kDSETestUrl[] = "https://www.dsetest.com";

class TestSearchEngineDelegate
    : public SearchPermissionsService::SearchEngineDelegate {
 public:
  std::u16string GetDSEName() override { return std::u16string(); }

  url::Origin GetDSEOrigin() override {
    return url::Origin::Create(GURL(kDSETestUrl));
  }
};
}  // namespace
#endif

class GeolocationPermissionContextDelegateTests
    : public ChromeRenderViewHostTestHarness {
 protected:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));
#if BUILDFLAG(IS_ANDROID)
    static_cast<permissions::GeolocationPermissionContextAndroid*>(
        PermissionManagerFactory::GetForProfile(profile())
            ->GetPermissionContextForTesting(ContentSettingsType::GEOLOCATION))
        ->SetLocationSettingsForTesting(
            std::make_unique<MockLocationSettings>());
    MockLocationSettings::SetLocationStatus(
        /*has_android_coarse_location_permission=*/true,
        /*has_android_fine_location_permission=*/true,
        /*is_system_location_setting_enabled=*/true);
    MockLocationSettings::SetCanPromptForAndroidPermission(true);
    MockLocationSettings::SetLocationSettingsDialogStatus(false /* enabled */,
                                                          GRANTED);
    MockLocationSettings::ClearHasShownLocationSettingsDialog();
#endif
  }

  void RequestPermissionFromCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host,
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
    PermissionManagerFactory::GetForProfile(profile())
        ->RequestPermissionsFromCurrentDocument(
            render_frame_host,
            content::PermissionRequestDescription(permission, user_gesture),
            base::BindOnce(
                [](base::OnceCallback<void(blink::mojom::PermissionStatus)>
                       callback,
                   const std::vector<blink::mojom::PermissionStatus>& state) {
                  DCHECK_EQ(state.size(), 1U);
                  std::move(callback).Run(state[0]);
                },
                std::move(callback)));
  }

  content::PermissionResult GetPermissionResultForOriginWithoutContext(
      Profile* profile,
      blink::PermissionType permission,
      const url::Origin& origin) {
    return PermissionManagerFactory::GetForProfile(profile)
        ->GetPermissionResultForOriginWithoutContext(permission, origin,
                                                     origin);
  }
};

TEST_F(GeolocationPermissionContextDelegateTests, TabContentSettingIsUpdated) {
  GURL requesting_frame_url("https://www.example.com/geolocation");
  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  permissions::MockPermissionPromptFactory mock_prompt_factory(manager);
  NavigateAndCommit(requesting_frame_url);
  manager->DocumentOnLoadCompletedInPrimaryMainFrame();

  base::RunLoop run_loop;
  RequestPermissionFromCurrentDocument(
      blink::PermissionType::GEOLOCATION, main_rfh(), true,
      base::BindOnce(
          [](base::RunLoop* run_loop, blink::mojom::PermissionStatus status) {
            EXPECT_EQ(status, blink::mojom::PermissionStatus::GRANTED);
            run_loop->Quit();
          },
          &run_loop));
  task_environment()->RunUntilIdle();
  ASSERT_TRUE(manager->IsRequestInProgress());
  manager->Accept();
  run_loop.Run();
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  EXPECT_TRUE(
      content_settings->IsContentAllowed(ContentSettingsType::GEOLOCATION));
}

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/40835241): Flaky.
TEST_F(GeolocationPermissionContextDelegateTests,
       DISABLED_SearchGeolocationInIncognito) {
  url::Origin requesting_frame_url = url::Origin::Create(GURL(kDSETestUrl));

  SearchPermissionsService* service =
      SearchPermissionsService::Factory::GetForBrowserContext(profile());
  std::unique_ptr<TestSearchEngineDelegate> delegate =
      std::make_unique<TestSearchEngineDelegate>();
  service->SetSearchEngineDelegateForTest(std::move(delegate));
  service->InitializeSettingsIfNeeded();

  // The DSE geolocation should not be auto-granted even in a non-OTR profile.
  ASSERT_EQ(
      blink::mojom::PermissionStatus::ASK,
      GetPermissionResultForOriginWithoutContext(
          profile(), blink::PermissionType::GEOLOCATION, requesting_frame_url)
          .status);

  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // The DSE geolocation should not be auto-granted in an OTR profile.
  ASSERT_EQ(
      blink::mojom::PermissionStatus::ASK,
      GetPermissionResultForOriginWithoutContext(
          otr_profile, blink::PermissionType::GEOLOCATION, requesting_frame_url)
          .status);
}
#endif
