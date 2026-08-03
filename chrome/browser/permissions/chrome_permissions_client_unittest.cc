// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chrome_permissions_client.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_request.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"  // nogncheck
#include "chrome/browser/ui/hats/mock_hats_service.h"     // nogncheck
#include "chrome/browser/ui/hats/survey_config.h"         // nogncheck
#include "components/permissions/constants.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_ANDROID)
#include <optional>
#include <string>

#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "extensions/browser/mime_handler/mime_handler_test_helpers.h"
#include "extensions/browser/mime_handler/mock_mime_handler_stream_delegate.h"
#include "extensions/browser/mime_handler/stream_container.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"
#include "url/origin.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_ANDROID)

class ChromePermissionsClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if !BUILDFLAG(IS_ANDROID)
    permissions::PermissionHatsTriggerHelper::SetIsTest();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
#endif
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
  }

#if !BUILDFLAG(IS_ANDROID)
 private:
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
#endif
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(ChromePermissionsClientTest,
       MaybeCreateMessageUINeverQuietForUpgradeToPrecise) {
  auto request = std::make_unique<permissions::MockPermissionRequest>(
      GURL(permissions::MockPermissionRequest::kDefaultOrigin),
      permissions::RequestType::kGeolocation,
      permissions::PermissionRequestGestureType::GESTURE,
      permissions::GeolocationPromptType::kUpgradeToPrecise);

  base::WeakPtr<permissions::PermissionPromptAndroid> dummy_prompt;

  auto* client = ChromePermissionsClient::GetInstance();
  auto message_ui =
      client->MaybeCreateMessageUI(web_contents(), *request, dummy_prompt);

  EXPECT_FALSE(message_ui);
}
#elif BUILDFLAG(ENABLE_EXTENSIONS)
namespace {

constexpr char kExtensionOrigin[] =
    "chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

}  // namespace

// Fabricates a MIME handler OOPIF frame tree -- without loading a real
// extension -- to unit-test `GetEmbeddingOriginOverride()` in isolation.
class ChromePermissionsClientMimeHandlerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void TearDown() override {
    web_contents()->RemoveUserData(
        extensions::mime_handler::MimeHandlerStreamManager::UserDataKey());
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Commits `url` on `host` and ensures the `MimeHandlerStreamManager` exists.
  content::RenderFrameHost* NavigateAndCommit(content::RenderFrameHost* host,
                                              const GURL& url) {
    content::RenderFrameHost* committed =
        content::NavigationSimulator::NavigateAndCommitFromDocument(url, host);
    extensions::mime_handler::MimeHandlerStreamManager::Create(web_contents());
    return committed;
  }

  content::RenderFrameHost* AppendChild(content::RenderFrameHost* parent,
                                        const std::string& name) {
    auto* tester = content::RenderFrameHostTester::For(parent);
    tester->InitializeRenderFrameIfNeeded();
    return tester->AppendChild(name);
  }
};

