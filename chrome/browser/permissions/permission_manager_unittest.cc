// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permission_manager.h"

#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/pwc/test_support/test_pwc_permission_delegate.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/permission_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/permissions_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

class PermissionManagerTest : public base::test::WithFeatureOverride,
                              public ChromeRenderViewHostTestHarness {
 public:
  PermissionManagerTest()
      : base::test::WithFeatureOverride(
            content_settings::features::kApproximateGeolocationPermission) {}

  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/false);
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(GetBrowserContext()));
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  void OnPermissionChange(content::PermissionResult permission) {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
    callback_called_ = true;
    callback_result_ = permission.status;
  }

  bool callback_called() const { return callback_called_; }

  PermissionStatus callback_result() const { return callback_result_; }

  content::RenderFrameHost* AddChildRFH(
      content::RenderFrameHost* parent,
      const GURL& origin,
      network::mojom::PermissionsPolicyFeature feature =
          network::mojom::PermissionsPolicyFeature::kNotFound) {
    network::ParsedPermissionsPolicy frame_policy = {};
    if (feature != network::mojom::PermissionsPolicyFeature::kNotFound) {
      frame_policy.emplace_back(
          feature,
          std::vector{*network::OriginWithPossibleWildcards::FromOrigin(
              url::Origin::Create(origin))},
          /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false);
    }
    content::RenderFrameHost* result =
        content::RenderFrameHostTester::For(parent)->AppendChildWithPolicy(
            "", frame_policy);
    content::RenderFrameHostTester::For(result)
        ->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&result, origin);
    return result;
  }

  void SimulateNavigation(content::RenderFrameHost** rfh, const GURL& url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, *rfh);
    navigation_simulator->Commit();
    *rfh = navigation_simulator->GetFinalRenderFrameHost();
  }

  void SetPermission(const GURL& url,
                     ContentSettingsType content_settings_type,
                     PermissionStatus status) {
    permissions::PermissionsClient::Get()
        ->GetSettingsMap(GetBrowserContext())
        ->SetPermissionSettingDefaultScope(
            url, url, content_settings_type,
            content_settings::PermissionSettingsRegistry::GetInstance()
                ->Get(content_settings_type)
                ->delegate()
                .ToPermissionSetting(
                    permissions::PermissionUtil::
                        PermissionStatusToContentSetting(status)));
  }

 private:
  bool callback_called_ = false;
  PermissionStatus callback_result_ = PermissionStatus::ASK;
  base::OnceClosure quit_closure_;
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PermissionManagerTest);

TEST_P(PermissionManagerTest, GetCanonicalOriginSearch) {
  const GURL google_com("https://www.google.com");
  const GURL google_de("https://www.google.de");
  const GURL other_url("https://other.url");
  const GURL google_base = GURL(UIThreadSearchTermsData().GoogleBaseURLValue())
                               .DeprecatedGetOriginAsURL();
  const GURL remote_ntp = GURL(std::string("chrome-search://") +
                               chrome::kChromeSearchRemoteNtpHost);
  const GURL other_chrome_search = GURL("chrome-search://not-local-ntp");
  const GURL& top_level_ntp = chrome::ChromeUINewTabURLAsGURL();
  const GURL webui_ntp = chrome::ChromeUINewTabPageURLAsGURL();

  // "Normal" URLs are not affected by GetCanonicalOrigin.
  EXPECT_EQ(google_com, permissions::PermissionUtil::GetCanonicalOrigin(
                            content_settings::GeolocationContentSettingsType(),
                            google_com, google_com));
  EXPECT_EQ(google_de, permissions::PermissionUtil::GetCanonicalOrigin(
                           content_settings::GeolocationContentSettingsType(),
                           google_de, google_de));
  EXPECT_EQ(other_url, permissions::PermissionUtil::GetCanonicalOrigin(
                           content_settings::GeolocationContentSettingsType(),
                           other_url, other_url));
  EXPECT_EQ(google_base, permissions::PermissionUtil::GetCanonicalOrigin(
                             content_settings::GeolocationContentSettingsType(),
                             google_base, google_base));

  // The WebUI NTP URL gets mapped to the Google base URL.
  EXPECT_EQ(google_base, permissions::PermissionUtil::GetCanonicalOrigin(
                             content_settings::GeolocationContentSettingsType(),
                             webui_ntp, top_level_ntp));

  // chrome-search://remote-ntp and other URLs are not affected.
  EXPECT_EQ(remote_ntp, permissions::PermissionUtil::GetCanonicalOrigin(
                            content_settings::GeolocationContentSettingsType(),
                            remote_ntp, top_level_ntp));
  EXPECT_EQ(google_com, permissions::PermissionUtil::GetCanonicalOrigin(
                            content_settings::GeolocationContentSettingsType(),
                            google_com, top_level_ntp));
  EXPECT_EQ(other_chrome_search,
            permissions::PermissionUtil::GetCanonicalOrigin(
                content_settings::GeolocationContentSettingsType(),
                other_chrome_search, top_level_ntp));
}

