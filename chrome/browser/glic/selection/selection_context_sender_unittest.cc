// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_context_sender.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/selection/selection_context.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/mock_glic_instance.h"
#include "chrome/browser/glic/test_support/mock_glic_instance_coordinator.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace glic {

using ::testing::_;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

class FakePageContextEligibilityHelper
    : public tabs::PageContextEligibilityHelper {
 public:
  explicit FakePageContextEligibilityHelper(tabs::TabInterface& tab)
      : tabs::PageContextEligibilityHelper(tab) {}

  optimization_guide::PageContextEligibilityStatus IsPageContextEligible()
      const override {
    return status_;
  }

  void SetStatus(optimization_guide::PageContextEligibilityStatus status) {
    status_ = status;
  }

  // Runs the callbacks registered with the helper, like a real change does.
  void Notify(optimization_guide::PageContextEligibilityStatus status) {
    NotifyEligibilityChanged(status);
  }

 private:
  optimization_guide::PageContextEligibilityStatus status_ =
      optimization_guide::PageContextEligibilityStatus::kEligible;
};

}  // namespace

class SelectionContextSenderTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/true);
    ChromeRenderViewHostTestHarness::SetUp();

    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&SelectionContextSenderTest::CreateService,
                            base::Unretained(this)));
    GlicKeyedServiceFactory::GetGlicKeyedService(profile(), /*create=*/true);

    ON_CALL(mock_tab_, GetContents()).WillByDefault(Return(web_contents()));
    ON_CALL(mock_tab_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_bwi_));
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_tab_, RegisterWillDiscardContents(_))
        .WillByDefault(
            [this](tabs::TabInterface::WillDiscardContentsCallback cb) {
              return discard_callbacks_.Add(std::move(cb));
            });

    ON_CALL(mock_service_->mock_coordinator(), AddGlobalShowHideCallback(_))
        .WillByDefault([this](base::RepeatingClosure cb) {
          return global_show_hide_callbacks_.Add(std::move(cb));
        });

    eligibility_helper_ =
        std::make_unique<FakePageContextEligibilityHelper>(mock_tab_);
    sender_ = std::make_unique<SelectionContextSender>(mock_tab_);
  }

  void TearDown() override {
    sender_.reset();
    eligibility_helper_.reset();
    mock_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

 protected:
  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto service = std::make_unique<NiceMock<MockGlicKeyedService>>(
        context, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
    mock_service_ = service.get();
    return service;
  }

  void SetPanelShowing(GlicInstance* instance, bool showing) {
    ON_CALL(*mock_service_, GetInstanceForTab(&mock_tab_))
        .WillByDefault(Return(instance));
    ON_CALL(*mock_service_, IsPanelShowingForBrowser(testing::Ref(mock_bwi_)))
        .WillByDefault(Return(showing));
  }

  void NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus status) {
    eligibility_helper_->SetStatus(status);
    eligibility_helper_->Notify(status);
  }

  // The UTF-8 text in a selection context, or "" when it has none.
  static std::string SentText(const mojom::AdditionalContextPtr& context) {
    if (!context || context->parts.empty() || !context->parts[0]->is_data()) {
      return "";
    }
    const auto& bytes = context->parts[0]->get_data()->data;
    return std::string(bytes.begin(), bytes.end());
  }

  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<MockGlicKeyedService> mock_service_ = nullptr;
  tabs::MockTabInterface mock_tab_;
  NiceMock<MockBrowserWindowInterface> mock_bwi_;
  ui::UnownedUserDataHost user_data_host_;
  base::RepeatingClosureList global_show_hide_callbacks_;
  base::RepeatingCallbackList<
      void(tabs::TabInterface*, content::WebContents*, content::WebContents*)>
      discard_callbacks_;
  std::unique_ptr<FakePageContextEligibilityHelper> eligibility_helper_;
  std::unique_ptr<SelectionContextSender> sender_;
};