// The embedding origin must be keyed on the requesting frame's position in the
// MIME handler subtree, not on its origin. A frame inside the extension OOPIF
// subtree resolves to the extension origin; a same-origin frame outside the
// subtree (the outer page that embedded a same-origin PDF) does not -- so it
// cannot inherit the extension's permission attribution. Regression test for
// crbug.com/519078527.
TEST_F(ChromePermissionsClientMimeHandlerTest,
       OnlyRequestersInsideExtensionSubtreeGetExtensionOrigin) {
  // The sample stream's original URL; the embedder must commit to it for the
  // manager to recognize the extension host beneath it.
  const GURL original_url("https://original_url1");

  // embedder(origin A) -> extension host(chrome-extension) -> content(origin
  // A). The embedder is same-origin with the content frame and stands in for
  // the outer page that embedded the PDF.
  content::RenderFrameHost* embedder = AppendChild(main_rfh(), "embedder");
  embedder = NavigateAndCommit(embedder, original_url);

  content::RenderFrameHost* extension_host = AppendChild(embedder, "extension");
  content::OverrideLastCommittedOrigin(
      extension_host, url::Origin::Create(GURL(kExtensionOrigin)));

  content::RenderFrameHost* content_host =
      AppendChild(extension_host, "content");
  // In production the content frame commits to the original URL; fake that
  // committed origin here so the content frame shares the embedder/outer
  // origin -- the exploit precondition. (A real cross-origin navigation under
  // the chrome-extension:// parent isn't representable in this unit harness.)
  content::OverrideLastCommittedOrigin(content_host,
                                       url::Origin::Create(original_url));

  // Register the stream so the manager treats `extension_host` as the extension
  // OOPIF under `embedder` (mirrors what NavigateToExtensionUrl() does in
  // production).
  auto* manager =
      extensions::mime_handler::MimeHandlerStreamManager::FromWebContents(
          web_contents());
  ASSERT_TRUE(manager);
  manager->AddStreamContainer(
      embedder->GetFrameTreeNodeId(), "internal_id",
      extensions::mime_handler::GenerateSampleStreamContainer(1),
      std::make_unique<testing::NiceMock<
          extensions::mime_handler::MockMimeHandlerStreamDelegate>>());
  manager->ClaimStreamInfoForTesting(embedder);
  manager->SetExtensionFrameTreeNodeIdForTesting(
      embedder, extension_host->GetFrameTreeNodeId());
  ASSERT_TRUE(manager->IsExtensionHost(extension_host));

  auto* client = ChromePermissionsClient::GetInstance();

  // The embedder (outer page) and the content frame committed to the same
  // origin; only their frame-tree position differs.
  ASSERT_EQ(embedder->GetLastCommittedOrigin(),
            content_host->GetLastCommittedOrigin());

  // Requester inside the subtree (the content frame) is attributed to the
  // extension.
  std::optional<GURL> inside = client->GetEmbeddingOriginOverride(
      content_host->GetLastCommittedOrigin().GetURL(), content_host);
  ASSERT_TRUE(inside.has_value());
  EXPECT_EQ(GURL(kExtensionOrigin), *inside);

  // Same-origin requester outside the subtree (the embedder/outer page) is not
  // overridden, so it keeps its own origin and cannot inherit the grant.
  std::optional<GURL> outside = client->GetEmbeddingOriginOverride(
      embedder->GetLastCommittedOrigin().GetURL(), embedder);
  EXPECT_FALSE(outside.has_value());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromePermissionsClientTest, HaTSUrlReportedOnlyIfOptedIn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{permissions::features::kPermissionsPromptSurvey,
        {{"probability_vector", "1.0"},
         {"survey_display_time", "OnPromptResolved"},
         {"trigger_id", "pqEK9eaX30ugnJ3q1cK0UsVJTo1z"}}}},
      {});

  HatsServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildMockHatsService));
  auto* mock_hats_service =
      static_cast<MockHatsService*>(HatsServiceFactory::GetForProfile(
          profile(), /*create_if_necessary=*/true));

  GURL kTestUrl("about:blank");
  NavigateAndCommit(kTestUrl);

  // Case 1: User is opted out of URL-keyed anonymized data collection.
  profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  EXPECT_CALL(*mock_hats_service,
              LaunchSurvey(kHatsSurveyTriggerPermissionsPrompt, testing::_,
                           testing::_, testing::_,
                           testing::Contains(testing::Pair(
                               permissions::kPermissionPromptSurveyUrlKey, "")),
                           testing::_, testing::_));

  ChromePermissionsClient::GetInstance()->TriggerPromptHatsSurveyIfEnabled(
      web_contents(), permissions::RequestType::kNotifications,
      permissions::PermissionAction::GRANTED,
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
      permissions::PermissionPromptDispositionReason::DEFAULT_FALLBACK,
      permissions::PermissionRequestGestureType::GESTURE, base::Minutes(1),
      /*is_post_prompt=*/true, kTestUrl, std::nullopt, CONTENT_SETTING_DEFAULT,
      base::DoNothing(), std::monostate());

  testing::Mock::VerifyAndClearExpectations(mock_hats_service);

  // Case 2: User is opted in to URL-keyed anonymized data collection.
  profile()->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  EXPECT_CALL(*mock_hats_service,
              LaunchSurvey(kHatsSurveyTriggerPermissionsPrompt, testing::_,
                           testing::_, testing::_,
                           testing::Contains(testing::Pair(
                               permissions::kPermissionPromptSurveyUrlKey,
                               kTestUrl.spec())),
                           testing::_, testing::_));

  ChromePermissionsClient::GetInstance()->TriggerPromptHatsSurveyIfEnabled(
      web_contents(), permissions::RequestType::kNotifications,
      permissions::PermissionAction::GRANTED,
      permissions::PermissionPromptDisposition::ANCHORED_BUBBLE,
      permissions::PermissionPromptDispositionReason::DEFAULT_FALLBACK,
      permissions::PermissionRequestGestureType::GESTURE, base::Minutes(1),
      /*is_post_prompt=*/true, kTestUrl, std::nullopt, CONTENT_SETTING_DEFAULT,
      base::DoNothing(), std::monostate());
}