TEST_P(PermissionManagerTest, GetCanonicalOriginPermissionDelegation) {
  const GURL requesting_origin("https://www.requesting.com");
  const GURL embedding_origin("https://www.google.de");

  // The embedding origin should be returned except in the case of notifications
  // and, if they're enabled, extensions.
  EXPECT_EQ(embedding_origin,
            permissions::PermissionUtil::GetCanonicalOrigin(
                content_settings::GeolocationContentSettingsType(),
                requesting_origin, embedding_origin));
  EXPECT_EQ(requesting_origin, permissions::PermissionUtil::GetCanonicalOrigin(
                                   ContentSettingsType::NOTIFICATIONS,
                                   requesting_origin, embedding_origin));
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  const GURL extensions_requesting_origin(
      "chrome-extension://abcdefghijklmnopqrstuvxyz");
  EXPECT_EQ(extensions_requesting_origin,
            permissions::PermissionUtil::GetCanonicalOrigin(
                content_settings::GeolocationContentSettingsType(),
                extensions_requesting_origin, embedding_origin));
#endif
}

TEST_P(PermissionManagerTest, SubscribeWithPermissionDelegation) {
  const char* kOrigin1 = "https://example.com";
  const char* kOrigin2 = "https://google.com";
  const GURL url1 = GURL(kOrigin1);
  const GURL url2 = GURL(kOrigin2);
  const auto geolocation_permission_descriptor = content::
      PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::GEOLOCATION);

  NavigateAndCommit(url1);
  content::RenderFrameHost* parent = main_rfh();
  content::RenderFrameHost* child = AddChildRFH(parent, url2);
  content::PermissionController* permission_controller =
      GetBrowserContext()->GetPermissionController();

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          permission_controller,
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  blink::PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, child, url2,
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionManagerTest::OnPermissionChange,
                              base::Unretained(this)));
  EXPECT_FALSE(callback_called());

  // Location should be blocked for the child because it's not delegated.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, child));

  // Allow access for the top level origin.
  SetPermission(url1, content_settings::GeolocationContentSettingsType(),
                PermissionStatus::GRANTED);

  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, parent));

  // The child's permission should still be block and no callback should be run.
  EXPECT_EQ(PermissionStatus::DENIED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, child));

  EXPECT_FALSE(callback_called());

  // Enabling geolocation by FP should allow the child to request access also.
  child = AddChildRFH(parent, url2,
                      network::mojom::PermissionsPolicyFeature::kGeolocation);

  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller->GetPermissionStatusForCurrentDocument(
                geolocation_permission_descriptor, child));

  permission_controller->UnsubscribeFromPermissionResultChange(subscription_id);
}

class PermissionManagerPwcTest : public ChromeRenderViewHostTestHarness {
 public:
  PermissionManagerPwcTest() : test_url_("https://pwc-test.example.com") {}

