// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/top_level_storage_access_api/top_level_storage_access_permission_context.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {

constexpr char kRequestOutcomeHistogram[] =
    "API.TopLevelStorageAccess.RequestOutcome";

GURL GetTopLevelURL() {
  return GURL("https://embedder.example.com");
}

GURL GetRequesterURL() {
  return GURL("https://requester.example.com");
}

GURL GetDummyEmbeddingUrl() {
  return GURL("https://example_embedder.com");
}

}  // namespace

class TopLevelStorageAccessPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 public:
  explicit TopLevelStorageAccessPermissionContextTest(bool saa_enabled) {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (saa_enabled) {
      enabled.push_back(blink::features::kStorageAccessAPIForOriginExtension);
    } else {
      disabled.push_back(blink::features::kStorageAccessAPIForOriginExtension);
    }
    features_.InitWithFeatures(enabled, disabled);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Ensure we are navigated to some page so that the proper views get setup.
    NavigateAndCommit(GetTopLevelURL());

    // Create PermissionRequestManager.
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());

    mock_permission_prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(
            permissions::PermissionRequestManager::FromWebContents(
                web_contents()));
  }

  void TearDown() override {
    mock_permission_prompt_factory_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  permissions::PermissionRequestID CreateFakeID() {
    return permissions::PermissionRequestID(
        web_contents()->GetPrimaryMainFrame(),
        request_id_generator_.GenerateNextId());
  }

 private:
  base::test::ScopedFeatureList features_;
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
  permissions::PermissionRequestID::RequestLocalId::Generator
      request_id_generator_;
};

class TopLevelStorageAccessPermissionContextTestAPIDisabledTest
    : public TopLevelStorageAccessPermissionContextTest {
 public:
  TopLevelStorageAccessPermissionContextTestAPIDisabledTest()
      : TopLevelStorageAccessPermissionContextTest(false) {}
};

TEST_F(TopLevelStorageAccessPermissionContextTestAPIDisabledTest,
       InsecureOriginsAreDisallowed) {
  GURL insecure_url = GURL("http://www.example.com");
  TopLevelStorageAccessPermissionContext permission_context(profile());
  EXPECT_FALSE(permission_context.IsPermissionAvailableToOrigins(insecure_url,
                                                                 insecure_url));
  EXPECT_FALSE(permission_context.IsPermissionAvailableToOrigins(
      insecure_url, GetRequesterURL()));
}

// When the Storage Access API feature is disabled (the default) we
// should block the permission request.
TEST_F(TopLevelStorageAccessPermissionContextTestAPIDisabledTest,
       PermissionBlocked) {
  TopLevelStorageAccessPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, future.Get());
}

TEST_F(TopLevelStorageAccessPermissionContextTestAPIDisabledTest,
       PermissionStatusBlocked) {
  TopLevelStorageAccessPermissionContext permission_context(profile());

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetTopLevelURL())
                .content_setting);
}

class TopLevelStorageAccessPermissionContextTestAPIEnabledTest
    : public TopLevelStorageAccessPermissionContextTest {
 public:
  TopLevelStorageAccessPermissionContextTestAPIEnabledTest()
      : TopLevelStorageAccessPermissionContextTest(true) {}

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

// No user gesture should force a permission rejection.
TEST_F(TopLevelStorageAccessPermissionContextTestAPIEnabledTest,
       PermissionDeniedWithoutUserGesture) {
  TopLevelStorageAccessPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/false, future.GetCallback());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, future.Get());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram,
                TopLevelStorageAccessRequestOutcome::kDeniedByPrerequisites),
            1);
}

TEST_F(TopLevelStorageAccessPermissionContextTestAPIEnabledTest,
       PermissionStatusAsksWhenFeatureEnabled) {
  TopLevelStorageAccessPermissionContext permission_context(profile());

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetTopLevelURL())
                .content_setting);
}

class TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest
    : public TopLevelStorageAccessPermissionContextTestAPIEnabledTest {
 public:
  TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kFirstPartySets,
            blink::features::kStorageAccessAPIForOriginExtension,
        },
        /*disabled_features=*/{});
  }
  void SetUp() override {
    TopLevelStorageAccessPermissionContextTestAPIEnabledTest::SetUp();

    // Create a FPS with https://requester.example.com as the member and
    // https://embedder.example.com as the primary.
    first_party_sets_handler_.SetGlobalSets(net::GlobalFirstPartySets(
        base::Version("1.2.3"),
        /*entries=*/
        {{net::SchemefulSite(GetRequesterURL()),
          {net::FirstPartySetEntry(net::SchemefulSite(GetTopLevelURL()),
                                   net::SiteType::kAssociated, 0)}}},
        /*aliases=*/{}));
  }

 private:
  base::test::ScopedFeatureList features_;
  first_party_sets::ScopedMockFirstPartySetsHandler first_party_sets_handler_;
};