TEST_F(ChromePermissionsClientTest, CanBypassEmbeddingOriginCheckWebUI) {
  auto* client = ChromePermissionsClient::GetInstance();
  GURL dummy_requesting("about:blank");

  // NTP / New Tab:
  EXPECT_TRUE(client->CanBypassEmbeddingOriginCheck(
      dummy_requesting, chrome::ChromeUINewTabURLAsGURL()));
  EXPECT_TRUE(client->CanBypassEmbeddingOriginCheck(
      dummy_requesting, chrome::ChromeUINewTabPageURLAsGURL()));

  // Omnibox Popup, Omnibox Everywhere & Contextual Tasks:
  EXPECT_TRUE(client->CanBypassEmbeddingOriginCheck(
      dummy_requesting, GURL(chrome::kChromeUIOmniboxPopupURL)));
  EXPECT_TRUE(client->CanBypassEmbeddingOriginCheck(
      dummy_requesting, GURL(chrome::kChromeUIOmniboxEverywhereURL)));
  EXPECT_TRUE(client->CanBypassEmbeddingOriginCheck(
      dummy_requesting, GURL(chrome::kChromeUIContextualTasksURL)));

  // Non-WebUI origin should not bypass:
  EXPECT_FALSE(client->CanBypassEmbeddingOriginCheck(dummy_requesting,
                                                     GURL("about:blank")));
}

// Test that GURL is converted to origin so that way subpaths are ignored
// and only identity is compared.
TEST_F(ChromePermissionsClientTest,
       CanBypassEmbeddingOriginCheckWebUIWithSubpaths) {
  auto* client = ChromePermissionsClient::GetInstance();
  GURL dummy_requesting("about:blank");

  // New Tab, NTP, Omnibox Popup, and Contextual Tasks with subpaths/query
  // strings:
  GURL newtab_subpath =
      chrome::ChromeUINewTabURLAsGURL().Resolve("subpath/page?param=1#hash");
  GURL ntp_subpath = GURL(chrome::ChromeUINewTabPageURLAsGURL().spec())
                         .Resolve("subpath/page?param=1#hash");
  GURL omnibox_subpath = GURL(chrome::kChromeUIOmniboxPopupURL)
                             .Resolve("subpath/page?param=1#hash");
  GURL contextual_tasks_subpath = GURL(chrome::kChromeUIContextualTasksURL)
                                      .Resolve("subpath/page?param=1#hash");

  EXPECT_TRUE(
      client->CanBypassEmbeddingOriginCheck(dummy_requesting, newtab_subpath));
  EXPECT_TRUE(
      client->CanBypassEmbeddingOriginCheck(dummy_requesting, ntp_subpath));
  EXPECT_TRUE(
      client->CanBypassEmbeddingOriginCheck(dummy_requesting, omnibox_subpath));
  EXPECT_TRUE(client->CanBypassEmbeddingOriginCheck(dummy_requesting,
                                                    contextual_tasks_subpath));
}