  void SetUp() override {
    scoped_pwc_feature_list_.InitAndEnableFeature(
        pwc::mojom::features::kPrivilegedWebContents);
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/false);
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(GetBrowserContext()));

    url::Origin test_origin = url::Origin::Create(test_url_);
    auto policy_delegate = std::make_unique<pwc::FixedPwcPolicyDelegate>(
        std::vector<url::Origin>{test_origin},
        std::vector<url::Origin>{test_origin});

    pwc_ = pwc::PrivilegedWebContents::Create(
        pwc::PrivilegedComponent::kTestComponent, profile(),
        std::move(policy_delegate));
    ASSERT_TRUE(pwc_);

    pwc_->SetPermissionDelegate(
        std::make_unique<pwc::TestPwcPermissionDelegate>());

    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        pwc_->web_contents(), test_url_);
  }

  void TearDown() override {
    pwc_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  content::RenderFrameHost* pwc_main_rfh() {
    return pwc_->web_contents()->GetPrimaryMainFrame();
  }

  pwc::TestPwcPermissionDelegate* delegate() {
    return static_cast<pwc::TestPwcPermissionDelegate*>(
        pwc_->permission_delegate());
  }

  content::PermissionController* permission_controller() {
    return GetBrowserContext()->GetPermissionController();
  }

  const GURL& test_url() const { return test_url_; }

 private:
  base::test::ScopedFeatureList scoped_pwc_feature_list_;
  GURL test_url_;
  std::unique_ptr<pwc::PrivilegedWebContents> pwc_;
};

TEST_F(PermissionManagerPwcTest, GetPermissionStatus_HonorsPwcDelegate) {
  auto mic_descriptor = content::PermissionDescriptorUtil::
      CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::AUDIO_CAPTURE);

  // 1. Granted by delegate.
  delegate()->set_result(
      content::PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                content::PermissionStatusSource::UNSPECIFIED));
  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller()->GetPermissionStatusForCurrentDocument(
                mic_descriptor, pwc_main_rfh()));

  // 2. Denied by delegate with custom source.
  delegate()->set_result(
      content::PermissionResult(blink::mojom::PermissionStatus::DENIED,
                                content::PermissionStatusSource::KILL_SWITCH));
  content::PermissionResult result =
      permission_controller()->GetPermissionResultForCurrentDocument(
          mic_descriptor, pwc_main_rfh());
  EXPECT_EQ(PermissionStatus::DENIED, result.status);
  EXPECT_EQ(content::PermissionStatusSource::KILL_SWITCH, result.source);
}

TEST_F(PermissionManagerPwcTest,
       GetPermissionStatus_UnhandledTypeDefaultsToDenied) {
  auto notifications_descriptor = content::PermissionDescriptorUtil::
      CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::NOTIFICATIONS);

  // Set standard tab permission to ALLOW for test_url in
  // HostContentSettingsMap.
  permissions::PermissionsClient::Get()
      ->GetSettingsMap(profile())
      ->SetContentSettingDefaultScope(test_url(), test_url(),
                                      ContentSettingsType::NOTIFICATIONS,
                                      CONTENT_SETTING_ALLOW);

  // Delegate returns nullopt (unhandled type).
  delegate()->set_result(std::nullopt);

  // PWC must default to DENIED with FEATURE_POLICY source, NOT leaking ALLOW
  // from tab settings.
  content::PermissionResult result =
      permission_controller()->GetPermissionResultForCurrentDocument(
          notifications_descriptor, pwc_main_rfh());
  EXPECT_EQ(PermissionStatus::DENIED, result.status);
  EXPECT_EQ(content::PermissionStatusSource::FEATURE_POLICY, result.source);
}

