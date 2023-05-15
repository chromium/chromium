// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"

#include "base/barrier_callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {

using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

constexpr char kGrantIsImplicitHistogram[] =
    "API.StorageAccess.GrantIsImplicit";
constexpr char kPromptResultHistogram[] = "Permissions.Action.StorageAccess";
constexpr char kRequestOutcomeHistogram[] = "API.StorageAccess.RequestOutcome";

GURL GetTopLevelURL() {
  return GURL("https://embedder.com");
}

GURL GetTopLevelURLSubdomain() {
  return GURL("https://sub.embedder.com");
}

GURL GetDummyEmbeddingUrlWithSubdomain() {
  return GURL("https://subdomain.example_embedder_1.com");
}

GURL GetRequesterURL() {
  return GURL("https://requester.example.com");
}

net::SchemefulSite GetRequesterSite() {
  return net::SchemefulSite(GetRequesterURL());
}

GURL GetRequesterURLSubdomain() {
  return GURL("https://another-requester.example.com");
}

GURL GetDummyEmbeddingUrl(int dummy_id) {
  return GURL(std::string(url::kHttpsScheme) + "://example_embedder_" +
              base::NumberToString(dummy_id) + ".com");
}

}  // namespace

class StorageAccessGrantPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 public:
  explicit StorageAccessGrantPermissionContextTest(bool saa_enabled) {
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (saa_enabled) {
      enabled.push_back(
          {blink::features::kStorageAccessAPI,
           {
               {
                   blink::features::kStorageAccessAPIAutoGrantInFPS.name,
                   "false",
               },
               {
                   blink::features::kStorageAccessAPIAutoDenyOutsideFPS.name,
                   "false",
               },
               {
                   blink::features::kStorageAccessAPIImplicitGrantLimit.name,
                   "0",
               },
           }});
    } else {
      disabled.push_back(blink::features::kStorageAccessAPI);
    }
    features_.InitWithFeaturesAndParameters(enabled, disabled);
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

    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
            web_contents()));
  }

  void TearDown() override {
    mock_permission_prompt_factory_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Helper to ensure that a given content setting is consistently applied on a
  // cross-site scope.
  void CheckCrossSiteContentSettings(ContentSetting expected_setting) {
    HostContentSettingsMap* settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    DCHECK(settings_map);

    auto setting =
        settings_map->GetContentSetting(GetRequesterURL(), GetTopLevelURL(),
                                        ContentSettingsType::STORAGE_ACCESS);

    EXPECT_EQ(setting, expected_setting);

    setting = settings_map->GetContentSetting(
        GetRequesterURLSubdomain(), GetTopLevelURL(),
        ContentSettingsType::STORAGE_ACCESS);

    EXPECT_EQ(setting, expected_setting);

    setting = settings_map->GetContentSetting(
        GetRequesterURLSubdomain(), GetTopLevelURLSubdomain(),
        ContentSettingsType::STORAGE_ACCESS);

    EXPECT_EQ(setting, expected_setting);

    setting = settings_map->GetContentSetting(
        GetRequesterURL(), GetTopLevelURLSubdomain(),
        ContentSettingsType::STORAGE_ACCESS);

    EXPECT_EQ(setting, expected_setting);
  }

  permissions::PermissionRequestID CreateFakeID() {
    return permissions::PermissionRequestID(
        web_contents()->GetPrimaryMainFrame(),
        request_id_generator_.GenerateNextId());
  }

  content_settings::PageSpecificContentSettings*
  page_specific_content_settings() {
    return content_settings::PageSpecificContentSettings::GetForFrame(
        web_contents()->GetPrimaryMainFrame());
  }

 private:
  base::test::ScopedFeatureList features_;
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
  permissions::PermissionRequestID::RequestLocalId::Generator
      request_id_generator_;
};

class StorageAccessGrantPermissionContextAPIDisabledTest
    : public StorageAccessGrantPermissionContextTest {
 public:
  StorageAccessGrantPermissionContextAPIDisabledTest()
      : StorageAccessGrantPermissionContextTest(false) {}
};