TEST_F(ChromePermissionsClientTest, GetCanonicalOriginOverrideWebUI) {
  auto* client = ChromePermissionsClient::GetInstance();

  GURL ntp_url = chrome::ChromeUINewTabPageURLAsGURL();
  GURL newtab_url = chrome::ChromeUINewTabURLAsGURL();
  GURL omnibox_url(chrome::kChromeUIOmniboxPopupURL);
  GURL contextual_tasks_url(chrome::kChromeUIContextualTasksURL);

  // NTP embedder + NTP requester -> Overridden to DSE (Google) origin:
  std::optional<GURL> ntp_override =
      client->GetCanonicalOriginOverride(ntp_url, newtab_url);
  EXPECT_TRUE(ntp_override.has_value());
  EXPECT_EQ(ntp_override->host(), "www.google.com");

  // Omnibox popup embedder + requester -> Overridden to DSE (Google) origin:
  std::optional<GURL> omnibox_override =
      client->GetCanonicalOriginOverride(omnibox_url, omnibox_url);
  EXPECT_TRUE(omnibox_override.has_value());
  EXPECT_EQ(omnibox_override->host(), "www.google.com");

  // Omnibox everywhere embedder + requester -> Overridden to DSE (Google)
  // origin:
  GURL omnibox_everywhere_url(chrome::kChromeUIOmniboxEverywhereURL);
  std::optional<GURL> everywhere_override = client->GetCanonicalOriginOverride(
      omnibox_everywhere_url, omnibox_everywhere_url);
  EXPECT_TRUE(everywhere_override.has_value());
  EXPECT_EQ(everywhere_override->host(), "www.google.com");

  // Contextual tasks embedder + requester -> Overridden to DSE (Google) origin:
  std::optional<GURL> contextual_override = client->GetCanonicalOriginOverride(
      contextual_tasks_url, contextual_tasks_url);
  EXPECT_TRUE(contextual_override.has_value());
  EXPECT_EQ(contextual_override->host(), "www.google.com");

  // Unmatched requester:
  GURL other_url("about:blank");
  std::optional<GURL> other_override =
      client->GetCanonicalOriginOverride(other_url, newtab_url);
  EXPECT_EQ(other_override, other_url);
}

// Test that GURL is converted to origin so that way subpaths are ignored
// and only identity is compared.
TEST_F(ChromePermissionsClientTest,
       GetCanonicalOriginOverrideWebUIWithSubpaths) {
  auto* client = ChromePermissionsClient::GetInstance();

  GURL newtab_subpath =
      chrome::ChromeUINewTabURLAsGURL().Resolve("subpath/page?param=1#hash");

  // Add trailing slash via spec since `ChromeUINewTabPageAsGURL()` does not
  // have trailing slash.
  GURL ntp_subpath = GURL(chrome::ChromeUINewTabPageURLAsGURL().spec())
                         .Resolve("subpath/page?param=1#hash");
  GURL omnibox_subpath = GURL(chrome::kChromeUIOmniboxPopupURL)
                             .Resolve("subpath/page?param=1#hash");
  GURL contextual_tasks_subpath = GURL(chrome::kChromeUIContextualTasksURL)
                                      .Resolve("subpath/page?param=1#hash");

  // NTP subpath embedder + NTP subpath requester -> Overridden to DSE (Google)
  // origin:
  std::optional<GURL> ntp_override =
      client->GetCanonicalOriginOverride(ntp_subpath, newtab_subpath);
  EXPECT_TRUE(ntp_override.has_value());
  EXPECT_EQ(ntp_override->host(), "www.google.com");

  // Omnibox popup subpath embedder + requester -> Overridden to DSE (Google)
  // origin:
  std::optional<GURL> omnibox_override =
      client->GetCanonicalOriginOverride(omnibox_subpath, omnibox_subpath);
  EXPECT_TRUE(omnibox_override.has_value());
  EXPECT_EQ(omnibox_override->host(), "www.google.com");

  // Contextual tasks subpath embedder + requester -> Overridden to DSE (Google)
  // origin:
  std::optional<GURL> contextual_override = client->GetCanonicalOriginOverride(
      contextual_tasks_subpath, contextual_tasks_subpath);
  EXPECT_TRUE(contextual_override.has_value());
  EXPECT_EQ(contextual_override->host(), "www.google.com");
}

