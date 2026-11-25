// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/critical_action_ui_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace critical_actions {
namespace {

using ::testing::_;
using ::testing::Return;

// Android settings has no /contactInfo route, so it links to /addresses.
#if BUILDFLAG(IS_ANDROID)
constexpr std::string_view kExpectedContactInfoSubPage =
    chrome::kAddressesSubPage;
#else
constexpr std::string_view kExpectedContactInfoSubPage =
    chrome::kContactInfoSubPage;
#endif

TEST(CriticalActionUiUtilsTest, PasswordManagerLinkoutDomainExtraction) {
  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kCredentialAccess,
                                  GURL("https://www.example.com/login")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/example.com"}));

  EXPECT_EQ(GetCriticalActionLinkoutUrl(
                ActionType::kCredentialAccess,
                GURL("https://login.example.co.uk/auth")),
            base::StrCat(
                {GetGooglePasswordManagerSubPageURLStr(), "/example.co.uk"}));

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kCredentialAccess,
                                  GURL("http://localhost:8080/login")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/localhost"}));

  EXPECT_EQ(GetCriticalActionLinkoutUrl(ActionType::kCredentialAccess, GURL()),
            chrome::kChromeUIPasswordManagerURL);
}

TEST(CriticalActionUiUtilsTest, LinkoutsForActionTypes) {
  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kFormFill,
                                  GURL("https://www.example.com/checkout")),
      base::StrCat(
          {chrome::kChromeUISettingsURL, kExpectedContactInfoSubPage}));

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kFormFillContactInfo,
                                  GURL("https://www.example.com/checkout")),
      base::StrCat(
          {chrome::kChromeUISettingsURL, kExpectedContactInfoSubPage}));

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kFormFillPayments,
                                  GURL("https://www.example.com/checkout")),
      base::StrCat({chrome::kChromeUISettingsURL, chrome::kPaymentsSubPage}));

  EXPECT_EQ(GetCriticalActionLinkoutUrl(ActionType::kDownload,
                                        GURL("https://www.example.com/file")),
            chrome::kChromeUIDownloadsURL);

  EXPECT_EQ(GetCriticalActionLinkoutUrl(
                ActionType::kSettingChange,
                GURL("https://docs.google.com/document/d/123")),
            net::AppendQueryParameter(
                GURL(base::StrCat({chrome::kChromeUISettingsURL,
                                   chrome::kSiteDetailsSubpage})),
                "site", "https://docs.google.com")
                .spec());

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kGooglePasswordManager,
                                  GURL("https://www.example.com/login")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/example.com"}));

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kFederatedLogin,
                                  GURL("https://www.example.com/auth")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/example.com"}));

  EXPECT_EQ(
      GetCriticalActionLinkoutUrl(ActionType::kCredentialsOtp,
                                  GURL("https://www.example.com/otp")),
      base::StrCat({GetGooglePasswordManagerSubPageURLStr(), "/example.com"}));

  EXPECT_EQ(GetCriticalActionLinkoutUrl(ActionType::kWebMcpTool,
                                        GURL("https://www.example.com/tool")),
            std::nullopt);

  EXPECT_EQ(GetCriticalActionLinkoutUrl(ActionType::kUnknown,
                                        GURL("https://www.example.com/")),
            chrome::kChromeUISettingsURL);
}

// Regression test: an empty URL yields an opaque origin, which serialises to
// the literal string "null". Without a guard this produces a site details page
// for a site that does not exist.
TEST(CriticalActionUiUtilsTest, SettingChangeWithEmptyUrlDoesNotLinkToNull) {
  const std::optional<std::string> linkout =
      GetCriticalActionLinkoutUrl(ActionType::kSettingChange, GURL());

  EXPECT_EQ(linkout, chrome::kChromeUISettingsURL);
  EXPECT_NE(linkout,
            net::AppendQueryParameter(
                GURL(base::StrCat({chrome::kChromeUISettingsURL,
                                   chrome::kSiteDetailsSubpage})),
                "site", "null")
                .spec());
}