TEST_F(SelectionContextSenderTest, UsesInstanceWhenShowing) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  ON_CALL(instance, IsLiveMode()).WillByDefault(Return(false));
  SetPanelShowing(&instance, true);

  EXPECT_CALL(*mock_service_, Invoke).Times(0);
  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });

  sender_->SetText(u"Sample selected text");

  ASSERT_TRUE(sent);
  EXPECT_EQ(sent->source, mojom::AdditionalContextSource::kTextSelection);
  ASSERT_EQ(sent->parts.size(), 1u);
  ASSERT_TRUE(sent->parts[0]->is_data());
  EXPECT_EQ(sent->parts[0]->get_data()->mime_type, kMimeTypeGlicSelection);
  EXPECT_EQ(SentText(sent), "Sample selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());

  // Clearing sends an empty context.
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  sender_->SetText(u"");

  ASSERT_TRUE(sent);
  EXPECT_EQ(sent->source, mojom::AdditionalContextSource::kTextSelection);
  EXPECT_TRUE(sent->parts.empty());
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, InvokesWhenInstanceHidden) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(false));
  SetPanelShowing(&instance, true);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke(Field(&GlicInvokeOptions::target,
                                           Field(&Target::live_mode_behavior,
                                                 LiveModeBehavior::kFail))))
      .Times(1);

  sender_->SetText(u"Sample selected text");
}

TEST_F(SelectionContextSenderTest, SkipsShowingLiveModeInstance) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  ON_CALL(instance, IsLiveMode()).WillByDefault(Return(true));
  SetPanelShowing(&instance, true);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);

  sender_->SetText(u"Sample selected text");
  // Nothing reached the panel, so there's nothing to clear later.
  EXPECT_FALSE(sender_->has_sent_selection_context());
  sender_->SetText(u"");
}

TEST_F(SelectionContextSenderTest,
       DoesNotInvokeWhenClearingWithoutShowingInstance) {
  sender_->MarkTextSent(u"Sample selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());

  ON_CALL(*mock_service_, GetInstanceForTab(&mock_tab_))
      .WillByDefault(Return(nullptr));
  EXPECT_CALL(*mock_service_, Invoke).Times(0);

  sender_->SetText(u"");
  EXPECT_FALSE(sender_->has_sent_selection_context());

  sender_->MarkTextSent(u"Sample selected text");
  NiceMock<MockGlicInstance> hidden_instance;
  ON_CALL(hidden_instance, IsShowing()).WillByDefault(Return(false));
  ON_CALL(*mock_service_, GetInstanceForTab(&mock_tab_))
      .WillByDefault(Return(&hidden_instance));
  EXPECT_CALL(hidden_instance, SendAdditionalContext).Times(0);

  sender_->SetText(u"");
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, IneligiblePageSkipsSend) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetPanelShowing(&instance, true);
  eligibility_helper_->SetStatus(
      optimization_guide::PageContextEligibilityStatus::kNotEligible);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);

  sender_->SetText(u"Ineligible text");
  // Nothing was sent, so clearing doesn't push an empty context either.
  EXPECT_FALSE(sender_->has_sent_selection_context());
  sender_->SetText(u"");
}

