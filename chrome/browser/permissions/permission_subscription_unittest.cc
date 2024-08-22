// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/permissions/test/permission_test_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/permissions_test_utils.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/origin.h"

using blink::PermissionType;
using blink::mojom::PermissionsPolicyFeature;

// This class tests PermissionStatus.onChange observer.
class PermissionSubscriptionTest : public ChromeRenderViewHostTestHarness {
 public:
  void OnPermissionChange(PermissionStatus permission) {
    if (!quit_closure_.is_null()) {
      std::move(quit_closure_).Run();
    }
    callback_called_ = true;
    callback_count_++;
    callback_result_ = permission;
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
                  ->GetPermissionResultForOriginWithoutContext(type, url_, url_)
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
    GetHostContentSettingsMap()->SetContentSettingDefaultScope(
        requesting_origin, embedding_origin,
        permissions::PermissionUtil::PermissionTypeToContentSettingType(
            permission),
        permissions::PermissionUtil::PermissionStatusToContentSetting(status));
  }

  PermissionStatus GetPermissionStatusForCurrentDocument(
      blink::PermissionType permission,
      content::RenderFrameHost* render_frame_host) {
    return GetPermissionController()->GetPermissionStatusForCurrentDocument(
        permission, render_frame_host);
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
    blink::ParsedPermissionsPolicy frame_policy = {};
    if (feature != PermissionsPolicyFeature::kNotFound) {
      frame_policy.emplace_back(
          feature,
          std::vector{*blink::OriginWithPossibleWildcards::FromOrigin(
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
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(GetBrowserContext()));
    NavigateAndCommit(url());
  }

  void TearDown() override {
    GetPermissionManager()->Shutdown();
    RenderViewHostTestHarness::TearDown();
  }

  const url::Origin url_;
  const url::Origin other_url_;
  bool callback_called_ = false;
  int callback_count_ = 0;
  PermissionStatus callback_result_ = PermissionStatus::ASK;
  base::OnceClosure quit_closure_;
};

TEST_F(PermissionSubscriptionTest,
       SubscriptionDestroyedCleanlyWithoutUnsubscribe) {
  // Test that the PermissionManager shuts down cleanly with subscriptions that
  // haven't been removed, crbug.com/720071.
  content::SubscribeToPermissionStatusChange(
      GetPermissionController(), PermissionType::GEOLOCATION,
      /*render_process_host=*/nullptr, main_rfh(), url(),
      /*should_include_device_status=*/false,
      base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                          base::Unretained(this)));
}

TEST_F(PermissionSubscriptionTest, SubscribeUnsubscribeAfterShutdown) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
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
      ->UnsubscribeFromPermissionStatusChange(subscription_id);

  // Check that subscribe/unsubscribe after shutdown don't crash.
  content::PermissionController::SubscriptionId subscription2_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription2_id);
}

TEST_F(PermissionSubscriptionTest, SameTypeChangeNotifies) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, DifferentTypeChangeDoesNotNotify) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::NOTIFICATIONS, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, ChangeAfterUnsubscribeDoesNotNotify) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());
}

TEST_F(PermissionSubscriptionTest,
       ChangeAfterUnsubscribeOnlyNotifiesActiveSubscribers) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  content::SubscribeToPermissionStatusChange(
      GetPermissionController(), PermissionType::GEOLOCATION,
      /*render_process_host=*/nullptr, main_rfh(), url(),
      /*should_include_device_status=*/false,
      base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                          base::Unretained(this)));

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_EQ(callback_count(), 1);
}

TEST_F(PermissionSubscriptionTest, DifferentPrimaryUrlDoesNotNotify) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(other_url(), url(), PermissionType::GEOLOCATION,
                PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, DifferentSecondaryUrlDoesNotNotify) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::STORAGE_ACCESS_GRANT,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(url(), other_url(), PermissionType::STORAGE_ACCESS_GRANT,
                PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, WildCardPatternNotifies) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->SetDefaultContentSetting(
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::GRANTED, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, ClearSettingsNotifies) {
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetHostContentSettingsMap()->ClearSettingsForOneType(
      ContentSettingsType::GEOLOCATION);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::ASK, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, NewValueCorrectlyPassed) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::DENIED);

  EXPECT_TRUE(callback_called());
  EXPECT_EQ(PermissionStatus::DENIED, callback_result());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, ChangeWithoutPermissionChangeDoesNotNotify) {
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_FALSE(callback_called());

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, ChangesBackAndForth) {
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::ASK);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
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
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, ChangesBackAndForthWorker) {
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::ASK);

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION, process(),
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
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, SubscribeMIDIPermission) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::MIDI,
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
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, SubscribeWithPermissionDelegation) {
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
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
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
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

TEST_F(PermissionSubscriptionTest, SubscribeUnsubscribeAndResubscribe) {
  NavigateAndCommit(url());

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
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
      ->UnsubscribeFromPermissionStatusChange(subscription_id);

  // ensure no callbacks are received when unsubscribed.
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::DENIED);
  SetPermission(PermissionType::GEOLOCATION, PermissionStatus::GRANTED);

  EXPECT_EQ(callback_count(), 1);

  content::PermissionController::SubscriptionId subscription_id_2 =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
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
      ->UnsubscribeFromPermissionStatusChange(subscription_id_2);
}

TEST_F(PermissionSubscriptionTest, SubscribersAreNotifedOfEmbargoEvents) {
  NavigateAndCommit(url());

  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
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
  autoblocker->RecordDismissAndEmbargo(url(), ContentSettingsType::GEOLOCATION,
                                       /*dismissed_prompt_was_quiet=*/false);
  EXPECT_EQ(callback_count(), 0);
  autoblocker->RecordDismissAndEmbargo(url(), ContentSettingsType::GEOLOCATION,
                                       /*dismissed_prompt_was_quiet=*/false);
  EXPECT_EQ(callback_count(), 0);
  autoblocker->RecordDismissAndEmbargo(url(), ContentSettingsType::GEOLOCATION,
                                       /*dismissed_prompt_was_quiet=*/false);
  EXPECT_EQ(callback_count(), 1);

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

// TODO(b/339158416): Add back
// RequestableDevicePermissionChangesLazilyNotifiesObservers and
// NonrequestableDevicePermissionChangesLazilyNotifiesObservers tests once the
// implementation is finished.

TEST_F(PermissionSubscriptionTest,
       SubscribeUnsubscribeForNotAddedPermissionContext) {
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::TOP_LEVEL_STORAGE_ACCESS,
          /*render_process_host=*/nullptr, main_rfh(), url(),
          /*should_include_device_status=*/false,
          base::BindRepeating(&PermissionSubscriptionTest::OnPermissionChange,
                              base::Unretained(this)));

  GetBrowserContext()
      ->GetPermissionController()
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}

// TODO(https://crbug.com/359831269): Fix new tab page test for Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_SubscribeUnsubscribeForNewTabPage \
  DISABLED_SubscribeUnsubscribeForNewTabPage
#else
#define MAYBE_SubscribeUnsubscribeForNewTabPage \
  SubscribeUnsubscribeForNewTabPage
#endif
TEST_F(PermissionSubscriptionTest, MAYBE_SubscribeUnsubscribeForNewTabPage) {
  NavigateAndCommit(GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
            main_rfh()->GetLastCommittedOrigin().GetURL());
  content::PermissionController::SubscriptionId subscription_id =
      content::SubscribeToPermissionStatusChange(
          GetPermissionController(), PermissionType::GEOLOCATION,
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
      ->UnsubscribeFromPermissionStatusChange(subscription_id);
}