class OpenCriticalActionConversationTest : public testing::Test {
 protected:
  explicit OpenCriticalActionConversationTest(
      bool action_level_chat_linkouts_enabled = false) {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kCriticalActionHistory,
        {{"enable_chat_linkouts", "true"},
         {"enable_action_level_chat_linkouts",
          action_level_chat_linkouts_enabled ? "true" : "false"}});
  }

  void SetUp() override { profile_ = TestingProfile::Builder().Build(); }

  void TearDown() override { profile_.reset(); }

  // Adds a critical action with a conversation (but no anchor) to the
  // profile's `CriticalActionService`.
  void AddStoredCriticalAction(const std::string& id) {
    CriticalActionService* service =
        CriticalActionFactory::GetForProfile(profile_.get());
    ASSERT_NE(service, nullptr);
    CriticalActionEntry entry;
    entry.critical_action_id = id;
    entry.visit_id = 42;
    entry.timestamp = base::Time::Now();
    entry.action_source = ActionSource::kActor;
    entry.action_type = ActionType::kFormFill;
    entry.conversation_id = "conv-123";
    service->AddCriticalAction(entry);
  }

  // Runs `OpenCriticalActionConversation()` without a WebContents and returns
  // its result.
  OpenConversationResult OpenConversation(Profile* profile,
                                          const std::string& id,
                                          OpenConversationSource source) {
    base::test::TestFuture<OpenConversationResult> future;
    OpenCriticalActionConversation(profile, /*web_contents=*/nullptr, id,
                                   source, future.GetCallback());
    return future.Get();
  }

  // Runs `OpenCriticalActionConversationForTab()` on `tab` and returns its
  // result.
  OpenConversationResult OpenConversationForTab(Profile* profile,
                                                tabs::TabInterface* tab,
                                                const std::string& id,
                                                OpenConversationSource source) {
    base::test::TestFuture<OpenConversationResult> future;
    OpenCriticalActionConversationForTab(profile, tab, id, source,
                                         future.GetCallback());
    return future.Get();
  }

  Profile* profile() { return profile_.get(); }

 private:
  // Declared before `task_environment_` so that it outlives it.
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

class OpenCriticalActionConversationActionLevelTest
    : public OpenCriticalActionConversationTest {
 protected:
  OpenCriticalActionConversationActionLevelTest()
      : OpenCriticalActionConversationTest(
            /*action_level_chat_linkouts_enabled=*/true) {}
};

TEST_F(OpenCriticalActionConversationTest, EmptyIdIsInvalidActionEntry) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(OpenConversation(profile(), /*id=*/"",
                             OpenConversationSource::kEntryMenu),
            OpenConversationResult::kErrorInvalidActionEntry);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.EntryMenu",
      OpenConversationResult::kErrorInvalidActionEntry, 1);
}

TEST_F(OpenCriticalActionConversationTest, NullProfileIsInternalError) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(OpenConversation(/*profile=*/nullptr, "action-1",
                             OpenConversationSource::kEntryMenu),
            OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.EntryMenu",
      OpenConversationResult::kErrorInternal, 1);
}

TEST_F(OpenCriticalActionConversationTest, MissingActionIsInvalidActionEntry) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(OpenConversation(profile(), "missing-id",
                             OpenConversationSource::kEntryMenu),
            OpenConversationResult::kErrorInvalidActionEntry);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.EntryMenu",
      OpenConversationResult::kErrorInvalidActionEntry, 1);
}

TEST_F(OpenCriticalActionConversationTest, NullWebContentsIsInternalError) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));

  base::HistogramTester histogram_tester;
  // Without a WebContents, `GlicLinkoutHandler` fails cleanly with
  // `kErrorInternal`, which is recorded.
  EXPECT_EQ(OpenConversation(profile(), "stored-action-id",
                             OpenConversationSource::kEntryMenu),
            OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.EntryMenu",
      OpenConversationResult::kErrorInternal, 1);
}

// Opening a conversation stored in one profile on another profile's tab would
// leak it across profiles, so a mismatch is a fatal error.
TEST_F(OpenCriticalActionConversationTest, WebContentsFromOtherProfileCrashes) {
  content::RenderViewHostTestEnabler rvh_test_enabler;
  std::unique_ptr<TestingProfile> other_profile =
      TestingProfile::Builder().Build();
  std::unique_ptr<content::WebContents> other_web_contents =
      content::WebContentsTester::CreateTestWebContents(other_profile.get(),
                                                        /*instance=*/nullptr);

  EXPECT_CHECK_DEATH(OpenCriticalActionConversation(
      profile(), other_web_contents.get(), "action-1",
      OpenConversationSource::kEntryMenu, base::DoNothing()));
}