TEST_F(SelectionContextSenderTest, GlobalShowHideResendsWhenPanelShows) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetPanelShowing(&instance, false);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"Selected text");
  EXPECT_FALSE(sender_->has_sent_selection_context());

  SetPanelShowing(&instance, true);
  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  global_show_hide_callbacks_.Notify();
  EXPECT_EQ(SentText(sent), "Selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, DynamicEligibilityChangeClearsContext) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetPanelShowing(&instance, true);

  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  sender_->SetText(u"Eligible text");
  ASSERT_TRUE(sent);
  EXPECT_EQ(sent->parts.size(), 1u);
  EXPECT_TRUE(sender_->has_sent_selection_context());

  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kNotEligible);
  ASSERT_TRUE(sent);
  EXPECT_TRUE(sent->parts.empty());
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest,
       EligibilityChangePushesContextWhenPanelShowing) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetPanelShowing(&instance, true);
  eligibility_helper_->SetStatus(
      optimization_guide::PageContextEligibilityStatus::kNotEligible);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"Selected text");

  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);
  ASSERT_TRUE(sent);
  EXPECT_EQ(sent->parts.size(), 1u);
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest,
       EligibilityChangeDoesNotPushContextWhenPanelClosed) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(false));
  SetPanelShowing(&instance, false);
  eligibility_helper_->SetStatus(
      optimization_guide::PageContextEligibilityStatus::kNotEligible);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"Selected text");

  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, WillDiscardContentsClearsState) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetPanelShowing(&instance, true);

  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());

  discard_callbacks_.Notify(&mock_tab_, web_contents(), nullptr);
  EXPECT_FALSE(sender_->has_sent_selection_context());
  EXPECT_TRUE(::selection::PageSelection::From(&mock_tab_)->GetText().empty());

  // No-op on show/hide after discard cleared `text_`.
  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  global_show_hide_callbacks_.Notify();
}

TEST_F(SelectionContextSenderTest, IsTheTabsPageSelection) {
  EXPECT_EQ(::selection::PageSelection::From(&mock_tab_), sender_.get());

  sender_->SetText(u"Selected text");
  EXPECT_EQ(::selection::PageSelection::From(&mock_tab_)->GetText(),
            u"Selected text");
}

TEST_F(SelectionContextSenderTest,
       EligibilityChangeDoesNotPushContextWhenNoBrowserWindow) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetPanelShowing(&instance, true);
  // E.g. the tab is between windows while being dragged.
  ON_CALL(mock_tab_, GetBrowserWindowInterface())
      .WillByDefault(Return(nullptr));
  eligibility_helper_->SetStatus(
      optimization_guide::PageContextEligibilityStatus::kNotEligible);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);
  sender_->SetText(u"Selected text");
  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, UnknownEligibilityClearsContext) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetPanelShowing(&instance, true);
  // The helper starts out unknown; get to eligible first.
  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);

  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());

  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kUnknown);
  ASSERT_TRUE(sent);
  EXPECT_TRUE(sent->parts.empty());
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, NoEligibilityHelperSkipsSend) {
  sender_.reset();
  eligibility_helper_.reset();
  sender_ = std::make_unique<SelectionContextSender>(mock_tab_);

  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetPanelShowing(&instance, true);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);
  sender_->SetText(u"Selected text");
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, NoInstanceForTabDoesNotSend) {
  ON_CALL(*mock_service_, GetInstanceForTab(&mock_tab_))
      .WillByDefault(Return(nullptr));
  ON_CALL(*mock_service_, IsPanelShowingForBrowser(testing::Ref(mock_bwi_)))
      .WillByDefault(Return(true));

  EXPECT_CALL(*mock_service_, Invoke).Times(0);
  sender_->SetText(u"Selected text");
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, NoGlicServiceDoesNothing) {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(
      GlicKeyedServiceFactory::GetInstance(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return nullptr;
          }));
  std::unique_ptr<TestingProfile> other_profile = builder.Build();
  std::unique_ptr<content::WebContents> other_contents =
      content::WebContentsTester::CreateTestWebContents(other_profile.get(),
                                                        nullptr);
  tabs::MockTabInterface other_tab;
  ui::UnownedUserDataHost other_host;
  ON_CALL(other_tab, GetContents()).WillByDefault(Return(other_contents.get()));
  ON_CALL(other_tab, GetBrowserWindowInterface())
      .WillByDefault(Return(&mock_bwi_));
  ON_CALL(other_tab, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(other_host));
  FakePageContextEligibilityHelper other_helper(other_tab);

  SelectionContextSender sender(other_tab);
  EXPECT_FALSE(sender.IsPanelShowing());
  sender.SetText(u"Selected text");
  EXPECT_FALSE(sender.has_sent_selection_context());
  sender.MarkTextSent(u"Selected text");
  sender.SetText(u"");
  EXPECT_FALSE(sender.has_sent_selection_context());
}

}  // namespace glic
