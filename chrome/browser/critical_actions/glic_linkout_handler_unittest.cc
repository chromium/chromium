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
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace critical_actions {
namespace {

using ::testing::_;

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

 private:
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
  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);
  glic::GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass;

  EXPECT_CALL(*mock_glic_service(), Invoke(_)).Times(1);

  CriticalActionEntry entry;
  entry.conversation_id = "c_12345";

  base::test::TestFuture<OpenConversationResult> future;
  GlicLinkoutHandler::GetInstance()->OpenConversation(
      web_contents(), entry,
      glic::mojom::InvocationSource::kHistoryPageChatLinkout,
      future.GetCallback());

  EXPECT_EQ(future.Get(), OpenConversationResult::kSuccess);
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

}  // namespace
}  // namespace critical_actions
