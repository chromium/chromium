// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/glic_linkout_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/glic_enums.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/mock_tab_list_interface.h"  // nogncheck
#include "chrome/browser/tab_list/tab_list_interface.h"       // nogncheck
#include "chrome/browser/ui/android/android_profile_browser_collection_service.h"  // nogncheck
#include "chrome/browser/ui/android/android_profile_browser_collection_service_factory.h"  // nogncheck
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"  // nogncheck
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"  // nogncheck
#endif

namespace critical_actions {
namespace {

using ::testing::_;
using ::testing::Return;

class GlicLinkoutHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  GlicLinkoutHandlerTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    glic::GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&GlicLinkoutHandlerTest::CreateMockGlicService,
                            base::Unretained(this)));
  }

  void TearDown() override {
    scoped_glic_bypass_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
    profile_manager_.reset();
  }

  std::unique_ptr<KeyedService> CreateMockGlicService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<testing::NiceMock<glic::MockGlicKeyedService>>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        profile_manager_->profile_manager(), &glic_profile_manager_, nullptr,
        nullptr);
  }

  glic::MockGlicKeyedService* mock_glic_service() {
    return static_cast<glic::MockGlicKeyedService*>(
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                           /*create=*/true));
  }

  // Arguments of a `GlicKeyedService::Invoke()` call.
  struct CapturedInvoke {
    std::optional<glic::Target::Surface> surface;
    std::optional<glic::ConversationId> conversation;
    std::optional<glic::mojom::InvocationSource> source;
  };

  // Makes `web_contents()` a tab and bypasses the Glic enablement checks, so
  // that `OpenConversation()` gets as far as `Invoke()`.
  void SetUpTabAndEnableGlic() {
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_tab_);
    ON_CALL(mock_tab_, GetProfile()).WillByDefault(Return(profile()));
    scoped_glic_bypass_.emplace();
  }

  // Expects exactly one `Invoke()` call and records its arguments in
  // `captured`.
  void ExpectSingleInvoke(CapturedInvoke& captured) {
    EXPECT_CALL(*mock_glic_service(), Invoke(_))
        .WillOnce([&captured](glic::GlicInvokeOptions options) {
          captured.source = options.GetInvocationSource();
          captured.surface = options.target.surface;
          if (const auto* conversation = std::get_if<glic::ConversationId>(
                  &options.target.conversation)) {
            captured.conversation = *conversation;
          }
          return nullptr;
        });
  }

  OpenConversationResult OpenConversation(
      content::WebContents* web_contents,
      const CriticalActionEntry& entry,
      glic::mojom::InvocationSource source) {
    base::test::TestFuture<OpenConversationResult> future;
    GlicLinkoutHandler::GetInstance()->OpenConversation(
        web_contents, entry, source, future.GetCallback());
    return future.Get();
  }

  OpenConversationResult OpenConversation(
      Profile* profile,
      tabs::TabInterface* tab,
      const CriticalActionEntry& entry,
      glic::mojom::InvocationSource source) {
    base::test::TestFuture<OpenConversationResult> future;
    GlicLinkoutHandler::GetInstance()->OpenConversation(
        profile, tab, entry, source, future.GetCallback());
    return future.Get();
  }

  tabs::MockTabInterface* mock_tab() { return &mock_tab_; }

 private:
  tabs::MockTabInterface mock_tab_;
  std::optional<glic::GlicEnabling::ScopedBypassEnablementChecksForTesting>
      scoped_glic_bypass_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  glic::GlicProfileManager glic_profile_manager_;
};

TEST_F(GlicLinkoutHandlerTest, FailsWhenConversationIdIsEmpty) {
  CriticalActionEntry entry;
  entry.conversation_id = "";

  base::test::TestFuture<OpenConversationResult> future;
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      web_contents(), entry,
      glic::mojom::InvocationSource::kHistoryPageChatLinkout,
      future.GetCallback());

  EXPECT_EQ(future.Get(), OpenConversationResult::kErrorInvalidActionEntry);
}

TEST_F(GlicLinkoutHandlerTest, FailsWhenWebContentsIsNull) {
  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  base::test::TestFuture<OpenConversationResult> future;
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      /*web_contents=*/nullptr, entry,
      glic::mojom::InvocationSource::kHistoryPageChatLinkout,
      future.GetCallback());

  EXPECT_EQ(future.Get(), OpenConversationResult::kErrorInternal);
}

