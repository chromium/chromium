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

using testing::Contains;
using testing::Each;
using PermissionStatus = blink::mojom::PermissionStatus;

constexpr char kRequestOutcomeHistogram[] =
    "API.TopLevelStorageAccess.RequestOutcome";

MATCHER_P(DecidedByRelatedWebsiteSets, inner, "") {
  return testing::ExplainMatchResult(
      inner, arg.metadata.decided_by_related_website_sets(), result_listener);
}

GURL GetTopLevelURL() {
  return GURL("https://embedder.example.com");
}

GURL GetRequesterURL() {
  return GURL("https://requester.com");
}

GURL GetDummyEmbeddingUrl() {
  return GURL("https://example_embedder.com");
}

}  // namespace

class TopLevelStorageAccessPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TopLevelStorageAccessPermissionContextTest() = default;

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
    first_party_sets_handler_.SetGlobalSets(net::GlobalFirstPartySets());
  }

  ContentSetting DecidePermissionSync(
      TopLevelStorageAccessPermissionContext* permission_context,
      bool user_gesture,
      const GURL& requester_url,
      const GURL& embedding_url) {
    base::test::TestFuture<ContentSetting> future;
    permission_context->DecidePermissionForTesting(
        permissions::PermissionRequestData(permission_context, CreateFakeID(),
                                           user_gesture, requester_url,
                                           embedding_url),
        future.GetCallback());
    return future.Get();
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

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  first_party_sets::ScopedMockFirstPartySetsHandler&
  first_party_sets_handler() {
    return first_party_sets_handler_;
  }

 private:
  base::HistogramTester histogram_tester_;
  first_party_sets::ScopedMockFirstPartySetsHandler first_party_sets_handler_;
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
  permissions::PermissionRequestID::RequestLocalId::Generator
      request_id_generator_;
};

TEST_F(TopLevelStorageAccessPermissionContextTest,
       InsecureOriginsAreDisallowed) {
  GURL insecure_url = GURL("http://www.example.com");
  TopLevelStorageAccessPermissionContext permission_context(profile());
  EXPECT_FALSE(permission_context.IsPermissionAvailableToOrigins(insecure_url,
                                                                 insecure_url));
  EXPECT_FALSE(permission_context.IsPermissionAvailableToOrigins(
      insecure_url, GetRequesterURL()));
}

// No user gesture should force a permission rejection.
TEST_F(TopLevelStorageAccessPermissionContextTest,
       PermissionDeniedWithoutUserGesture) {
  TopLevelStorageAccessPermissionContext permission_context(profile());

  EXPECT_EQ(DecidePermissionSync(&permission_context, /*user_gesture=*/false,
                                 GetRequesterURL(), GetTopLevelURL()),
            CONTENT_SETTING_BLOCK);

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram,
                TopLevelStorageAccessRequestOutcome::kDeniedByPrerequisites),
            1);
}

TEST_F(TopLevelStorageAccessPermissionContextTest,
       PermissionStatusAsksWhenFeatureEnabled) {
  TopLevelStorageAccessPermissionContext permission_context(profile());

  EXPECT_EQ(PermissionStatus::ASK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetTopLevelURL())
                .status);
}

