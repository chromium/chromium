// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"

#include <memory>

#include "base/barrier_callback.h"
#include "base/check_deref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/dips/dips_bounce_detector.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace {

using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::Each;
using testing::ElementsAre;
using testing::Gt;
using testing::IsEmpty;
using testing::Lt;
using testing::Pair;
using testing::UnorderedElementsAre;
using PermissionStatus = blink::mojom::PermissionStatus;

constexpr char kGrantIsImplicitHistogram[] =
    "API.StorageAccess.GrantIsImplicit";
constexpr char kPromptResultHistogram[] = "Permissions.Action.StorageAccess";
constexpr char kRequestOutcomeHistogram[] = "API.StorageAccess.RequestOutcome";

MATCHER_P(DecidedByRelatedWebsiteSets, inner, "") {
  return testing::ExplainMatchResult(
      inner, arg.metadata.decided_by_related_website_sets(), result_listener);
}

MATCHER_P(IsExpired, inner, "") {
  return testing::ExplainMatchResult(inner, arg.IsExpired(), result_listener);
}

MATCHER_P(ExpirationIs, inner, "") {
  return testing::ExplainMatchResult(inner, arg.metadata.expiration(),
                                     result_listener);
}

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
  StorageAccessGrantPermissionContextTest() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    features_.InitWithFeaturesAndParameters(enabled, disabled);
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
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));

    CHECK_DEREF(DIPSService::Get(browser_context()))
        .RecordInteractionForTesting(GetRequesterURL());
    permission_context_ =
        std::make_unique<StorageAccessGrantPermissionContext>(profile());
  }

  void TearDown() override {
    permission_context_.reset();
    mock_permission_prompt_factory_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<base::test::TestFuture<ContentSetting>> DecidePermission(
      bool user_gesture) {
    auto future = std::make_unique<base::test::TestFuture<ContentSetting>>();
    permission_context_->DecidePermissionForTesting(
        permissions::PermissionRequestData(permission_context(), CreateFakeID(),
                                           user_gesture, GetRequesterURL(),
                                           GetTopLevelURL()),
        future->GetCallback());
    return future;
  }

  ContentSetting DecidePermissionSync(bool user_gesture) {
    return DecidePermission(user_gesture)->Get();
  }

  ContentSetting RequestPermissionSync() {
    base::test::TestFuture<ContentSetting> future;
    permission_context()->RequestPermission(
        permissions::PermissionRequestData(permission_context(), CreateFakeID(),
                                           /*user_gesture=*/true,
                                           GetRequesterURL()),
        future.GetCallback());

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

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::ScopedFeatureList features_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<StorageAccessGrantPermissionContext> permission_context_;
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
  permissions::PermissionRequestID::RequestLocalId::Generator
      request_id_generator_;
  first_party_sets::ScopedMockFirstPartySetsHandler first_party_sets_handler_;
};

TEST_F(StorageAccessGrantPermissionContextTest, InsecureOriginsDisallowed) {
  GURL insecure_url = GURL("http://www.example.com");
  EXPECT_FALSE(permission_context()->IsPermissionAvailableToOrigins(
      insecure_url, insecure_url));
  EXPECT_FALSE(permission_context()->IsPermissionAvailableToOrigins(
      insecure_url, GetRequesterURL()));

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

// Test that after a successful explicit storage access grant, there's a content
// setting that applies on an (embedded site, top-level site) scope.
TEST_F(StorageAccessGrantPermissionContextTest,
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
TEST_F(StorageAccessGrantPermissionContextTest, PermissionDecided) {
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
TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionDeniedWithoutUserGesture) {
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            DecidePermissionSync(/*user_gesture=*/false));
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kDeniedByPrerequisites, 1);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

TEST_F(StorageAccessGrantPermissionContextTest, PermissionGrantReused) {
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

TEST_F(StorageAccessGrantPermissionContextTest, BlockReused) {
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

TEST_F(StorageAccessGrantPermissionContextTest, FpsGrantReused) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  content_settings::ContentSettingConstraints constraints;
  constraints.set_decided_by_related_website_sets(true);
  map->SetContentSettingDefaultScope(GetRequesterURL(), GetTopLevelURL(),
                                     ContentSettingsType::STORAGE_ACCESS,
                                     CONTENT_SETTING_ALLOW, constraints);

  RequestPermissionSync();
  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kReusedImplicitGrant, 1);
  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

TEST_F(StorageAccessGrantPermissionContextTest,
       PermissionStatusAsksWhenFeatureEnabled) {
  EXPECT_EQ(PermissionStatus::ASK,
            permission_context()
                ->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                      GetRequesterURL(), GetTopLevelURL())
                .status);
}

// When 3p cookie access is already allowed by user-agent-specific cookie
// settings, request should be allowed without granting an explicit storage
// access permission.
TEST_F(StorageAccessGrantPermissionContextTest, AllowedByCookieSettings) {
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
TEST_F(StorageAccessGrantPermissionContextTest, DeniedByCookieSettings) {
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

class StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest
    : public StorageAccessGrantPermissionContextTest {
 public:
  StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest() {
    StorageAccessGrantPermissionContext::SetImplicitGrantLimitForTesting(5);
  }

  // Helper to request storage access on enough unique embedding_origin GURLs
  // from |requesting_origin| to ensure that all potential implicit grants will
  // be granted.
  void ExhaustImplicitGrants(const GURL& requesting_origin) {
    permissions::PermissionRequestID fake_id = CreateFakeID();

    const int implicit_grant_limit =
        StorageAccessGrantPermissionContext::GetImplicitGrantLimitForTesting();
    base::test::TestFuture<const std::vector<ContentSetting>> future;
    auto barrier = base::BarrierCallback<ContentSetting>(implicit_grant_limit,
                                                         future.GetCallback());
    for (int grant_id = 0; grant_id < implicit_grant_limit; grant_id++) {
      permission_context()->DecidePermissionForTesting(
          permissions::PermissionRequestData(permission_context(), fake_id,
                                             /*user_gesture=*/true,
                                             requesting_origin,
                                             GetDummyEmbeddingUrl(grant_id)),
          barrier);
    }
    ASSERT_TRUE(future.Wait());
    EXPECT_FALSE(request_manager()->IsRequestInProgress());
  }

 private:
};

// Validate that each requesting origin has its own implicit grant limit. If
// the limit for one origin is exhausted it should not affect another.
TEST_F(StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest,
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
      permissions::PermissionRequestData(
          permission_context(), CreateFakeID(), /*user_gesture=*/true,
          alternate_requester_url, GetTopLevelURL()),
      future.GetCallback());

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
TEST_F(StorageAccessGrantPermissionContextAPIWithImplicitGrantsTest,
       ImplicitGrantLimitSiteScoping) {
  histogram_tester().ExpectTotalCount(kGrantIsImplicitHistogram, 0);

  ExhaustImplicitGrants(GetRequesterURL());

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GetDummyEmbeddingUrlWithSubdomain());

  int implicit_grant_limit =
      StorageAccessGrantPermissionContext::GetImplicitGrantLimitForTesting();

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

TEST_F(StorageAccessGrantPermissionContextTest, ExplicitGrantDenial) {
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

TEST_F(StorageAccessGrantPermissionContextTest,
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
  EXPECT_EQ(PermissionStatus::ASK,
            permission_context()
                ->GetPermissionStatus(/*render_frame_host=*/nullptr,
                                      GetRequesterURL(), GetTopLevelURL())
                .status);

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              UnorderedElementsAre(Pair(GetRequesterSite(), false)));
}

TEST_F(StorageAccessGrantPermissionContextTest, ExplicitGrantAccept) {
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

class StorageAccessGrantPermissionContextAPIWithFirstPartySetsTest
    : public StorageAccessGrantPermissionContextTest {
 public:
  StorageAccessGrantPermissionContextAPIWithFirstPartySetsTest() = default;

  void SetUp() override {
    StorageAccessGrantPermissionContextTest::SetUp();

    // Create a FPS with https://requester.example.com as the member and
    // https://embedder.com as the primary.
    first_party_sets_handler_.SetGlobalSets(net::GlobalFirstPartySets(
        base::Version("1.2.3"),
        /*entries=*/
        {{net::SchemefulSite(GetTopLevelURL()),
          {net::FirstPartySetEntry(net::SchemefulSite(GetTopLevelURL()),
                                   net::SiteType::kPrimary, std::nullopt)}},
         {net::SchemefulSite(GetRequesterURL()),
          {net::FirstPartySetEntry(net::SchemefulSite(GetTopLevelURL()),
                                   net::SiteType::kAssociated, 0)}}},
        /*aliases=*/{}));
  }

 private:
  first_party_sets::ScopedMockFirstPartySetsHandler first_party_sets_handler_;
};

TEST_F(StorageAccessGrantPermissionContextAPIWithFirstPartySetsTest,
       ImplicitGrant_AutograntedWithinFPS) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  DCHECK(settings_map);

  // Overriding the clock to test expiry time.
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  settings_map->SetClockForTesting(&test_clock);

  // Check no `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets` exists yet.
  ASSERT_THAT(settings_map->GetSettingsForOneType(
                  ContentSettingsType::STORAGE_ACCESS,
                  content_settings::mojom::SessionModel::DURABLE),
              Each(DecidedByRelatedWebsiteSets(false)));

  EXPECT_EQ(DecidePermissionSync(/*user_gesture=*/true), CONTENT_SETTING_ALLOW);

  histogram_tester().ExpectUniqueSample(
      kRequestOutcomeHistogram, RequestOutcome::kGrantedByFirstPartySet, 1);
  histogram_tester().ExpectUniqueSample(kGrantIsImplicitHistogram,
                                        /*sample=*/true, 1);

  DCHECK(settings_map);
  // Check the `SessionModel::DURABLE` setting with
  // `decided_by_related_website_sets` granted by FPS and its expiry.
  EXPECT_THAT(
      settings_map->GetSettingsForOneType(
          ContentSettingsType::STORAGE_ACCESS,
          content_settings::mojom::SessionModel::DURABLE),
      Contains(AllOf(
          DecidedByRelatedWebsiteSets(true), IsExpired(false),
          ExpirationIs(
              test_clock.Now() +
              permissions::kStorageAccessAPIRelatedWebsiteSetsLifetime))));

  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}

class StorageAccessGrantPermissionContextAPIWithFedCMConnectionTest
    : public StorageAccessGrantPermissionContextTest {
 public:
  StorageAccessGrantPermissionContextAPIWithFedCMConnectionTest() = default;

  void SetUp() override {
    StorageAccessGrantPermissionContextTest::SetUp();

    feature_list_.InitAndEnableFeature(
        blink::features::kFedCmWithStorageAccessAPI);

    FederatedIdentityPermissionContextFactory::GetForProfile(profile())
        ->GrantSharingPermission(
            /*relying_party_requester=*/url::Origin::Create(
                GURL("https://unrelated-site.test")),
            /*relying_party_embedder=*/
            url::Origin::Create(GetTopLevelURL()),
            /*identity_provider=*/url::Origin::Create(GetRequesterURL()),
            "my_account");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(StorageAccessGrantPermissionContextAPIWithFedCMConnectionTest,
       AutoResolveWithConnection) {
  prompt_factory().set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::NONE);

  auto future = DecidePermission(/*user_gesture=*/false);
  // Ensure no prompt is shown.
  ASSERT_FALSE(request_manager()->IsRequestInProgress());
  EXPECT_EQ(CONTENT_SETTING_ALLOW, future->Get());

  histogram_tester().ExpectUniqueSample(kRequestOutcomeHistogram,
                                        RequestOutcome::kAllowedByFedCM, 1);

  EXPECT_THAT(HostContentSettingsMapFactory::GetForProfile(profile())
                  ->GetSettingsForOneType(
                      ContentSettingsType::STORAGE_ACCESS,
                      content_settings::mojom::SessionModel::DURABLE),
              ElementsAre(ContentSettingPatternSource(
                  ContentSettingsPattern::Wildcard(),
                  ContentSettingsPattern::Wildcard(),
                  content_settings::ContentSettingToValue(
                      ContentSetting::CONTENT_SETTING_ASK),
                  content_settings::ProviderType::kDefaultProvider,
                  /*incognito=*/false)));
  EXPECT_THAT(page_specific_content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());
}