TEST_F(GlicLinkoutHandlerTest, FailsWhenGlicIsDisabledForProfile) {
  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  base::test::TestFuture<OpenConversationResult> future;
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      web_contents(), entry,
      glic::mojom::InvocationSource::kHistoryPageChatLinkout,
      future.GetCallback());

  // Glic is disabled by default in standard TestingProfile without flags.
  EXPECT_EQ(future.Get(), OpenConversationResult::kErrorInternal);
}

TEST_F(GlicLinkoutHandlerTest, SucceedsWhenGlicIsEnabledAndTabExists) {
  SetUpTabAndEnableGlic();
  CapturedInvoke captured;
  ExpectSingleInvoke(captured);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  EXPECT_EQ(
      OpenConversation(web_contents(), entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kSuccess);
  ASSERT_TRUE(captured.conversation.has_value());
  EXPECT_EQ(captured.conversation->conversation_id, "c_12345");
  EXPECT_EQ(captured.source,
            glic::mojom::InvocationSource::kHistoryPageChatLinkout);
}

// The entry-menu linkout is visit-level: it passes no turn ID even if the entry
// has an anchor.
TEST_F(GlicLinkoutHandlerTest, HistoryPageChatLinkoutSourceIgnoresAnchor) {
  SetUpTabAndEnableGlic();
  CapturedInvoke captured;
  ExpectSingleInvoke(captured);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";
  entry.conversation_anchor_id = "r_turn_99";

  EXPECT_EQ(
      OpenConversation(web_contents(), entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kSuccess);
  ASSERT_TRUE(captured.conversation.has_value());
  EXPECT_EQ(captured.conversation->conversation_id, "c_12345");
  EXPECT_EQ(captured.conversation->turn_id, std::nullopt);
  EXPECT_EQ(captured.source,
            glic::mojom::InvocationSource::kHistoryPageChatLinkout);
}

// Sources other than the critical action row linkout also pass no turn ID.
TEST_F(GlicLinkoutHandlerTest, UnrelatedSourceIgnoresAnchor) {
  SetUpTabAndEnableGlic();
  CapturedInvoke captured;
  ExpectSingleInvoke(captured);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";
  entry.conversation_anchor_id = "r_turn_99";

  EXPECT_EQ(OpenConversation(web_contents(), entry,
                             glic::mojom::InvocationSource::kTopChromeButton),
            OpenConversationResult::kSuccess);
  ASSERT_TRUE(captured.conversation.has_value());
  EXPECT_EQ(captured.conversation->conversation_id, "c_12345");
  EXPECT_EQ(captured.conversation->turn_id, std::nullopt);
  EXPECT_EQ(captured.source, glic::mojom::InvocationSource::kTopChromeButton);
}

TEST_F(GlicLinkoutHandlerTest,
       CriticalActionChatLinkoutSourcePassesAnchorAsTurnId) {
  SetUpTabAndEnableGlic();
  CapturedInvoke captured;
  ExpectSingleInvoke(captured);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";
  entry.conversation_anchor_id = "r_turn_99";

  EXPECT_EQ(
      OpenConversation(
          web_contents(), entry,
          glic::mojom::InvocationSource::kHistoryPageCriticalActionChatLinkout),
      OpenConversationResult::kSuccess);
  ASSERT_TRUE(captured.conversation.has_value());
  EXPECT_EQ(captured.conversation->conversation_id, "c_12345");
  EXPECT_EQ(captured.conversation->turn_id, "r_turn_99");
  EXPECT_EQ(
      captured.source,
      glic::mojom::InvocationSource::kHistoryPageCriticalActionChatLinkout);
}

TEST_F(GlicLinkoutHandlerTest,
       CriticalActionChatLinkoutSourceOmitsTurnIdWhenAnchorIsEmpty) {
  SetUpTabAndEnableGlic();
  CapturedInvoke captured;
  ExpectSingleInvoke(captured);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";
  entry.conversation_anchor_id = "";

  EXPECT_EQ(
      OpenConversation(
          web_contents(), entry,
          glic::mojom::InvocationSource::kHistoryPageCriticalActionChatLinkout),
      OpenConversationResult::kSuccess);
  ASSERT_TRUE(captured.conversation.has_value());
  EXPECT_EQ(captured.conversation->conversation_id, "c_12345");
  EXPECT_EQ(captured.conversation->turn_id, std::nullopt);
}

TEST_F(GlicLinkoutHandlerTest, FailsWhenTabInterfaceMissing) {
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  base::test::TestFuture<OpenConversationResult> future;
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      web_contents(), entry,
      glic::mojom::InvocationSource::kHistoryPageChatLinkout,
      future.GetCallback());

  EXPECT_EQ(future.Get(), OpenConversationResult::kErrorInternal);
}

TEST_F(GlicLinkoutHandlerTest, TabOverloadSucceedsWhenGlicIsEnabled) {
  SetUpTabAndEnableGlic();
  CapturedInvoke captured;
  ExpectSingleInvoke(captured);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";
  entry.conversation_anchor_id = "r_turn_99";

  EXPECT_EQ(
      OpenConversation(profile(), mock_tab(), entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kSuccess);
  ASSERT_TRUE(captured.conversation.has_value());
  EXPECT_EQ(captured.conversation->conversation_id, "c_12345");
  // Like the WebContents overload, the visit-level source passes no turn ID.
  EXPECT_EQ(captured.conversation->turn_id, std::nullopt);
  EXPECT_EQ(captured.source,
            glic::mojom::InvocationSource::kHistoryPageChatLinkout);
}

TEST_F(GlicLinkoutHandlerTest, TabOverloadPassesAnchorForCriticalActionRow) {
  SetUpTabAndEnableGlic();
  CapturedInvoke captured;
  ExpectSingleInvoke(captured);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";
  entry.conversation_anchor_id = "r_turn_99";

  EXPECT_EQ(
      OpenConversation(
          profile(), mock_tab(), entry,
          glic::mojom::InvocationSource::kHistoryPageCriticalActionChatLinkout),
      OpenConversationResult::kSuccess);
  ASSERT_TRUE(captured.conversation.has_value());
  EXPECT_EQ(captured.conversation->turn_id, "r_turn_99");
}

TEST_F(GlicLinkoutHandlerTest, TabOverloadFailsWithoutTabOrTabbedWindow) {
  SetUpTabAndEnableGlic();
  EXPECT_CALL(*mock_glic_service(), Invoke(_)).Times(0);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  // Without a tab, only Android falls back to a tabbed browser window, and the
  // profile has none here.
  EXPECT_EQ(
      OpenConversation(profile(), /*tab=*/nullptr, entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kErrorInternal);
}

TEST_F(GlicLinkoutHandlerTest, TabOverloadFailsWithoutProfile) {
  SetUpTabAndEnableGlic();
  EXPECT_CALL(*mock_glic_service(), Invoke(_)).Times(0);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  EXPECT_EQ(
      OpenConversation(/*profile=*/nullptr, mock_tab(), entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kErrorInternal);
}

// A conversation stored in one profile must not be opened in another profile's
// tab.
TEST_F(GlicLinkoutHandlerTest, TabOverloadFailsForIncognitoTab) {
  // Created before the Glic enablement checks are bypassed, as that would make
  // the new profile build a real `GlicKeyedService`.
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  SetUpTabAndEnableGlic();
  ON_CALL(*mock_tab(), GetProfile()).WillByDefault(Return(incognito_profile));
  EXPECT_CALL(*mock_glic_service(), Invoke(_)).Times(0);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  EXPECT_EQ(
      OpenConversation(profile(), mock_tab(), entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kErrorInternal);
}

TEST_F(GlicLinkoutHandlerTest, TabOverloadFailsForTabOfOtherProfile) {
  // Created before the Glic enablement checks are bypassed, as that would make
  // the new profile build a real `GlicKeyedService`.
  std::unique_ptr<TestingProfile> other_profile =
      TestingProfile::Builder().Build();
  SetUpTabAndEnableGlic();
  ON_CALL(*mock_tab(), GetProfile()).WillByDefault(Return(other_profile.get()));
  EXPECT_CALL(*mock_glic_service(), Invoke(_)).Times(0);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  EXPECT_EQ(
      OpenConversation(profile(), mock_tab(), entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kErrorInternal);
}

TEST_F(GlicLinkoutHandlerTest, TabOverloadFailsWhenConversationIdIsEmpty) {
  SetUpTabAndEnableGlic();
  EXPECT_CALL(*mock_glic_service(), Invoke(_)).Times(0);

  CriticalActionEntry entry;

  EXPECT_EQ(
      OpenConversation(profile(), mock_tab(), entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kErrorInvalidActionEntry);
}

#if BUILDFLAG(IS_ANDROID)
// Serves a fixed list of browser windows as the windows of a profile.
class FakeProfileBrowserCollectionService
    : public AndroidProfileBrowserCollectionService {
 public:
  FakeProfileBrowserCollectionService(Profile* profile, BrowserVector browsers)
      : AndroidProfileBrowserCollectionService(profile),
        browsers_(std::move(browsers)) {}

 protected:
  BrowserVector GetBrowsers(Order order) override { return browsers_; }

 private:
  BrowserVector browsers_;
};

// Covers the Android fallback used when the caller has no tab (e.g.
// HistoryActivity).
class GlicLinkoutHandlerWithoutTabTest : public GlicLinkoutHandlerTest {
 protected:
  void SetUp() override {
    GlicLinkoutHandlerTest::SetUp();
    ON_CALL(browser_, GetProfile()).WillByDefault(Return(profile()));
    ON_CALL(browser_, GetType())
        .WillByDefault(Return(BrowserWindowInterface::TYPE_NORMAL));
    tab_list_registration_.emplace(browser_.GetUnownedUserDataHost(),
                                   tab_list_);
    SetUpTabAndEnableGlic();
  }

  void TearDown() override {
    GlicLinkoutHandlerTest::TearDown();
    tab_list_registration_.reset();
  }

  // Makes `browser()` the only tabbed browser window of `profile()`, or leaves
  // the profile without any browser window if `has_window` is false.
  void SetUpBrowserWindows(bool has_window) {
    AndroidProfileBrowserCollectionServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindLambdaForTesting([this, has_window](
                                           content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
              BrowserCollection::BrowserVector browsers;
              if (has_window) {
                browsers.push_back(&browser_);
              }
              return std::make_unique<FakeProfileBrowserCollectionService>(
                  Profile::FromBrowserContext(context), std::move(browsers));
            }));
  }

  BrowserWindowInterface* browser() { return &browser_; }
  MockTabListInterface& tab_list() { return tab_list_; }

 private:
  testing::NiceMock<MockBrowserWindowInterface> browser_;
  testing::NiceMock<MockTabListInterface> tab_list_;
  std::optional<ui::ScopedUnownedUserData<TabListInterface>>
      tab_list_registration_;
};

TEST_F(GlicLinkoutHandlerWithoutTabTest, UsesActiveTabOfTabbedWindow) {
  SetUpBrowserWindows(/*has_window=*/true);
  ON_CALL(tab_list(), GetActiveTab()).WillByDefault(Return(mock_tab()));
  CapturedInvoke captured;
  ExpectSingleInvoke(captured);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  EXPECT_EQ(
      OpenConversation(profile(), /*tab=*/nullptr, entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kSuccess);
  ASSERT_TRUE(captured.surface.has_value());
  const auto* tab_handle = std::get_if<tabs::TabHandle>(&*captured.surface);
  ASSERT_NE(tab_handle, nullptr);
  EXPECT_EQ(*tab_handle, mock_tab()->GetHandle());
  ASSERT_TRUE(captured.conversation.has_value());
  EXPECT_EQ(captured.conversation->conversation_id, "c_12345");
}

TEST_F(GlicLinkoutHandlerWithoutTabTest, OpensNewTabInEmptyWindow) {
  SetUpBrowserWindows(/*has_window=*/true);
  ON_CALL(tab_list(), GetActiveTab()).WillByDefault(Return(nullptr));
  CapturedInvoke captured;
  ExpectSingleInvoke(captured);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  EXPECT_EQ(
      OpenConversation(profile(), /*tab=*/nullptr, entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kSuccess);
  ASSERT_TRUE(captured.surface.has_value());
  const auto* new_tab = std::get_if<glic::NewTab>(&*captured.surface);
  ASSERT_NE(new_tab, nullptr);
  EXPECT_EQ(new_tab->window, browser());
}

TEST_F(GlicLinkoutHandlerWithoutTabTest, FailsWithoutTabbedWindow) {
  SetUpBrowserWindows(/*has_window=*/false);
  EXPECT_CALL(*mock_glic_service(), Invoke(_)).Times(0);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  EXPECT_EQ(
      OpenConversation(profile(), /*tab=*/nullptr, entry,
                       glic::mojom::InvocationSource::kHistoryPageChatLinkout),
      OpenConversationResult::kErrorInternal);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace critical_actions