TEST_F(OpenCriticalActionConversationTest,
       FromRowWithActionLevelLinkoutsDisabled) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(OpenConversation(profile(), "stored-action-id",
                             OpenConversationSource::kCriticalActionRow),
            OpenConversationResult::kErrorInternal);
  // Still recorded under the requesting source, even though it is served like
  // the entry menu while action-level chat linkouts are disabled.
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.CriticalActionRow",
      OpenConversationResult::kErrorInternal, 1);
  histogram_tester.ExpectTotalCount(
      "CriticalActions.OpenConversationResult.EntryMenu", 0);
  histogram_tester.ExpectTotalCount(
      "CriticalActions.OpenConversation.HasConversationAnchor", 0);
}

TEST_F(OpenCriticalActionConversationActionLevelTest,
       HasAnchorNotRecordedForEntryMenu) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(OpenConversation(profile(), "stored-action-id",
                             OpenConversationSource::kEntryMenu),
            OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectTotalCount(
      "CriticalActions.OpenConversation.HasConversationAnchor", 0);
}

TEST_F(OpenCriticalActionConversationActionLevelTest, HasAnchorRecordedForRow) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(OpenConversation(profile(), "stored-action-id",
                             OpenConversationSource::kCriticalActionRow),
            OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversation.HasConversationAnchor", false, 1);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.CriticalActionRow",
      OpenConversationResult::kErrorInternal, 1);
}

TEST_F(OpenCriticalActionConversationTest, ForTabRecordsResult) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));

  base::HistogramTester histogram_tester;
  base::test::TestFuture<OpenConversationResult> future;
  OpenCriticalActionConversationForTab(
      profile(), /*tab=*/nullptr, "stored-action-id",
      OpenConversationSource::kEntryMenu, future.GetCallback());
  // The tab variant goes through the same lookup and records its result like
  // the WebContents one. Glic isn't enabled for the test profile, so
  // `GlicLinkoutHandler` fails with `kErrorInternal` before resolving any tab;
  // the tab resolution itself isn't exercised here.
  EXPECT_EQ(future.Get(), OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.EntryMenu",
      OpenConversationResult::kErrorInternal, 1);
}

// A tab of another profile, e.g. an Incognito tab handed over by the Android
// history UI, is refused before the action is looked up, instead of crashing or
// opening the conversation in it.
TEST_F(OpenCriticalActionConversationActionLevelTest,
       ForTabFromIncognitoProfileIsInternalError) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));
  tabs::MockTabInterface incognito_tab;
  ON_CALL(incognito_tab, GetProfile())
      .WillByDefault(
          Return(profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(
      OpenConversationForTab(profile(), &incognito_tab, "stored-action-id",
                             OpenConversationSource::kCriticalActionRow),
      OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.CriticalActionRow",
      OpenConversationResult::kErrorInternal, 1);
  // Only recorded once the action is looked up.
  histogram_tester.ExpectTotalCount(
      "CriticalActions.OpenConversation.HasConversationAnchor", 0);
}

TEST_F(OpenCriticalActionConversationActionLevelTest,
       ForTabFromOtherProfileIsInternalError) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));
  std::unique_ptr<TestingProfile> other_profile =
      TestingProfile::Builder().Build();
  tabs::MockTabInterface other_tab;
  ON_CALL(other_tab, GetProfile()).WillByDefault(Return(other_profile.get()));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(OpenConversationForTab(profile(), &other_tab, "stored-action-id",
                                   OpenConversationSource::kCriticalActionRow),
            OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.CriticalActionRow",
      OpenConversationResult::kErrorInternal, 1);
  histogram_tester.ExpectTotalCount(
      "CriticalActions.OpenConversation.HasConversationAnchor", 0);
}

