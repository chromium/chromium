// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/with_feature_override.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/permission_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/permissions_test_utils.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

using blink::PermissionType;
using network::mojom::PermissionsPolicyFeature;

// This class tests PermissionStatus.onChange observer.
class PermissionSubscriptionTest : public ChromeRenderViewHostTestHarness {
 public:
  void OnPermissionChange(content::PermissionResult permission_result) {
    if (!quit_closure_.is_null()) {
      std::move(quit_closure_).Run();
    }
    callback_called_ = true;
    callback_count_++;
    callback_result_ = permission_result.status;
  }

 protected:
  PermissionSubscriptionTest()
      : url_(url::Origin::Create(GURL("https://example.com"))),
        other_url_(url::Origin::Create(GURL("https://foo.com"))) {}

  permissions::PermissionManager* GetPermissionManager() {
    return static_cast<permissions::PermissionManager*>(
        profile()->GetPermissionControllerDelegate());
  }

  content::PermissionController* GetPermissionController() {
    return GetBrowserContext()->GetPermissionController();
  }

  HostContentSettingsMap* GetHostContentSettingsMap() {
    return permissions::PermissionsClient::Get()->GetSettingsMap(
        GetBrowserContext());
  }

  void CheckPermissionStatus(blink::PermissionType type,
                             PermissionStatus expected) {
    EXPECT_EQ(expected,
              GetPermissionController()
                  ->GetPermissionResultForOriginWithoutContext(
                      content::PermissionDescriptorUtil::
                          CreatePermissionDescriptorForPermissionType(type),
                      url_, url_)
                  .status);
  }

  void SetPermission(blink::PermissionType permission,
                     PermissionStatus status) {
    SetPermission(url_.GetURL(), url_.GetURL(), permission, status);
  }

  void SetPermission(const GURL& url,
                     blink::PermissionType permission,
                     PermissionStatus status) {
    SetPermission(url, url, permission, status);
  }

  void SetPermission(const GURL& requesting_origin,
                     const GURL& embedding_origin,
                     blink::PermissionType permission,
                     PermissionStatus status) {
    ContentSettingsType content_settings_type =
        permissions::PermissionUtil::PermissionTypeToContentSettingsType(
            permission);
    GetHostContentSettingsMap()->SetPermissionSettingDefaultScope(
        requesting_origin, embedding_origin, content_settings_type,
        content_settings::PermissionSettingsRegistry::GetInstance()
            ->Get(content_settings_type)
            ->delegate()
            .ToPermissionSetting(
                permissions::PermissionUtil::PermissionStatusToContentSetting(
                    status)));
  }

  PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host) {
    return GetPermissionController()->GetPermissionStatusForCurrentDocument(
        content::PermissionDescriptorUtil::
            CreatePermissionDescriptorForPermissionType(permission),
        render_frame_host);
  }

  const GURL url() const { return url_.GetURL(); }

  const GURL other_url() const { return other_url_.GetURL(); }

  bool callback_called() const { return callback_called_; }

  int callback_count() const { return callback_count_; }

  PermissionStatus callback_result() const { return callback_result_; }

  void Reset() {
    callback_called_ = false;
    callback_count_ = 0;
    callback_result_ = PermissionStatus::ASK;
  }

  content::RenderFrameHost* AddChildRFH(
      content::RenderFrameHost* parent,
      const GURL& origin,
      PermissionsPolicyFeature feature = PermissionsPolicyFeature::kNotFound) {
    network::ParsedPermissionsPolicy frame_policy = {};
    if (feature != PermissionsPolicyFeature::kNotFound) {
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

 private:
  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/false);
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(GetBrowserContext()));
    NavigateAndCommit(url());
  }

  void TearDown() override {
    GetPermissionManager()->Shutdown();
    RenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  const url::Origin url_;
  const url::Origin other_url_;
  bool callback_called_ = false;
  int callback_count_ = 0;
  PermissionStatus callback_result_ = PermissionStatus::ASK;
  base::OnceClosure quit_closure_;
};

class PermissionSubscriptionGeolocationTest
    : public base::test::WithFeatureOverride,
      public PermissionSubscriptionTest {
 public:
  PermissionSubscriptionGeolocationTest()
      : base::test::WithFeatureOverride(
            content_settings::features::kApproximateGeolocationPermission) {}
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PermissionSubscriptionGeolocationTest);

TEST_P(PermissionSubscriptionGeolocationTest,
       SubscriptionDestroyedCleanlyWithoutUnsubscribe) {
  // Test that the PermissionManager shuts down cleanly with subscriptions that
  // haven't been removed, crbug.com/40519661.
  content::SubscribeToPermissionResultChange(
      GetPermissionController(),
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              PermissionType::GEOLOCATION),
      /*render_process_host=*/nullptr, main_rfh(), url(),
      /*should_include_device_status=*/false,
      base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                          base::Unretained(this)));
}

