// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"
#include <memory>

#include "base/barrier_callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/prefs/pref_service.h"
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

using testing::_;
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

struct TestCase {
  std::string test_name;
  bool saa_feature_enabled;
  bool permission_saa_feature_enabled;
};

}  // namespace

class StorageAccessGrantPermissionContextTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<TestCase> {
 public:
  StorageAccessGrantPermissionContextTest() {
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (StorageAccessAPIFeatureEnabled()) {
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

    if (PermissionStorageAccessAPIFeatureEnabled()) {
      enabled.push_back(
          {permissions::features::kPermissionStorageAccessAPI, {}});
    } else {
      disabled.push_back(permissions::features::kPermissionStorageAccessAPI);
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

    // Enable 3p cookie blocking.
    profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));

    content_settings::PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<chrome::PageSpecificContentSettingsDelegate>(
            web_contents()));

    DIPSService* dips_service = DIPSService::Get(browser_context());
    CHECK(dips_service);
    base::test::TestFuture<void> future;
    dips_service->storage()
        ->AsyncCall(&DIPSStorage::RecordInteraction)
        .WithArgs(GetRequesterURL(), base::Time::Now(),
                  DIPSCookieMode::kBlock3PC)
        .Then(future.GetCallback());
    ASSERT_TRUE(future.Wait());
    permission_context_ =
        std::make_unique<StorageAccessGrantPermissionContext>(profile());
  }

  void TearDown() override {
    permission_context_.reset();
    mock_permission_prompt_factory_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  bool StorageAccessAPIFeatureEnabled() {
    return GetParam().saa_feature_enabled;
  }
  bool PermissionStorageAccessAPIFeatureEnabled() {
    return GetParam().permission_saa_feature_enabled;
  }

  std::unique_ptr<base::test::TestFuture<ContentSetting>> DecidePermission(
      bool user_gesture) {
    auto future = std::make_unique<base::test::TestFuture<ContentSetting>>();
    permission_context_->DecidePermissionForTesting(
        CreateFakeID(), GetRequesterURL(), GetTopLevelURL(), user_gesture,
        future->GetCallback());
    return future;
  }

  ContentSetting DecidePermissionSync(bool user_gesture) {
    return DecidePermission(user_gesture)->Get();
  }

  ContentSetting RequestPermissionSync() {
    base::test::TestFuture<ContentSetting> future;
    permission_context()->RequestPermission(CreateFakeID(), GetRequesterURL(),
                                            true, future.GetCallback());

    return future.Get();
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

  void WaitUntilPrompt() {
    mock_permission_prompt_factory_->WaitForPermissionBubble();
    ASSERT_TRUE(request_manager()->IsRequestInProgress());
  }

  content_settings::PageSpecificContentSettings*
  page_specific_content_settings() {
    return content_settings::PageSpecificContentSettings::GetForFrame(
        web_contents()->GetPrimaryMainFrame());
  }

  StorageAccessGrantPermissionContext* permission_context() {
    return permission_context_.get();
  }

  permissions::PermissionRequestManager* request_manager() {
    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    CHECK(manager);
    return manager;
  }

  permissions::MockPermissionPromptFactory& prompt_factory() {
    return *mock_permission_prompt_factory_;
  }

  struct PrintToStringParamName {
    std::string operator()(
        const testing::TestParamInfo<::TestCase>& info) const {
      return info.param.test_name;
    }
  };

 private:
  base::test::ScopedFeatureList features_;
  std::unique_ptr<StorageAccessGrantPermissionContext> permission_context_;
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
  permissions::PermissionRequestID::RequestLocalId::Generator
      request_id_generator_;
};

class StorageAccessGrantPermissionContextAPIDisabledTest
    : public StorageAccessGrantPermissionContextTest {
};

TEST_P(StorageAccessGrantPermissionContextAPIDisabledTest,
       InsecureOriginsDisallowed) {
  GURL insecure_url = GURL("http://www.example.com");
  EXPECT_FALSE(permission_context()->IsPermissionAvailableToOrigins(
      insecure_url, insecure_url));
  EXPECT_FALSE(permission_context()->IsPermissionAvailableToOrigins(
      insecure_url, GetRequesterURL()));

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

// When the Storage Access API feature is disabled (the default) we
// should block the permission request.
TEST_P(StorageAccessGrantPermissionContextAPIDisabledTest, PermissionBlocked) {
  EXPECT_EQ(CONTENT_SETTING_BLOCK, DecidePermissionSync(/*user_gesture=*/true));

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

// When 3p cookie access is already allowed by user-agent-specific cookie
// settings, request should be allowed even when the Storage Access API feature
// is disabled.
TEST_P(StorageAccessGrantPermissionContextAPIDisabledTest,
       AllowedByCookieSettings) {
  base::HistogramTester histogram_tester;
  // Allow 3p cookies.
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));

  // User gesture is not needed.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            DecidePermissionSync(/*user_gesture=*/false));
  histogram_tester.ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kAllowedByCookieSettings, 1);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

// It's a bit odd to have a parameterized test suite only check the disabled
// case. But in this case, the test cases of API being enabled by either feature
// is already covered by StorageAccessGrantPermissionContextAPIEnabledTest.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    StorageAccessGrantPermissionContextAPIDisabledTest,
    testing::ValuesIn<TestCase>({
        {"disable_all", false, false},
    }),
    StorageAccessGrantPermissionContextTest::PrintToStringParamName());