// Test that GURL is converted to origin so that way subpaths are ignored
// and only identity is compared.
TEST_F(ChromePermissionsClientTest, IsFromNewTabPageWebUIWithSubpaths) {
  auto* client = ChromePermissionsClient::GetInstance();

  // Add trailing slash via spec since `ChromeUINewTabPageAsGURL()` does not
  // have trailing slash.
  GURL ntp_subpath = GURL(chrome::ChromeUINewTabPageURLAsGURL().spec())
                         .Resolve("subpath/page?param=1#hash");
  GURL newtab_subpath =
      chrome::ChromeUINewTabURLAsGURL().Resolve("subpath/page?param=1#hash");

  content::OverrideLastCommittedOrigin(web_contents()->GetPrimaryMainFrame(),
                                       url::Origin::Create(ntp_subpath));

  EXPECT_TRUE(client->IsFromNewTabPage(web_contents(), ntp_subpath,
                                       /*already_overrode_requester=*/false));
  EXPECT_TRUE(client->IsFromNewTabPage(web_contents(), newtab_subpath,
                                       /*already_overrode_requester=*/false));
}

// Test that GURL is converted to origin so that way subpaths are ignored
// and only identity is compared.
TEST_F(ChromePermissionsClientTest, IsPrivilegedInternalWebUIWithSubpaths) {
  auto* client = ChromePermissionsClient::GetInstance();

  GURL omnibox_subpath = GURL(chrome::kChromeUIOmniboxPopupURL)
                             .Resolve("subpath/page?param=1#hash");
  GURL contextual_tasks_subpath = GURL(chrome::kChromeUIContextualTasksURL)
                                      .Resolve("subpath/page?param=1#hash");

  content::OverrideLastCommittedOrigin(web_contents()->GetPrimaryMainFrame(),
                                       url::Origin::Create(omnibox_subpath));
  EXPECT_TRUE(client->IsPrivilegedInternalWebUI(
      web_contents(), omnibox_subpath, /*already_overrode_requester=*/false));
  EXPECT_TRUE(client->IsPrivilegedInternalWebUIForUIRouting(web_contents()));

  GURL everywhere_subpath = GURL(chrome::kChromeUIOmniboxEverywhereURL)
                                .Resolve("subpath/page?param=1#hash");
  content::OverrideLastCommittedOrigin(web_contents()->GetPrimaryMainFrame(),
                                       url::Origin::Create(everywhere_subpath));
  EXPECT_TRUE(
      client->IsPrivilegedInternalWebUI(web_contents(), everywhere_subpath,
                                        /*already_overrode_requester=*/false));
  EXPECT_TRUE(client->IsPrivilegedInternalWebUIForUIRouting(web_contents()));

  content::OverrideLastCommittedOrigin(
      web_contents()->GetPrimaryMainFrame(),
      url::Origin::Create(contextual_tasks_subpath));
  EXPECT_TRUE(client->IsPrivilegedInternalWebUI(
      web_contents(), contextual_tasks_subpath,
      /*already_overrode_requester=*/false));
  EXPECT_TRUE(client->IsPrivilegedInternalWebUIForUIRouting(web_contents()));
}
#endif  // !BUILDFLAG(IS_ANDROID)