TEST_P(PermissionSubscriptionGeolocationTest,
       SubscribeUnsubscribeAfterShutdown) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  // Simulate Keyed Services shutdown pass. Note: Shutdown will be called second
  // time during browser_context destruction. This is ok for now: Shutdown is
  // reenterant.
  GetPermissionManager()->Shutdown();

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);

  // Check that subscribe/unsubscribe after shutdown don't crash.
  content::PermissionController::SubscriptionId subscription2_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription2_id);
}

TEST_P(PermissionSubscriptionGeolocationTest, SameTypeChangeNotifies) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest,
       DifferentTypeChangeDoesNotNotify) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::NOTIFICATIONS, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest,
       ChangeAfterUnsubscribeDoesNotNotify) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());
}

TEST_P(PermissionSubscriptionGeolocationTest,
       ChangeAfterUnsubscribeOnlyNotifiesActiveSubscribers) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  content::SubscribeToPermissionResultChange(
      GetPermissionController(),
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              PermissionType::GEOLOCATION),
      /*render_process_host=*/nullptr, main_rfh(), url(),
      /*should_include_device_status=*/false,
      base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                          base::Unretained(this)));

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_EQ(callback_count(), 1);
}

TEST_P(PermissionSubscriptionGeolocationTest,
       DifferentPrimaryUrlDoesNotNotify) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(other_url(), url(), PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, DifferentSecondaryUrlDoesNotNotify) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::STORAGE_ACCESS_GRANT),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(url(), other_url(), PermissionType::STORAGE_ACCESS_GRANT,
                PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest, WildCardPatternNotifies) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetDefaultPermissionSetting(
      content_settings::GeolocationContentSettingsType(),
      content_settings::PermissionSettingsRegistry::GetInstance()
          ->Get(content_settings::GeolocationContentSettingsType())
          ->delegate()
          .ToPermissionSetting(CONTENT_SETTING_ALLOW));

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest, ClearSettingsNotifies) {
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->ClearSettingsForOneType(
      content_settings::GeolocationContentSettingsType());

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest, NewValueCorrectlyPassed) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::DENIED);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest,
       ChangeWithoutPermissionChangeDoesNotNotify) {
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest, ChangesBackAndForth) {
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::ASK);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  Reset();

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest, ChangesBackAndForthWorker) {
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::ASK);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          process(),
          /*render_frame_host=*/nullptr, url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  Reset();

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::ASK);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, SubscribeMIDIPermission) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(PermissionType::MIDI),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::ASK);
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);
  CheckPermissionStatus(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest,
       SubscribeWithPermissionDelegation) {
  NavigateAndCommit(url());
  content::RenderFrameHost* parent = main_rfh();
  content::RenderFrameHost* child = AddChildRFH(parent, other_url());

  // Allow access for the top level origin.
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  // Enabling geolocation by FP should allow the child to request access also.
  child =
      AddChildRFH(parent, other_url(), PermissionsPolicyFeature::kGeolocation);

  EXPECT_EQ(PermissionStatus::GRANTED, GetPermissionStatusForCurrentDocument(
                                           PermissionType::GEOLOCATION, child));

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, child, other_url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));
  EXPECT_FALSE(callback_called());

  // Blocking access to the parent should trigger the callback to be run for the
  // child also.
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::DENIED);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  EXPECT_EQ(PermissionStatus::DENIED, GetPermissionStatusForCurrentDocument(
                                          PermissionType::GEOLOCATION, child));

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest,
       SubscribeUnsubscribeAndResubscribe) {
  NavigateAndCommit(url());

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));
  EXPECT_EQ(callback_count(), 0);

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_EQ(callback_count(), 1);
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);

  // ensure no callbacks are received when unsubscribed.
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::DENIED);
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_EQ(callback_count(), 1);

  content::PermissionController::SubscriptionId subscription_id_2 =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));
  EXPECT_EQ(callback_count(), 1);

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::DENIED);

  EXPECT_EQ(callback_count(), 2);
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id_2);
}