TEST_F(TopLevelStorageAccessPermissionContextTest,
       ImplicitGrant_DenialQueryStillAsk) {
  TopLevelStorageAccessPermissionContext permission_context(profile());

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets` exists yet.
  ASSERT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Each(DecidedByRelatedWebsiteSets(false)));

  EXPECT_EQ(DecidePermissionSync(&permission_context, /*user_gesture=*/true,
                                 GetRequesterURL(), GetDummyEmbeddingUrl()),
            CONTENT_SETTING_BLOCK);

  // Check the `SessionModel::DURABLE` settings with
  // `decided_by_related_website_sets`. None were granted, and implicit denials
  // are not currently persisted, which preserves the default `ASK` setting.
  EXPECT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Each(DecidedByRelatedWebsiteSets(false)));

  EXPECT_EQ(PermissionStatus::ASK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetDummyEmbeddingUrl())
                .status);
}

class TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest
    : public TopLevelStorageAccessPermissionContextTest {
 public:
  TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest() = default;

  void SetUp() override {
    TopLevelStorageAccessPermissionContextTest::SetUp();

    const net::SchemefulSite top_level(GetTopLevelURL());
    first_party_sets_handler().SetGlobalSets(net::GlobalFirstPartySets(
        base::Version("1.2.3"),
        /*entries=*/
        {
            {net::SchemefulSite(GetRequesterURL()),
             net::FirstPartySetEntry(top_level, net::SiteType::kAssociated, 0)},
            {top_level, net::FirstPartySetEntry(
                            top_level, net::SiteType::kPrimary, std::nullopt)},
        },
        /*aliases=*/{}));
  }

 private:
};

TEST_F(TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_AutograntedWithinFPS) {
  TopLevelStorageAccessPermissionContext permission_context(profile());

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets` exists yet.
  ASSERT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Each(DecidedByRelatedWebsiteSets(false)));

  EXPECT_EQ(DecidePermissionSync(&permission_context, /*user_gesture=*/true,
                                 GetRequesterURL(), GetTopLevelURL()),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram,
                TopLevelStorageAccessRequestOutcome::kGrantedByFirstPartySet),
            1);

  // Check the `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets` granted by FPS.
  EXPECT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Contains(DecidedByRelatedWebsiteSets(true)));
}

TEST_F(TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_CrossSiteFrameQueryStillAsk) {
  // First, grant the permission based on FPS membership.
  TopLevelStorageAccessPermissionContext permission_context(profile());

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets` exists yet.
  ASSERT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Each(DecidedByRelatedWebsiteSets(false)));

  EXPECT_EQ(DecidePermissionSync(&permission_context, /*user_gesture=*/true,
                                 GetRequesterURL(), GetTopLevelURL()),
            CONTENT_SETTING_ALLOW);

  // Check the `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets` granted by FPS.
  EXPECT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Contains(DecidedByRelatedWebsiteSets(true)));

  // Next, set up a cross-site frame.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  content::RenderFrameHost* navigated_subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GetDummyEmbeddingUrl(), subframe);

  // Even though the permission is granted, queries from cross-site frames
  // should return the default value.
  EXPECT_EQ(PermissionStatus::ASK,
            permission_context
                .GetPermissionStatus(navigated_subframe, GetRequesterURL(),
                                     GetTopLevelURL())
                .status);
}

TEST_F(TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_AutodeniedOutsideFPS) {
  TopLevelStorageAccessPermissionContext permission_context(profile());

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets` exists yet.
  ASSERT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Each(DecidedByRelatedWebsiteSets(false)));

  EXPECT_EQ(DecidePermissionSync(&permission_context, /*user_gesture=*/true,
                                 GetRequesterURL(), GetDummyEmbeddingUrl()),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram,
                TopLevelStorageAccessRequestOutcome::kDeniedByFirstPartySet),
            1);

  // Check the `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets`. None were granted, and implicit denials
  // are not currently persisted, which preserves the default `ASK` setting.
  EXPECT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Each(DecidedByRelatedWebsiteSets(false)));
}

TEST_F(TopLevelStorageAccessPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_DenialQueryStillAsk) {
  TopLevelStorageAccessPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  CHECK(settings_map);

  // Check no `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets` exists yet.
  ASSERT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Each(DecidedByRelatedWebsiteSets(false)));

  EXPECT_EQ(DecidePermissionSync(&permission_context, /*user_gesture=*/true,
                                 GetRequesterURL(), GetDummyEmbeddingUrl()),
            CONTENT_SETTING_BLOCK);

  // Check the `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets`. None were granted, and implicit denials
  // are not currently persisted, which preserves the default `ASK` setting.
  EXPECT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Each(DecidedByRelatedWebsiteSets(false)));

  // The permission denial should not be exposed via query. Note that the block
  // setting is not persisted anyway with the current implementation; this is a
  // forward-looking test.
  EXPECT_EQ(PermissionStatus::ASK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetDummyEmbeddingUrl())
                .status);
}