class StorageAccessGrantPermissionContextAPIEnabledTest
    : public StorageAccessGrantPermissionContextTest {
 public:

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

// Test that after a successful explicit storage access grant, there's a content
// setting that applies on an (embedded site, top-level site) scope.
TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest,
       ExplicitGrantAcceptCrossSiteContentSettings) {
  // Assert that all content settings are in their initial state.
  CheckCrossSiteContentSettings(ContentSetting::CONTENT_SETTING_ASK);

  auto future = DecidePermission(/*user_gesture=*/true);
  WaitUntilPrompt();

  // Accept the prompt and validate we get the expected setting back in our
  // callback.
  request_manager()->Accept();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, future->Get());

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
TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest, PermissionDecided) {
  auto future = DecidePermission(/*user_gesture=*/true);
  WaitUntilPrompt();

  permissions::PermissionRequest* request =
      request_manager()->Requests().front();
  ASSERT_TRUE(request);
  ASSERT_EQ(1u, request_manager()->Requests().size());
  // Prompt should have both origins.
  EXPECT_EQ(GetRequesterURL(), request_manager()->GetRequestingOrigin());
  EXPECT_EQ(GetTopLevelURL(), request_manager()->GetEmbeddingOrigin());

  request_manager()->Dismiss();
  EXPECT_EQ(CONTENT_SETTING_ASK, future->Get());
  histogram_tester().ExpectUniqueSample(kRequestOutcomeHistogram,
                                        RequestOutcome::kDismissedByUser, 1);
  // Expect no pscs entry for dismissed permissions.
  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

// No user gesture should force a permission rejection.
TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest,
       PermissionDeniedWithoutUserGesture) {
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            DecidePermissionSync(/*user_gesture=*/false));
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kDeniedByPrerequisites, 1);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest,
       PermissionGrantReused) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(GetRequesterURL(), GetTopLevelURL(),
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_ALLOW);
  RequestPermissionSync();
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kReusedPreviousDecision, 1);
  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(Pair(GetRequesterSite(), true)));
}

TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest, BlockReused) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(GetRequesterURL(), GetTopLevelURL(),
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_BLOCK);
  RequestPermissionSync();
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kReusedPreviousDecision, 1);
  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(Pair(GetRequesterSite(), true)));
}

TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest, FpsGrantReused) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings::ContentSettingConstraints constraint;
  constraint.set_session_model(
      content_settings::SessionModel::NonRestorableUserSession);
  map->SetContentSettingDefaultScope(GetRequesterURL(), GetTopLevelURL(),
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_ALLOW, constraint);

  RequestPermissionSync();
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kReusedImplicitGrant, 1);
  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