TEST_P(PermissionSubscriptionGeolocationTest,
       SubscribersAreNotifedOfEmbargoEvents) {
  NavigateAndCommit(url());

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));
  EXPECT_EQ(callback_count(), 0);

  auto* autoblocker =
      permissions::PermissionsClient::Get()->GetPermissionDecisionAutoBlocker(
          GetBrowserContext());

  // 3 dismisses will trigger embargo, which should call the subscription
  // callback.
  autoblocker->RecordDismissAndEmbargo(
      url(), content_settings::GeolocationContentSettingsType(),
      /*dismissed_prompt_was_quiet=*/false);
  EXPECT_EQ(callback_count(), 0);
  autoblocker->RecordDismissAndEmbargo(
      url(), content_settings::GeolocationContentSettingsType(),
      /*dismissed_prompt_was_quiet=*/false);
  EXPECT_EQ(callback_count(), 0);
  autoblocker->RecordDismissAndEmbargo(
      url(), content_settings::GeolocationContentSettingsType(),
      /*dismissed_prompt_was_quiet=*/false);
  EXPECT_EQ(callback_count(), 1);

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest,
       RequestableDevicePermissionChangesLazilyNotifiesObservers) {
  // Setup the initial state.
  SetPermission(url(), url(), PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  permissions::PermissionContextBase* geolocation_permission_context =
      GetPermissionManager()->GetPermissionContextForTesting(
          content_settings::GeolocationContentSettingsType());
  geolocation_permission_context->set_has_device_permission_for_test(true);
  geolocation_permission_context->set_can_request_device_permission_for_test(
      true);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/true,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  // Change device permission to denied. At this point we have not yet retrieved
  // the permission status so the device permission change has not been
  // detected.
  geolocation_permission_context->set_has_device_permission_for_test(false);
  EXPECT_FALSE(callback_called());

  // This call will trigger an update of the device permission status and
  // observers will be notified.
  GetPermissionController()->GetPermissionResultForCurrentDocument(
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              PermissionType::GEOLOCATION),
      main_rfh());

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());
  Reset();

  // Changing the permission status triggers a callback.
  SetPermission(url(), url(), PermissionType::GEOLOCATION,
                PermissionStatus::DENIED);
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());
  Reset();

  // We now reset to a granted state, which should move the overall permission
  // status to ask (origin status "granted", but Chrome does not have the device
  // permission).
  SetPermission(url(), url(), PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());
  Reset();

  // Now we reset the device permission status back to granted, which moves the
  // overall permission status to granted. At this point we have not yet
  // refreshed the permission status.
  geolocation_permission_context->set_has_device_permission_for_test(true);
  EXPECT_FALSE(callback_called());

  // This call will make us retrieve the device permission status and observers
  // will be notified.
  GetPermissionController()->GetPermissionResultForCurrentDocument(
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              PermissionType::GEOLOCATION),
      main_rfh());

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  // Cleanup.
  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_P(PermissionSubscriptionGeolocationTest,
       NonrequestableDevicePermissionChangesLazilyNotifiesObservers) {
  // Setup the initial state.
  SetPermission(url(), url(), PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  permissions::PermissionContextBase* geolocation_permission_context =
      GetPermissionManager()->GetPermissionContextForTesting(
          content_settings::GeolocationContentSettingsType());
  geolocation_permission_context->set_can_request_device_permission_for_test(
      false);
  geolocation_permission_context->set_has_device_permission_for_test(true);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/true,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  // Change device status. At this point the device permission has not been
  // queried yet.
  geolocation_permission_context->set_has_device_permission_for_test(false);
  EXPECT_FALSE(callback_called());

  // Get permission status to also trigger the device permission being queried
  // which would result in observers being notified.
  GetPermissionController()->GetPermissionResultForCurrentDocument(
      content::PermissionDescriptorUtil::
          CreatePermissionDescriptorForPermissionType(
              PermissionType::GEOLOCATION),
      main_rfh());

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());
  Reset();

  // Since the device level permission has changed, this will trigger a
  // notification to observers.
  geolocation_permission_context->set_has_device_permission_for_test(true);
  SetPermission(url(), url(), PermissionType::GEOLOCATION,
                PermissionStatus::DENIED);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());
  Reset();

  // Change permission to granted and observe change.
  SetPermission(url(), url(), PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());
  Reset();

  // Cleanup.
  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest,
       SubscribeUnsubscribeForNotAddedPermissionContext) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::TOP_LEVEL_STORAGE_ACCESS),
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}

// TODO(https://crbug.com/359831269): Fix new tab page test for Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SubscribeUnsubscribeForNewTabPage \
  DISABLED_SubscribeUnsubscribeForNewTabPage
#else
#define MAYBE_SubscribeUnsubscribeForNewTabPage \
  SubscribeUnsubscribeForNewTabPage
#endif
TEST_P(PermissionSubscriptionGeolocationTest,
       MAYBE_SubscribeUnsubscribeForNewTabPage) {
  NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
            main_rfh()->GetLastCommittedOrigin().GetURL());
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionResultChange(
          GetPermissionController(),
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  PermissionType::GEOLOCATION),
          /*render_process_host=*/nullptr, main_rfh(),
          GURL(chrome::kChromeUINewTabPageThirdPartyURL),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
                PermissionType::GEOLOCATION, PermissionStatus::GRANTED);
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  SetPermission(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
                PermissionType::GEOLOCATION, PermissionStatus::ASK);
  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionResultChange(subscription_id);
}
