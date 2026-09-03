// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/glic_linkout_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/glic_enums.mojom.h"
#include "chrome/test/base/browser_with_test_window_test.h"  // nocheck
#include "chrome/test/base/testing_browser_process.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace critical_actions {
namespace {

using ::testing::_;

class GlicLinkoutHandlerTest : public BrowserWithTestWindowTest {  // nocheck
 public:
  GlicLinkoutHandlerTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    glic::GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&GlicLinkoutHandlerTest::CreateMockGlicService,
                            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateMockGlicService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<testing::NiceMock<glic::MockGlicKeyedService>>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_, nullptr, nullptr);
  }

  glic::MockGlicKeyedService* mock_glic_service() {
    return static_cast<glic::MockGlicKeyedService*>(
        glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                           /*create=*/true));
  }

 private:
  glic::GlicProfileManager glic_profile_manager_;
};

TEST_F(GlicLinkoutHandlerTest, FailsWhenConversationIdIsEmpty) {
  AddTab(browser(), GURL("http://example.com"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  CriticalActionEntry entry;
  entry.conversation_id = "";

  base::test::TestFuture<OpenConversationResult> future;
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      contents, entry, glic::mojom::InvocationSource::kHistoryPageChatLinkout,
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
  AddTab(browser(), GURL("http://example.com"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  base::test::TestFuture<OpenConversationResult> future;
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      contents, entry, glic::mojom::InvocationSource::kHistoryPageChatLinkout,
      future.GetCallback());

  // Glic is disabled by default in standard TestingProfile without flags.
  EXPECT_EQ(future.Get(), OpenConversationResult::kErrorInternal);
}

TEST_F(GlicLinkoutHandlerTest, SucceedsWhenGlicIsEnabledAndTabExists) {
  AddTab(browser(), GURL("http://example.com"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;

  EXPECT_CALL(*mock_glic_service(), Invoke(_)).Times(1);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  base::test::TestFuture<OpenConversationResult> future;
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      contents, entry, glic::mojom::InvocationSource::kHistoryPageChatLinkout,
      future.GetCallback());

  EXPECT_EQ(future.Get(), OpenConversationResult::kSuccess);
}

}  // namespace
}  // namespace critical_actions