TEST_F(OpenCriticalActionConversationActionLevelTest,
       ForTabFromSameProfileLooksUpAction) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));
  tabs::MockTabInterface tab;
  base::WeakPtrFactory<tabs::TabInterface> tab_weak_factory(&tab);
  ON_CALL(tab, GetProfile()).WillByDefault(Return(profile()));
  ON_CALL(tab, GetWeakPtr()).WillByDefault([&tab_weak_factory] {
    return tab_weak_factory.GetWeakPtr();
  });

  base::HistogramTester histogram_tester;
  // The tab passes the profile check, so the action is looked up. Glic isn't
  // enabled for the test profile, so opening the conversation then fails.
  EXPECT_EQ(OpenConversationForTab(profile(), &tab, "stored-action-id",
                                   OpenConversationSource::kCriticalActionRow),
            OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversation.HasConversationAnchor", false, 1);
}

// Enables Glic with a mock `GlicKeyedService`, so that tests can tell whether,
// and on which tab, a conversation is opened.
class OpenCriticalActionConversationForTabGlicTest
    : public OpenCriticalActionConversationTest {
 protected:
  void SetUp() override {
    OpenCriticalActionConversationTest::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    glic::GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindLambdaForTesting([this](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::make_unique<
              testing::NiceMock<glic::MockGlicKeyedService>>(
              profile, IdentityManagerFactory::GetForProfile(profile),
              profile_manager_->profile_manager(), &glic_profile_manager_,
              nullptr, nullptr);
        }));
    scoped_glic_bypass_.emplace();

    ON_CALL(tab_, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(tab_, GetWeakPtr()).WillByDefault([this] {
      return tab_weak_factory_.GetWeakPtr();
    });
  }

  void TearDown() override {
    scoped_glic_bypass_.reset();
    // Destroys the profile, and with it the mock `GlicKeyedService`, while the
    // profile managers it points to are still alive.
    OpenCriticalActionConversationTest::TearDown();
    profile_manager_.reset();
  }

  glic::MockGlicKeyedService* mock_glic_service() {
    return static_cast<glic::MockGlicKeyedService*>(
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                           /*create=*/true));
  }

  tabs::MockTabInterface tab_;
  base::WeakPtrFactory<tabs::TabInterface> tab_weak_factory_{&tab_};

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  glic::GlicProfileManager glic_profile_manager_;
  std::optional<glic::GlicEnabling::ScopedBypassEnablementChecksForTesting>
      scoped_glic_bypass_;
};

TEST_F(OpenCriticalActionConversationForTabGlicTest, ForTabInvokesGlicOnTab) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));
  std::optional<glic::Target::Surface> surface;
  EXPECT_CALL(*mock_glic_service(), Invoke(_))
      .WillOnce([&surface](glic::GlicInvokeOptions options) {
        surface = options.target.surface;
        return nullptr;
      });

  base::HistogramTester histogram_tester;
  EXPECT_EQ(OpenConversationForTab(profile(), &tab_, "stored-action-id",
                                   OpenConversationSource::kEntryMenu),
            OpenConversationResult::kSuccess);
  ASSERT_TRUE(surface.has_value());
  const auto* tab_handle = std::get_if<tabs::TabHandle>(&*surface);
  ASSERT_NE(tab_handle, nullptr);
  EXPECT_EQ(*tab_handle, tab_.GetHandle());
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.EntryMenu",
      OpenConversationResult::kSuccess, 1);
}

// If the requested tab goes away while the action is looked up, the
// conversation isn't opened at all, rather than on another tab.
TEST_F(OpenCriticalActionConversationForTabGlicTest,
       ForTabDestroyedDuringLookupIsInternalError) {
  ASSERT_NO_FATAL_FAILURE(AddStoredCriticalAction("stored-action-id"));
  EXPECT_CALL(*mock_glic_service(), Invoke(_)).Times(0);

  base::HistogramTester histogram_tester;
  base::test::TestFuture<OpenConversationResult> future;
  OpenCriticalActionConversationForTab(profile(), &tab_, "stored-action-id",
                                       OpenConversationSource::kEntryMenu,
                                       future.GetCallback());
  // The lookup is asynchronous, so it is still in flight here.
  tab_weak_factory_.InvalidateWeakPtrs();

  EXPECT_EQ(future.Get(), OpenConversationResult::kErrorInternal);
  histogram_tester.ExpectUniqueSample(
      "CriticalActions.OpenConversationResult.EntryMenu",
      OpenConversationResult::kErrorInternal, 1);
}

}  // namespace
}  // namespace critical_actions
