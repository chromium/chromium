// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"

#include "base/barrier_callback.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kGrantIsImplicitHistogram[] =
    "API.StorageAccess.GrantIsImplicit";
constexpr char kPromptResultHistogram[] = "Permissions.Action.StorageAccess";
constexpr char kRequestOutcomeHistogram[] = "API.StorageAccess.RequestOutcome";

GURL GetTopLevelURL() {
  return GURL("https://embedder.example.com");
}

GURL GetDummyEmbeddingUrlWithSubdomain() {
  return GURL("https://subdomain.example_embedder_1.com");
}

GURL GetRequesterURL() {
  return GURL("https://requester.example.com");
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
      enabled.push_back({net::features::kStorageAccessAPI,
                         {
                             {
                                 "storage_access_api_auto_deny_outside_fps",
                                 "false",
                             },
                         }});
    } else {
      disabled.push_back(net::features::kStorageAccessAPI);
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
  }

  void TearDown() override {
    mock_permission_prompt_factory_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
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
        net::features::kStorageAccessAPIDefaultImplicitGrantLimit;
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

class StorageAccessGrantPermissionContextAPIDisabledTest
    : public StorageAccessGrantPermissionContextTest {
 public:
  StorageAccessGrantPermissionContextAPIDisabledTest()
      : StorageAccessGrantPermissionContextTest(false) {}
};

TEST_F(StorageAccessGrantPermissionContextAPIDisabledTest,
       InsecureOriginsAreAllowed) {
  GURL insecure_url = GURL("http://www.example.com");
  StorageAccessGrantPermissionContext permission_context(profile());
  EXPECT_TRUE(permission_context.IsPermissionAvailableToOrigins(insecure_url,
                                                                insecure_url));
  EXPECT_TRUE(permission_context.IsPermissionAvailableToOrigins(
      insecure_url, GetRequesterURL()));
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

// When the Storage Access API feature is enabled and we have a user gesture we
// should get a decision.
TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest, PermissionDecided) {
  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ExhaustImplicitGrants(GetRequesterURL(), permission_context);

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
  EXPECT_EQ(histogram_tester().GetBucketCount(kRequestOutcomeHistogram,
                                              RequestOutcome::kDismissedByUser),
            1);
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
  EXPECT_EQ(
      histogram_tester().GetBucketCount(kRequestOutcomeHistogram,
                                        RequestOutcome::kDeniedByPrerequisites),
      1);
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

// Validate that each requesting origin has its own implicit grant limit. If
// the limit for one origin is exhausted it should not affect another.
TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest,
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
TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest,
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
      net::features::kStorageAccessAPIDefaultImplicitGrantLimit;

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
}

TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest, ExplicitGrantDenial) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);
  histogram_tester().ExpectTotalCount(kPromptResultHistogram, 0);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ExhaustImplicitGrants(GetRequesterURL(), permission_context);
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, 5);

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

  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, 5);
  histogram_tester().ExpectTotalCount(kPromptResultHistogram, 1);
  histogram_tester().ExpectBucketCount(
      kPromptResultHistogram,
      /*sample=*/permissions::PermissionAction::DENIED, 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(kRequestOutcomeHistogram,
                                              RequestOutcome::kDeniedByUser),
            1);
}

TEST_F(StorageAccessGrantPermissionContextAPIEnabledTest, ExplicitGrantAccept) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);
  histogram_tester().ExpectTotalCount(kPromptResultHistogram, 0);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ExhaustImplicitGrants(GetRequesterURL(), permission_context);
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, 5);

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

  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 6);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, 5);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/false, 1);
  histogram_tester().ExpectTotalCount(kPromptResultHistogram, 1);
  histogram_tester().ExpectBucketCount(
      kPromptResultHistogram,
      /*sample=*/permissions::PermissionAction::GRANTED, 1);
  EXPECT_EQ(histogram_tester().GetBucketCount(kRequestOutcomeHistogram,
                                              RequestOutcome::kGrantedByUser),
            1);
}