TEST_F(TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_AutograntedWithinFPS) {
  TopLevelStorageAccessPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::NonRestorableUserSession` setting exists yet.
  ContentSettingsForOneType non_restorable_grants =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
          content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(0u, non_restorable_grants.size());

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  EXPECT_EQ(CONTENT_SETTING_ALLOW, future.Get());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram,
                TopLevelStorageAccessRequestOutcome::kGrantedByFirstPartySet),
            1);

  // Check the `SessionModel::NonRestorableUserSession` settings granted by FPS.
  non_restorable_grants = settings_map->GetSettingsForOneType(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(1u, non_restorable_grants.size());
}

TEST_F(TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_CrossSiteFrameQueryStillAsk) {
  // First, grant the permission based on FPS membership.
  TopLevelStorageAccessPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::NonRestorableUserSession` setting exists yet.
  ContentSettingsForOneType non_restorable_grants =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
          content_settings::SessionModel::NonRestorableUserSession);
  ASSERT_EQ(0u, non_restorable_grants.size());

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  EXPECT_EQ(CONTENT_SETTING_ALLOW, future.Get());

  // Check the `SessionModel::NonRestorableUserSession` settings granted by FPS.
  non_restorable_grants = settings_map->GetSettingsForOneType(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(1u, non_restorable_grants.size());

  // Next, set up a cross-site frame.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  content::RenderFrameHost* navigated_subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GetDummyEmbeddingUrl(), subframe);

  // Even though the permission is granted, queries from cross-site frames
  // should return the default value.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context
                .GetPermissionStatus(navigated_subframe, GetRequesterURL(),
                                     GetTopLevelURL())
                .content_setting);
}

TEST_F(TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_AutodeniedOutsideFPS) {
  TopLevelStorageAccessPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::NonRestorableUserSession` setting exists yet.
  ContentSettingsForOneType non_restorable_grants =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
          content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(0u, non_restorable_grants.size());

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetDummyEmbeddingUrl(),
      /*user_gesture=*/true, future.GetCallback());

  EXPECT_EQ(CONTENT_SETTING_BLOCK, future.Get());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram,
                TopLevelStorageAccessRequestOutcome::kDeniedByFirstPartySet),
            1);

  // Check the `SessionModel::NonRestorableUserSession` settings.
  // None were granted, and implicit denials are not currently persisted, which
  // preserves the default `ASK` setting.
  non_restorable_grants = settings_map->GetSettingsForOneType(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(0u, non_restorable_grants.size());
}

TEST_F(TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_DenialQueryStillAsk) {
  TopLevelStorageAccessPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::NonRestorableUserSession` setting exists yet.
  ContentSettingsForOneType non_restorable_grants =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
          content_settings::SessionModel::NonRestorableUserSession);
  ASSERT_EQ(0u, non_restorable_grants.size());

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetDummyEmbeddingUrl(),
      /*user_gesture=*/true, future.GetCallback());

  EXPECT_EQ(CONTENT_SETTING_BLOCK, future.Get());

  // Check the `SessionModel::NonRestorableUserSession` settings.
  // None were granted, and implicit denials are not currently persisted, which
  // preserves the default `ASK` setting.
  non_restorable_grants = settings_map->GetSettingsForOneType(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(0u, non_restorable_grants.size());

  // The permission denial should not be exposed via query. Note that the block
  // setting is not persisted anyway with the current implementation; this is a
  // forward-looking test.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetDummyEmbeddingUrl())
                .content_setting);
}

class TopLevelStorageAccessPermissionContextAPIFirstPartySetsDisabledTest
    : public TopLevelStorageAccessPermissionContextTestAPIEnabledTest {
 public:
  TopLevelStorageAccessPermissionContextAPIFirstPartySetsDisabledTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kStorageAccessAPIForOriginExtension},
        /*disabled_features=*/{features::kFirstPartySets});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(TopLevelStorageAccessPermissionContextAPIFirstPartySetsDisabledTest,
       PermissionDeniedWithFPSDisabled) {
  TopLevelStorageAccessPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::NonRestorableUserSession` setting exists yet.
  ContentSettingsForOneType non_restorable_grants =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
          content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(0u, non_restorable_grants.size());

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  EXPECT_EQ(CONTENT_SETTING_BLOCK, future.Get());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram,
                TopLevelStorageAccessRequestOutcome::kDeniedByPrerequisites),
            1);

  // Check the `SessionModel::NonRestorableUserSession` settings granted by FPS.
  non_restorable_grants = settings_map->GetSettingsForOneType(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
      content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(0u, non_restorable_grants.size());
}