TEST_F(StorageAccessGrantPermissionContextAPIDisabledTest,
       InsecureOriginsDisallowed) {
  GURL insecure_url = GURL("http://www.example.com");
  StorageAccessGrantPermissionContext permission_context(profile());
  EXPECT_FALSE(permission_context.IsPermissionAvailableToOrigins(insecure_url,
                                                                 insecure_url));
  EXPECT_FALSE(permission_context.IsPermissionAvailableToOrigins(
      insecure_url, GetRequesterURL()));

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

// When the Storage Access API feature is disabled (the default) we
// should block the permission request.
TEST_F(StorageAccessGrantPermissionContextAPIDisabledTest, PermissionBlocked) {
  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, future.Get());

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

class StorageAccessGrantPermissionContextAPIEnabledTest
    : public StorageAccessGrantPermissionContextTest {
 public:
  StorageAccessGrantPermissionContextAPIEnabledTest()
      : StorageAccessGrantPermissionContextTest(true) {}

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

// Test that after a successful explicit storage access grant, there's a content
// setting that applies on an (embedded site, top-level site) scope.
TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest,
       ExplicitGrantAcceptCrossSiteContentSettings) {
  StorageAccessGrantPermissionContext permission_context(profile());

  // Assert that all content settings are in their initial state.
  CheckCrossSiteContentSettings(ContentSetting::CONTENT_SETTING_ASK);

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      CreateFakeID(), GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  // Run until the prompt is ready.
  base::RunLoop().RunUntilIdle();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->IsRequestInProgress());

  // Accept the prompt and validate we get the expected setting back in our
  // callback.
  manager->Accept();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, future.Get());

  histogram_tester().ExpectUniqueSample(kGrantIsImplicitHistogram,
                                        /*sample=*/false, 1);
  histogram_tester().ExpectUniqueSample(
      kPromptResultHistogram, /*sample=*/permissions::PermissionAction::GRANTED,
      1);
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, /*sample=*/RequestOutcome::kGrantedByUser, 1);

  // Assert that the permission grant set a content setting that applies
  // at the right scope.
  CheckCrossSiteContentSettings(ContentSetting::CONTENT_SETTING_ALLOW);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(Pair(GetRequesterSite(), true)));
}

// When the Storage Access API feature is enabled and we have a user gesture we
// should get a decision.
TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest, PermissionDecided) {
  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  // Run until the prompt is ready.
  base::RunLoop().RunUntilIdle();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->IsRequestInProgress());

  permissions::PermissionRequest* request = manager->Requests().front();
  ASSERT_TRUE(request);
  ASSERT_EQ(1u, manager->Requests().size());
  // Prompt should have both origins.
  EXPECT_EQ(GetRequesterURL(), manager->GetRequestingOrigin());
  EXPECT_EQ(GetTopLevelURL(), manager->GetEmbeddingOrigin());

  manager->Dismiss();
  EXPECT_EQ(CONTENT_SETTING_ASK, future.Get());
  histogram_tester().ExpectUniqueSample(kRequestOutcomeHistogram,
                                        RequestOutcome::kDismissedByUser, 1);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(Pair(GetRequesterSite(), false)));
}

// No user gesture should force a permission rejection.
TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest,
       PermissionDeniedWithoutUserGesture) {
  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/false, future.GetCallback());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, future.Get());
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kDeniedByPrerequisites, 1);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

TEST_F(StorageAccessGrantPermissionContextAPIDisabledTest,
       PermissionStatusBlocked) {
  StorageAccessGrantPermissionContext permission_context(profile());

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetTopLevelURL())
                .content_setting);
}

TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest,
       PermissionStatusAsksWhenFeatureEnabled) {
  StorageAccessGrantPermissionContext permission_context(profile());

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetTopLevelURL())
                .content_setting);
}

class StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest
    : public StorageAccessGrantPermissionContextAPIEnabledTest {
 public:
  StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest() {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kFirstPartySets, {}},
         {blink::features::kStorageAccessAPI,
          {
              {
                  blink::features::kStorageAccessAPIAutoGrantInFPS.name,
                  "false",
              },
              {
                  blink::features::kStorageAccessAPIAutoDenyOutsideFPS.name,
                  "false",
              },
              {
                  blink::features::kStorageAccessAPIImplicitGrantLimit.name,
                  "5",
              },
          }}},
        /*disabled_features=*/{});
  }

  // Helper to request storage access on enough unique embedding_origin GURLs
  // from |requesting_origin| to ensure that all potential implicit grants will
  // be granted.
  void ExhaustImplicitGrants(
      const GURL& requesting_origin,
      StorageAccessGrantPermissionContext& permission_context) {
    permissions::PermissionRequestID fake_id = CreateFakeID();

    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    DCHECK(manager);
    const int implicit_grant_limit =
        blink::features::kStorageAccessAPIImplicitGrantLimit.Get();
    base::RunLoop run_loop;
    auto barrier = base::BarrierCallback<ContentSetting>(
        implicit_grant_limit,
        base::BindLambdaForTesting(
            [&](const std::vector<ContentSetting> results) {
              run_loop.Quit();
            }));
    for (int grant_id = 0; grant_id < implicit_grant_limit; grant_id++) {
      permission_context.DecidePermissionForTesting(
          fake_id, requesting_origin, GetDummyEmbeddingUrl(grant_id),
          /*user_gesture=*/true, barrier);
    }
    run_loop.Run();
    EXPECT_FALSE(manager->IsRequestInProgress());
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Validate that each requesting origin has its own implicit grant limit. If
// the limit for one origin is exhausted it should not affect another.
TEST_F(StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest,
       ImplicitGrantLimitPerRequestingOrigin) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ExhaustImplicitGrants(GetRequesterURL(), permission_context);
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, 5);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram, RequestOutcome::kGrantedByAllowance),
            5);
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());

  {
    base::test::TestFuture<ContentSetting> future;
    permission_context.DecidePermissionForTesting(
        fake_id, GetRequesterURL(), GetTopLevelURL(),
        /*user_gesture=*/true, future.GetCallback());

    // Run until the prompt is ready.
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(manager->IsRequestInProgress());

    // Close the prompt and validate we get the expected setting back in our
    // callback.
    manager->Dismiss();
    EXPECT_EQ(CONTENT_SETTING_ASK, future.Get());
  }
  EXPECT_EQ(histogram_tester().GetBucketCount(kRequestOutcomeHistogram,
                                              RequestOutcome::kDismissedByUser),
            1);

  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, 5);
  histogram_tester().ExpectTotalCount(kPromptResultHistogram, 1);
  histogram_tester().ExpectBucketCount(
      kPromptResultHistogram,
      /*sample=*/permissions::PermissionAction::DISMISSED, 1);

  GURL alternate_requester_url = GURL("https://requester2_example.com");

  // However now if a different requesting origin makes a request we should see
  // it gets auto-granted as the limit has not been reached for it yet.
  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, alternate_requester_url, GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  // We should have no prompts still and our latest result should be an allow.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, future.Get());
  EXPECT_FALSE(manager->IsRequestInProgress());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram, RequestOutcome::kGrantedByAllowance),
            6);

  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 6);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, 6);
  histogram_tester().ExpectBucketCount(
      kPromptResultHistogram,
      /*sample=*/permissions::PermissionAction::DISMISSED, 1);
}

// Validate that each the implicit grant limit is scoped by top-level site.
TEST_F(StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest,
       ImplicitGrantLimitSiteScoping) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);

  StorageAccessGrantPermissionContext permission_context(profile());

  ExhaustImplicitGrants(GetRequesterURL(), permission_context);

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GetDummyEmbeddingUrlWithSubdomain());

  // Although the grants are exhausted, another request from a top-level origin
  // that is same site with an existing grant should still be auto-granted. The
  // call is to `RequestPermission`, which checks for existing grants, while
  // `DecidePermission` does not.
  base::test::TestFuture<ContentSetting> future;
  permission_context.RequestPermission(CreateFakeID(), GetRequesterURL(), true,
                                       future.GetCallback());

  int implicit_grant_limit =
      blink::features::kStorageAccessAPIImplicitGrantLimit.Get();

  // We should have no prompts still and our latest result should be an allow.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, future.Get());
  EXPECT_FALSE(manager->IsRequestInProgress());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram, RequestOutcome::kGrantedByAllowance),
            implicit_grant_limit);

  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram,
                                      implicit_grant_limit);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, implicit_grant_limit);

  // TODO(crbug.com/1433644): Here we are actually logging a StorageAccess
  // request because we don't know that the previously granted permission was
  // implicit. We should tag implicit grants to be able to know later on whether
  // a previous grant was implicit.
  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(
                  Pair(GetRequesterSite(), true)));  // Should be IsEmpty().
}

TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest, ExplicitGrantDenial) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);
  histogram_tester().ExpectTotalCount(kPromptResultHistogram, 0);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  // Run until the prompt is ready.
  base::RunLoop().RunUntilIdle();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->IsRequestInProgress());

  // Deny the prompt and validate we get the expected setting back in our
  // callback.
  manager->Deny();
  EXPECT_EQ(CONTENT_SETTING_BLOCK, future.Get());

  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);
  histogram_tester().ExpectUniqueSample(
      kPromptResultHistogram, /*sample=*/permissions::PermissionAction::DENIED,
      1);
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, /*sample=*/RequestOutcome::kDeniedByUser, 1);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(Pair(GetRequesterSite(), false)));
}

TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest,
       ExplicitGrantDenialNotExposedViaQuery) {
  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  // Set the content setting to blocked, mimicking a prompt rejection by the
  // user.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  settings_map->SetContentSettingDefaultScope(
      GetRequesterURL(), GetTopLevelURL(), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_BLOCK);

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  // Ensure the prompt is not shown.
  base::RunLoop().RunUntilIdle();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);
  ASSERT_FALSE(manager->IsRequestInProgress());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, future.Get());

  // However, ensure that the user's denial is not exposed when querying the
  // permission, per the spec.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetTopLevelURL())
                .content_setting);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(Pair(GetRequesterSite(), false)));
}

TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest, ExplicitGrantAccept) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);
  histogram_tester().ExpectTotalCount(kPromptResultHistogram, 0);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  // Run until the prompt is ready.
  base::RunLoop().RunUntilIdle();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->IsRequestInProgress());

  // Accept the prompt and validate we get the expected setting back in our
  // callback.
  manager->Accept();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, future.Get());

  histogram_tester().ExpectUniqueSample(kGrantIsImplicitHistogram,
                                        /*sample=*/false, 1);
  histogram_tester().ExpectUniqueSample(
      kPromptResultHistogram, permissions::PermissionAction::GRANTED, 1);
  histogram_tester().ExpectUniqueSample(kRequestOutcomeHistogram,
                                        RequestOutcome::kGrantedByUser, 1);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(Pair(GetRequesterSite(), true)));
}

class StorageAccessGrantPermissionContextAPIWithFirstPartySetsTest
    : public StorageAccessGrantPermissionContextAPIEnabledTest {
 public:
  StorageAccessGrantPermissionContextAPIWithFirstPartySetsTest() {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kFirstPartySets, {}},
         {blink::features::kStorageAccessAPI,
          {
              {
                  blink::features::kStorageAccessAPIAutoGrantInFPS.name,
                  "true",
              },
              {
                  blink::features::kStorageAccessAPIAutoDenyOutsideFPS.name,
                  "true",
              },
              {
                  blink::features::kStorageAccessAPIImplicitGrantLimit.name,
                  "0",
              },
          }}},
        /*disabled_features=*/{});
  }
  void SetUp() override {
    StorageAccessGrantPermissionContextAPIEnabledTest::SetUp();

    // Create a FPS with https://requester.example.com as the member and
    // https://embedder.com as the primary.
    first_party_sets_handler_.SetGlobalSets(net::GlobalFirstPartySets(
        base::Version("1.2.3"),
        /*entries=*/
        {{net::SchemefulSite(GetTopLevelURL()),
          {net::FirstPartySetEntry(net::SchemefulSite(GetTopLevelURL()),
                                   net::SiteType::kPrimary, absl::nullopt)}},
         {net::SchemefulSite(GetRequesterURL()),
          {net::FirstPartySetEntry(net::SchemefulSite(GetTopLevelURL()),
                                   net::SiteType::kAssociated, 0)}}},
        /*aliases=*/{}));
  }

 private:
  base::test::ScopedFeatureList features_;
  first_party_sets::ScopedMockFirstPartySetsHandler first_party_sets_handler_;
};

TEST_F(StorageAccessGrantPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_AutograntedWithinFPS) {
  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  DCHECK(settings_map);

  // Check no `SessionModel::NonRestorableUserSession` setting exists yet.
  ContentSettingsForOneType non_restorable_grants;
  settings_map->GetSettingsForOneType(
      ContentSettingsType::STORAGE_ACCESS, &non_restorable_grants,
      content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(0u, non_restorable_grants.size());

  base::test::TestFuture<ContentSetting> future;
  permission_context.DecidePermissionForTesting(
      fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  EXPECT_EQ(CONTENT_SETTING_ALLOW, future.Get());
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kGrantedByFirstPartySet, 1);
  histogram_tester().ExpectUniqueSample(kGrantIsImplicitHistogram,
                                        /*sample=*/true, 1);

  DCHECK(settings_map);
  // Check the `SessionModel::NonRestorableUserSession` settings granted by FPS.
  settings_map->GetSettingsForOneType(
      ContentSettingsType::STORAGE_ACCESS, &non_restorable_grants,
      content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(1u, non_restorable_grants.size());

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}