TEST_P(StorageAccessGrantPermissionContextAPIDisabledTest,
       PermissionStatusBlocked) {
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context()
                ->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                      GetRequesterURL(), GetTopLevelURL())
                .content_setting);
}

TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest,
       PermissionStatusAsksWhenFeatureEnabled) {
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context()
                ->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                      GetRequesterURL(), GetTopLevelURL())
                .content_setting);
}

// When 3p cookie access is already allowed by user-agent-specific cookie
// settings, request should be allowed without granting an explicit storage
// access permission.
TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest,
       AllowedByCookieSettings) {
  // Allow 3p cookies.
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kOff));

  // User gesture is not needed.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            DecidePermissionSync(/*user_gesture=*/false));
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kAllowedByCookieSettings, 1);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

// When 3p cookie access is blocked by user explicitly, request should be denied
// without prompting.
TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest,
       DeniedByCookieSettings) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  settings_map->SetContentSettingDefaultScope(
      GetRequesterURL(), GetTopLevelURL(), ContentSettingsType::COOKIES,
      CONTENT_SETTING_BLOCK);

  // User gesture is not needed.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            DecidePermissionSync(/*user_gesture=*/false));
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kDeniedByCookieSettings, 1);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    StorageAccessGrantPermissionContextAPIEnabledTest,
    testing::ValuesIn<TestCase>({
        {"enable_all", true, true},
        {"enable_saa", true, false},
        {"enable_permission_saa", false, true},
    }),
    StorageAccessGrantPermissionContextTest::PrintToStringParamName());

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
        /*disabled_features=*/{
            permissions::features::kPermissionStorageAccessAPI});
  }

  // Helper to request storage access on enough unique embedding_origin GURLs
  // from |requesting_origin| to ensure that all potential implicit grants will
  // be granted.
  void ExhaustImplicitGrants(const GURL& requesting_origin) {
    permissions::PermissionRequestID fake_id = CreateFakeID();

    const int implicit_grant_limit =
        blink::features::kStorageAccessAPIImplicitGrantLimit.Get();
    base::test::TestFuture<const std::vector<ContentSetting>> future;
    auto barrier = base::BarrierCallback<ContentSetting>(implicit_grant_limit,
                                                         future.GetCallback());
    for (int grant_id = 0; grant_id < implicit_grant_limit; grant_id++) {
      permission_context()->DecidePermissionForTesting(
          fake_id, requesting_origin, GetDummyEmbeddingUrl(grant_id),
          /*user_gesture=*/true, barrier);
    }
    ASSERT_TRUE(future.Wait());
    EXPECT_FALSE(request_manager()->IsRequestInProgress());
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Validate that each requesting origin has its own implicit grant limit. If
// the limit for one origin is exhausted it should not affect another.
TEST_P(StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest,
       ImplicitGrantLimitPerRequestingOrigin) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);

  ExhaustImplicitGrants(GetRequesterURL());
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 5);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, 5);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram, RequestOutcome::kGrantedByAllowance),
            5);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
  {
    auto future = DecidePermission(/*user_gesture=*/true);
    WaitUntilPrompt();

    // Close the prompt and validate we get the expected setting back in our
    // callback.
    request_manager()->Dismiss();
    EXPECT_EQ(CONTENT_SETTING_ASK, future->Get());
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
  permission_context()->DecidePermissionForTesting(
      CreateFakeID(), alternate_requester_url, GetTopLevelURL(),
      /*user_gesture=*/true, future.GetCallback());

  // We should have no prompts still and our latest result should be an allow.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, future.Get());
  EXPECT_FALSE(request_manager()->IsRequestInProgress());
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
TEST_P(StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest,
       ImplicitGrantLimitSiteScoping) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);

  ExhaustImplicitGrants(GetRequesterURL());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GetDummyEmbeddingUrlWithSubdomain());

  int implicit_grant_limit =
      blink::features::kStorageAccessAPIImplicitGrantLimit.Get();

  // Although the grants are exhausted, another request from a top-level origin
  // that is same site with an existing grant should still be auto-granted. The
  // call is to `RequestPermission`, which checks for existing grants, while
  // `DecidePermission` does not.
  // We should have no prompts still and our latest result should be an allow.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, RequestPermissionSync());
  EXPECT_FALSE(request_manager()->IsRequestInProgress());
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRequestOutcomeHistogram, RequestOutcome::kGrantedByAllowance),
            implicit_grant_limit);

  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram,
                                      implicit_grant_limit);
  histogram_tester().ExpectBucketCount(kGrantIsImplicitHistogram,
                                       /*sample=*/true, implicit_grant_limit);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest, ExplicitGrantDenial) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);
  histogram_tester().ExpectTotalCount(kPromptResultHistogram, 0);

  auto future = DecidePermission(/*user_gesture=*/true);
  WaitUntilPrompt();

  // Deny the prompt and validate we get the expected setting back in our
  // callback.
  request_manager()->Deny();
  EXPECT_EQ(CONTENT_SETTING_BLOCK, future->Get());

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

TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest,
       ExplicitGrantDenialNotExposedViaQuery) {
  // Set the content setting to blocked, mimicking a prompt rejection by the
  // user.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  settings_map->SetContentSettingDefaultScope(
      GetRequesterURL(), GetTopLevelURL(), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_BLOCK);

  prompt_factory().set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  auto future = DecidePermission(/*user_gesture=*/true);
  // Ensure the prompt is not shown.
  ASSERT_FALSE(request_manager()->IsRequestInProgress());
  EXPECT_EQ(CONTENT_SETTING_BLOCK, future->Get());

  // However, ensure that the user's denial is not exposed when querying the
  // permission, per the spec.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            permission_context()
                ->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                      GetRequesterURL(), GetTopLevelURL())
                .content_setting);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(Pair(GetRequesterSite(), false)));
}

TEST_P(StorageAccessGrantPermissionContextAPIEnabledTest, ExplicitGrantAccept) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);
  histogram_tester().ExpectTotalCount(kPromptResultHistogram, 0);

  auto future = DecidePermission(/*user_gesture=*/true);
  WaitUntilPrompt();

  // Accept the prompt and validate we get the expected setting back in our
  // callback.
  request_manager()->Accept();
  EXPECT_EQ(CONTENT_SETTING_ALLOW, future->Get());

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

// This test suite is no-op since the enablde/disabled features are hard coded.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest,
    testing::ValuesIn<TestCase>({
        {"enable_saa", true, false},
    }),
    StorageAccessGrantPermissionContextTest::PrintToStringParamName());

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
        /*disabled_features=*/{
            permissions::features::kPermissionStorageAccessAPI});
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

TEST_P(StorageAccessGrantPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_AutograntedWithinFPS) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  DCHECK(settings_map);

  // Check no `SessionModel::NonRestorableUserSession` setting exists yet.
  ContentSettingsForOneType non_restorable_grants =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::STORAGE_ACCESS,
          content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(0u, non_restorable_grants.size());

  EXPECT_EQ(DecidePermissionSync(/*user_gesture=*/true), CONTENT_SETTING_ALLOW);

  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kGrantedByFirstPartySet, 1);
  histogram_tester().ExpectUniqueSample(kGrantIsImplicitHistogram,
                                        /*sample=*/true, 1);

  DCHECK(settings_map);
  // Check the `SessionModel::NonRestorableUserSession` settings granted by FPS.
  non_restorable_grants = settings_map->GetSettingsForOneType(
      ContentSettingsType::STORAGE_ACCESS,
      content_settings::SessionModel::NonRestorableUserSession);
  EXPECT_EQ(1u, non_restorable_grants.size());

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

// This test suite is no-op since the enablde/disabled features are hard coded.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    StorageAccessGrantPermissionContextAPIWithFirstPartySetsTest,
    testing::ValuesIn<TestCase>({
        {"enable_saa", true, false},
    }),
    StorageAccessGrantPermissionContextTest::PrintToStringParamName());
