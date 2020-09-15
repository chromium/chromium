// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace {

constexpr char kGrantIsImplicitHistogram[] =
    "API.StorageAccess.GrantIsImplicit";
constexpr char kPromptResultHistogram[] = "Permissions.Action.StorageAccess";

GURL GetTopLevelURL() {
  return GURL("https://embedder.example.com");
}

GURL GetRequesterURL() {
  return GURL("https://requester.example.com");
}

void SaveResult(ContentSetting* content_setting_result,
                ContentSetting content_setting) {
  DCHECK(content_setting_result);
  *content_setting_result = content_setting;
}

}  // namespace

class StorageAccessGrantPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 public:
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
    for (int grant_id = 0; grant_id < kDefaultImplicitGrantLimit; grant_id++) {
      const GURL embedding_origin(std::string(url::kHttpsScheme) +
                                  "://example_embedder_" +
                                  base::NumberToString(grant_id) + ".com");

      ContentSetting result = CONTENT_SETTING_DEFAULT;
      permission_context.DecidePermission(
          web_contents(), fake_id, requesting_origin, embedding_origin,
          /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
      base::RunLoop().RunUntilIdle();

      EXPECT_FALSE(manager->IsRequestInProgress());
    }
  }

  permissions::PermissionRequestID CreateFakeID() {
    return permissions::PermissionRequestID(web_contents()->GetMainFrame(),
                                            ++next_request_id_);
  }

 private:
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
  int next_request_id_ = 0;
};

TEST_F(StorageAccessGrantPermissionContextTest, InsecureOriginsAreAllowed) {
  GURL insecure_url = GURL("http://www.example.com");
  StorageAccessGrantPermissionContext permission_context(profile());
  EXPECT_TRUE(permission_context.IsPermissionAvailableToOrigins(insecure_url,
                                                                insecure_url));
  EXPECT_TRUE(permission_context.IsPermissionAvailableToOrigins(
      insecure_url, GetRequesterURL()));
}

// When the Storage Access API feature is disabled we should block the
// permission request.
TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionBlockedWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result);
}

// When the Storage Access API feature is enabled and we have a user gesture we
// should get a decision.
TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionDecidedWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ExhaustImplicitGrants(GetRequesterURL(), permission_context);

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  base::RunLoop().RunUntilIdle();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->IsRequestInProgress());

  permissions::PermissionRequest* request = manager->Requests().front();
  ASSERT_TRUE(request);
  ASSERT_EQ(1u, manager->Requests().size());
  // Prompt should have both origins.
  EXPECT_EQ(GetRequesterURL(), request->GetOrigin());
  EXPECT_EQ(GetTopLevelURL(), manager->GetEmbeddingOrigin());

  manager->Closing();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CONTENT_SETTING_ASK, result);
}

// No user gesture should force a permission rejection.
TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionDeniedWithoutUserGesture) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/false, base::BindOnce(&SaveResult, &result));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result);
}

TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionStatusBlockedWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetTopLevelURL())
                .content_setting);
}

TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionStatusAsksWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  StorageAccessGrantPermissionContext permission_context(profile());

  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context
                .GetPermissionStatus(/*render_frame_host=*/nullptr,
                                     GetRequesterURL(), GetTopLevelURL())
                .content_setting);
}

// Validate that each requesting origin has its own implicit grant limit. If
// the limit for one origin is exhausted it should not affect another.
TEST_F(StorageAccessGrantPermissionContextTest,
       ImplicitGrantLimitPerRequestingOrigin) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 0);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ExhaustImplicitGrants(GetRequesterURL(), permission_context);
  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester.ExpectBucketCount(kGrantIsImplicitHistogram,
                                     /*implicit_grant=*/1, 5);

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  base::RunLoop().RunUntilIdle();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->IsRequestInProgress());

  // Close the prompt and validate we get the expected setting back in our
  // callback.
  manager->Closing();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CONTENT_SETTING_ASK, result);

  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester.ExpectBucketCount(kGrantIsImplicitHistogram,
                                     /*implicit_grant=*/1, 5);
  histogram_tester.ExpectTotalCount(kPromptResultHistogram, 1);
  histogram_tester.ExpectBucketCount(kPromptResultHistogram,
                                     /*DISMISSED=*/2, 1);

  GURL alternate_requester_url = GURL("https://requester2_example.com");

  // However now if a different requesting origin makes a request we should see
  // it gets auto-granted as the limit has not been reached for it yet.
  result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, alternate_requester_url, GetTopLevelURL(),
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  base::RunLoop().RunUntilIdle();

  // We should have no prompts still and our latest result should be an allow.
  EXPECT_FALSE(manager->IsRequestInProgress());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, result);

  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 6);
  histogram_tester.ExpectBucketCount(kGrantIsImplicitHistogram,
                                     /*implicit_grant=*/1, 6);
  histogram_tester.ExpectBucketCount(kPromptResultHistogram,
                                     /*DISMISSED=*/2, 1);
}

TEST_F(StorageAccessGrantPermissionContextTest, ExplicitGrantDenial) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 0);
  histogram_tester.ExpectTotalCount(kPromptResultHistogram, 0);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ExhaustImplicitGrants(GetRequesterURL(), permission_context);
  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester.ExpectBucketCount(kGrantIsImplicitHistogram,
                                     /*implicit_grant=*/1, 5);

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  base::RunLoop().RunUntilIdle();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->IsRequestInProgress());

  // Deny the prompt and validate we get the expected setting back in our
  // callback.
  manager->Deny();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result);

  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester.ExpectBucketCount(kGrantIsImplicitHistogram,
                                     /*implicit_grant=*/1, 5);
  histogram_tester.ExpectTotalCount(kPromptResultHistogram, 1);
  histogram_tester.ExpectBucketCount(kPromptResultHistogram,
                                     /*DENIED=*/1, 1);
}

TEST_F(StorageAccessGrantPermissionContextTest, ExplicitGrantAccept) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(blink::features::kStorageAccessAPI);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 0);
  histogram_tester.ExpectTotalCount(kPromptResultHistogram, 0);

  StorageAccessGrantPermissionContext permission_context(profile());
  permissions::PermissionRequestID fake_id = CreateFakeID();

  ExhaustImplicitGrants(GetRequesterURL(), permission_context);
  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester.ExpectBucketCount(kGrantIsImplicitHistogram,
                                     /*implicit_grant=*/1, 5);

  ContentSetting result = CONTENT_SETTING_DEFAULT;
  permission_context.DecidePermission(
      web_contents(), fake_id, GetRequesterURL(), GetTopLevelURL(),
      /*user_gesture=*/true, base::BindOnce(&SaveResult, &result));
  base::RunLoop().RunUntilIdle();

  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->IsRequestInProgress());

  // Accept the prompt and validate we get the expected setting back in our
  // callback.
  manager->Accept();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, result);

  histogram_tester.ExpectTotalCount(kGrantIsImplicitHistogram, 6);
  histogram_tester.ExpectBucketCount(kGrantIsImplicitHistogram,
                                     /*implicit_grant=*/1, 5);
  histogram_tester.ExpectBucketCount(kGrantIsImplicitHistogram,
                                     /*explicit_grant=*/0, 1);
  histogram_tester.ExpectTotalCount(kPromptResultHistogram, 1);
  histogram_tester.ExpectBucketCount(kPromptResultHistogram,
                                     /*GRANTED=*/0, 1);
}