TEST_F(PermissionManagerPwcTest,
       RequestPermissions_ResolvesImmediatelyWithOverride) {
  auto mic_descriptor = content::PermissionDescriptorUtil::
      CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::AUDIO_CAPTURE);

  delegate()->set_result(
      content::PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                content::PermissionStatusSource::UNSPECIFIED));

  base::test::TestFuture<const std::vector<content::PermissionResult>&> future;
  content::PermissionRequestDescription description(
      mic_descriptor.Clone(), /*user_gesture=*/true, test_url());

  permission_controller()->RequestPermissionsFromCurrentDocument(
      pwc_main_rfh(), description, future.GetCallback());

  const std::vector<content::PermissionResult>& results = future.Get();
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(PermissionStatus::GRANTED, results[0].status);
}

TEST_F(PermissionManagerPwcTest,
       RequestPermissions_OriginMismatch_DoesNotOverride) {
  auto mic_descriptor = content::PermissionDescriptorUtil::
      CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::AUDIO_CAPTURE);

  delegate()->set_result(
      content::PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                content::PermissionStatusSource::UNSPECIFIED));

  base::test::TestFuture<const std::vector<content::PermissionResult>&> future;
  GURL mismatched_url("https://mismatch.example.com");
  content::PermissionRequestDescription description(
      mic_descriptor.Clone(), /*user_gesture=*/true, mismatched_url);

  permission_controller()->RequestPermissionsFromCurrentDocument(
      pwc_main_rfh(), description, future.GetCallback());

  // Because requesting_origin does not match the PWC RFH's last committed
  // origin, the PWC override does not apply.
  const std::vector<content::PermissionResult>& results = future.Get();
  ASSERT_EQ(1u, results.size());
  EXPECT_NE(PermissionStatus::GRANTED, results[0].status);
}

class PermissionManagerPwcGeolocationTest
    : public base::test::WithFeatureOverride,
      public PermissionManagerPwcTest {
 public:
  PermissionManagerPwcGeolocationTest()
      : base::test::WithFeatureOverride(
            content_settings::features::kApproximateGeolocationPermission) {}
};

TEST_P(PermissionManagerPwcGeolocationTest,
       GeolocationSubscription_PopulatesSettingWithoutCrash) {
  delegate()->set_result(
      content::PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                content::PermissionStatusSource::UNSPECIFIED));

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          permission_controller(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  blink::PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, pwc_main_rfh(), test_url(),
          /*should_include_device_status=*/false, base::DoNothing());

  EXPECT_TRUE(subscription_id);
  permission_controller()->UnsubscribeFromPermissionResultChange(
      subscription_id);
}

TEST_P(PermissionManagerPwcGeolocationTest,
       GetPermissionStatus_HonorsPwcDelegate) {
  auto geo_descriptor = content::PermissionDescriptorUtil::
      CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::GEOLOCATION);

  delegate()->set_result(
      content::PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                content::PermissionStatusSource::UNSPECIFIED));
  EXPECT_EQ(PermissionStatus::GRANTED,
            permission_controller()->GetPermissionStatusForCurrentDocument(
                geo_descriptor, pwc_main_rfh()));
}

TEST_P(PermissionManagerPwcGeolocationTest,
       RequestPermissions_ResolvesImmediatelyWithOverride) {
  auto geo_descriptor = content::PermissionDescriptorUtil::
      CreatePermissionDescriptorForPermissionType(
          blink::PermissionType::GEOLOCATION);

  delegate()->set_result(
      content::PermissionResult(blink::mojom::PermissionStatus::GRANTED,
                                content::PermissionStatusSource::UNSPECIFIED));

  base::test::TestFuture<const std::vector<content::PermissionResult>&> future;
  content::PermissionRequestDescription description(
      geo_descriptor.Clone(), /*user_gesture=*/true, test_url());

  permission_controller()->RequestPermissionsFromCurrentDocument(
      pwc_main_rfh(), description, future.GetCallback());

  const std::vector<content::PermissionResult>& results = future.Get();
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(PermissionStatus::GRANTED, results[0].status);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PermissionManagerPwcGeolocationTest);
