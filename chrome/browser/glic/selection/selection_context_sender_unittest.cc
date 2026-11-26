// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_context_sender.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/selection/selection_context.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/fake_glic_side_panel_coordinator.h"
#include "chrome/browser/glic/test_support/mock_glic_instance.h"
#include "chrome/browser/glic/test_support/mock_glic_instance_coordinator.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
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
using ::testing::ElementsAre;
using ::testing::IsEmpty;
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
    ON_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_tab_, RegisterWillDiscardContents(_))
        .WillByDefault(
            [this](tabs::TabInterface::WillDiscardContentsCallback cb) {
              return discard_callbacks_.Add(std::move(cb));
            });
    ON_CALL(mock_tab_, IsActivated()).WillByDefault(Return(true));
    ON_CALL(mock_tab_, RegisterDidActivate(_))
        .WillByDefault([this](tabs::TabInterface::DidActivateCallback cb) {
          return did_activate_callbacks_.Add(std::move(cb));
        });
    ON_CALL(mock_service_->mock_coordinator(), AddGlobalShowHideCallback(_))
        .WillByDefault([this](base::RepeatingClosure cb) {
          return global_show_hide_callbacks_.Add(std::move(cb));
        });
    ON_CALL(mock_service_->mock_coordinator(), AddInvokeFinishedCallback(_))
        .WillByDefault([this](base::RepeatingClosure cb) {
          return invoke_finished_callbacks_.Add(std::move(cb));
        });

    eligibility_helper_ =
        std::make_unique<FakePageContextEligibilityHelper>(mock_tab_);
    side_panel_coordinator_ =
        std::make_unique<FakeGlicSidePanelCoordinator>(&mock_tab_);
    sender_ = std::make_unique<SelectionContextSender>(mock_tab_);
  }

  void TearDown() override {
    sender_.reset();
    side_panel_coordinator_.reset();
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

  void SetInstance(GlicInstance* instance) {
    ON_CALL(*mock_service_, GetInstanceForTab(&mock_tab_))
        .WillByDefault(Return(instance));
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

  // Records the text of every push to `instance`; "" means "none".
  static void RecordPushes(MockGlicInstance& instance,
                           std::vector<std::string>& pushes) {
    ON_CALL(instance, SendAdditionalContext)
        .WillByDefault([&pushes](mojom::AdditionalContextPtr context) {
          pushes.push_back(SentText(context));
        });
  }

  static void SetFloating(MockGlicInstance& instance, bool floating) {
    mojom::PanelState state;
    state.kind = floating ? mojom::PanelStateKind::kDetached
                          : mojom::PanelStateKind::kHidden;
    instance.SetPanelState(std::move(state));
  }

  GlicEnabling::ScopedBypassEnablementChecksForTesting scoped_glic_bypass_;
  GlicProfileManager glic_profile_manager_;
  raw_ptr<MockGlicKeyedService> mock_service_ = nullptr;
  tabs::MockTabInterface mock_tab_;
  ui::UnownedUserDataHost user_data_host_;
  base::RepeatingClosureList global_show_hide_callbacks_;
  base::RepeatingClosureList invoke_finished_callbacks_;
  base::RepeatingCallbackList<
      void(tabs::TabInterface*, content::WebContents*, content::WebContents*)>
      discard_callbacks_;
  base::RepeatingCallbackList<void(tabs::TabInterface*)>
      did_activate_callbacks_;
  std::unique_ptr<FakePageContextEligibilityHelper> eligibility_helper_;
  std::unique_ptr<FakeGlicSidePanelCoordinator> side_panel_coordinator_;
  std::unique_ptr<SelectionContextSender> sender_;
};

TEST_F(SelectionContextSenderTest, UsesInstanceWhenShowing) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  ON_CALL(instance, IsLiveMode()).WillByDefault(Return(false));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

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

// Opening and reopening the panel when no selection was ever sent does not
// push an empty context.
TEST_F(SelectionContextSenderTest, OpenWithNoSelectionPushesNothing) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kClosed);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
}

// Closing the panel, deselecting on the page, and reopening tells "none" so
// the client clears any stale text chip.
TEST_F(SelectionContextSenderTest, CloseDeselectReopenTellsNone) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");

  // Close the panel and deselect on the page: nothing pushed while hidden.
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kClosed);
  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"");

  // Reopen the panel: "none" is pushed so any stale chip is cleared.
  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  ASSERT_TRUE(sent);
  EXPECT_EQ(sent->source, mojom::AdditionalContextSource::kTextSelection);
  EXPECT_TRUE(sent->parts.empty());
}

// Reopening resends a kept selection: the panel may have lost it while closed.
TEST_F(SelectionContextSenderTest, CloseKeepSelectionReopenResends) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");

  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kClosed);
  EXPECT_TRUE(sender_->has_sent_selection_context());

  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  EXPECT_EQ(SentText(sent), "Selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

// If the instance is replaced while the panel is closed, reopening pushes the
// kept selection to the new instance.
TEST_F(SelectionContextSenderTest, NewInstanceAfterKillPushesKeptSelection) {
  NiceMock<MockGlicInstance> old_instance;
  ON_CALL(old_instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&old_instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  EXPECT_CALL(old_instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");

  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kClosed);

  NiceMock<MockGlicInstance> new_instance;
  InstanceId new_id = InstanceId::Create(2, 0);
  ON_CALL(new_instance, id()).WillByDefault(ReturnRef(new_id));
  ON_CALL(new_instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&new_instance);

  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(new_instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  ASSERT_TRUE(sent);
  EXPECT_EQ(sent->parts.size(), 1u);
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

// A backgrounded tab's panel is not shown, so nothing is pushed or invoked
// while away; returning to the tab tells the panel once.
TEST_F(SelectionContextSenderTest, BackgroundedPanelDoesNotSendOrInvoke) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(false));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(
      GlicSidePanelCoordinator::State::kBackgrounded);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);
  sender_->SetText(u"Selected text");

  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
}

// Another tab's side panel toggling doesn't push from a tab whose own side
// panel is backgrounded or closed, even though the shared instance shows.
TEST_F(SelectionContextSenderTest, OtherTabsPanelToggleDoesNotSend) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  sender_->SetText(u"Selected text");

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  side_panel_coordinator_->SetState(
      GlicSidePanelCoordinator::State::kBackgrounded);
  global_show_hide_callbacks_.Notify();
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kClosed);
  global_show_hide_callbacks_.Notify();
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

// The instance an invocation creates already has its text; its show resends
// the same full text, not an elided copy.
TEST_F(SelectionContextSenderTest, MarkTextSentBeforeInstanceExists) {
  SetInstance(nullptr);
  const std::string long_text(200, 'a');
  sender_->MarkTextSent(base::UTF8ToUTF16(long_text),
                        SelectionTextLength::kFull);

  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  std::vector<std::string> pushes;
  RecordPushes(instance, pushes);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  EXPECT_THAT(pushes, ElementsAre(long_text));
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

// A new instance never got the old selection, so deselecting while closed
// pushes nothing to it.
TEST_F(SelectionContextSenderTest, DeselectReplaceInstanceReopenPushesNothing) {
  NiceMock<MockGlicInstance> old_instance;
  ON_CALL(old_instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&old_instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  EXPECT_CALL(old_instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");

  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kClosed);
  sender_->SetText(u"");

  NiceMock<MockGlicInstance> new_instance;
  InstanceId new_id = InstanceId::Create(2, 0);
  ON_CALL(new_instance, id()).WillByDefault(ReturnRef(new_id));
  ON_CALL(new_instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&new_instance);
  EXPECT_CALL(new_instance, SendAdditionalContext).Times(0);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

// The page turning ineligible while the panel is closed clears the chip when
// the panel reopens.
TEST_F(SelectionContextSenderTest, IneligibleWhileClosedClearsChipOnReopen) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");

  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kClosed);
  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kNotEligible);

  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  ASSERT_TRUE(sent);
  EXPECT_TRUE(sent->parts.empty());
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

// Marked text isn't sent again while the panel stays open.
TEST_F(SelectionContextSenderTest, DoesNotResendWhenMarkedAlreadySent) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  sender_->MarkTextSent(u"Selected text", SelectionTextLength::kFull);
  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"Selected text");
  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);
}

TEST_F(SelectionContextSenderTest, SkipsShowingLiveModeInstance) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  ON_CALL(instance, IsLiveMode()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);

  sender_->SetText(u"Sample selected text");
  // Nothing reached the panel, so there's nothing to clear later.
  EXPECT_FALSE(sender_->has_sent_selection_context());
  sender_->SetText(u"");
}

// A show during an invocation sends nothing; the next sync after it does.
TEST_F(SelectionContextSenderTest, SkipsPushWhileInvoking) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  ON_CALL(instance, IsInvoking()).WillByDefault(Return(true));
  SetInstance(&instance);
  sender_->SetText(u"Selected text");

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  EXPECT_FALSE(sender_->has_sent_selection_context());

  ON_CALL(instance, IsInvoking()).WillByDefault(Return(false));
  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

// The panel shown during an invocation gets the selection once it finishes.
TEST_F(SelectionContextSenderTest, InvokeFinishedSendsSelection) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  ON_CALL(instance, IsInvoking()).WillByDefault(Return(true));
  SetInstance(&instance);
  sender_->SetText(u"Selected text");

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  ON_CALL(instance, IsInvoking()).WillByDefault(Return(false));
  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  invoke_finished_callbacks_.Notify();
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

// Deselecting during an invocation clears the chip once it finishes.
TEST_F(SelectionContextSenderTest, DeselectDuringInvokeTellsNoneAfter) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");

  ON_CALL(instance, IsInvoking()).WillByDefault(Return(true));
  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"");

  ON_CALL(instance, IsInvoking()).WillByDefault(Return(false));
  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  invoke_finished_callbacks_.Notify();
  ASSERT_TRUE(sent);
  EXPECT_TRUE(sent->parts.empty());
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, SkipsPushWhenInstanceHiddenOrMissing) {
  SetInstance(nullptr);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);
  sender_->SetText(u"Sample selected text");

  NiceMock<MockGlicInstance> hidden_instance;
  ON_CALL(hidden_instance, IsShowing()).WillByDefault(Return(false));
  SetInstance(&hidden_instance);
  EXPECT_CALL(hidden_instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);
  sender_->SetText(u"Other selected text");
}

TEST_F(SelectionContextSenderTest, IneligiblePageSkipsSend) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  eligibility_helper_->SetStatus(
      optimization_guide::PageContextEligibilityStatus::kNotEligible);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);

  sender_->SetText(u"Ineligible text");
  // Nothing was sent, so clearing doesn't push an empty context either.
  EXPECT_FALSE(sender_->has_sent_selection_context());
  sender_->SetText(u"");
}

TEST_F(SelectionContextSenderTest, DynamicEligibilityChangeClearsContext) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

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
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
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
  SetInstance(&instance);
  eligibility_helper_->SetStatus(
      optimization_guide::PageContextEligibilityStatus::kNotEligible);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"Selected text");

  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kEligible);
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

// Live mode skips the push without recording it, so the same text is sent
// once live mode ends.
TEST_F(SelectionContextSenderTest, ResendsAfterLeavingLiveMode) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  ON_CALL(instance, IsLiveMode()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"Selected text");
  EXPECT_FALSE(sender_->has_sent_selection_context());

  ON_CALL(instance, IsLiveMode()).WillByDefault(Return(false));
  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

// The floating window counts as this tab's panel.
TEST_F(SelectionContextSenderTest, SendsToFloatingWindow) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetFloating(instance, true);
  SetInstance(&instance);

  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

// Showing the floating window tells it the current selection; deselecting
// while hidden tells "none" when it reopens.
TEST_F(SelectionContextSenderTest, FloatingWindowShowHide) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(false));
  SetFloating(instance, false);
  SetInstance(&instance);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"Selected text");

  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetFloating(instance, true);
  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  global_show_hide_callbacks_.Notify();
  EXPECT_TRUE(sender_->has_sent_selection_context());

  ON_CALL(instance, IsShowing()).WillByDefault(Return(false));
  SetFloating(instance, false);
  global_show_hide_callbacks_.Notify();
  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"");

  mojom::AdditionalContextPtr sent;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetFloating(instance, true);
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  global_show_hide_callbacks_.Notify();
  ASSERT_TRUE(sent);
  EXPECT_TRUE(sent->parts.empty());
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

// Side panel churn: each show resends the current selection, and the hidden
// states never send.
TEST_F(SelectionContextSenderTest, SidePanelChurnResendsCurrentSelection) {
  using State = GlicSidePanelCoordinator::State;
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  std::vector<std::string> pushes;
  RecordPushes(instance, pushes);

  side_panel_coordinator_->SetState(State::kShown);
  sender_->SetText(u"A");
  for (State state : {State::kClosed, State::kShown, State::kBackgrounded,
                      State::kShown, State::kClosed, State::kClosed,
                      State::kPeek, State::kShown, State::kShown}) {
    side_panel_coordinator_->SetState(state);
  }
  EXPECT_THAT(pushes, ElementsAre("A", "A", "A", "A", "A"));
}

// Only the selection at the time of the show is sent, never one replaced
// while hidden.
TEST_F(SelectionContextSenderTest, ChangedWhileHiddenSendsOnlyLatest) {
  using State = GlicSidePanelCoordinator::State;
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  std::vector<std::string> pushes;
  RecordPushes(instance, pushes);

  side_panel_coordinator_->SetState(State::kShown);
  sender_->SetText(u"A");
  side_panel_coordinator_->SetState(State::kClosed);
  sender_->SetText(u"B");
  sender_->SetText(u"");
  sender_->SetText(u"C");
  side_panel_coordinator_->SetState(State::kBackgrounded);
  side_panel_coordinator_->SetState(State::kShown);
  EXPECT_THAT(pushes, ElementsAre("A", "C"));
}

// "None" is sent once after a deselect, not on every later show.
TEST_F(SelectionContextSenderTest, DeselectedWhileHiddenTellsNoneOnce) {
  using State = GlicSidePanelCoordinator::State;
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  std::vector<std::string> pushes;
  RecordPushes(instance, pushes);

  side_panel_coordinator_->SetState(State::kShown);
  sender_->SetText(u"A");
  side_panel_coordinator_->SetState(State::kClosed);
  sender_->SetText(u"");
  side_panel_coordinator_->SetState(State::kShown);
  side_panel_coordinator_->SetState(State::kBackgrounded);
  side_panel_coordinator_->SetState(State::kShown);
  side_panel_coordinator_->SetState(State::kShown);
  EXPECT_THAT(pushes, ElementsAre("A", ""));
}

// A side panel shown before its instance shows sends nothing until both are.
TEST_F(SelectionContextSenderTest, ShowBeforeInstanceShowsWaits) {
  using State = GlicSidePanelCoordinator::State;
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(false));
  SetInstance(&instance);
  std::vector<std::string> pushes;
  RecordPushes(instance, pushes);

  sender_->SetText(u"A");
  side_panel_coordinator_->SetState(State::kShown);
  EXPECT_THAT(pushes, IsEmpty());

  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  side_panel_coordinator_->SetState(State::kShown);
  EXPECT_THAT(pushes, ElementsAre("A"));
}

// Eligibility flips across shows: an ineligible page sends "none" once and
// never its text; turning eligible again sends the text.
TEST_F(SelectionContextSenderTest, EligibilityFlipsAcrossShows) {
  using State = GlicSidePanelCoordinator::State;
  using Status = optimization_guide::PageContextEligibilityStatus;
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  std::vector<std::string> pushes;
  RecordPushes(instance, pushes);

  side_panel_coordinator_->SetState(State::kShown);
  sender_->SetText(u"A");
  side_panel_coordinator_->SetState(State::kClosed);
  NotifyEligibilityChanged(Status::kNotEligible);
  side_panel_coordinator_->SetState(State::kShown);
  side_panel_coordinator_->SetState(State::kShown);
  NotifyEligibilityChanged(Status::kEligible);
  side_panel_coordinator_->SetState(State::kClosed);
  side_panel_coordinator_->SetState(State::kShown);
  EXPECT_THAT(pushes, ElementsAre("A", "", "A", "A"));
}

// Live mode swallows shows; the next show after it ends sends the text.
TEST_F(SelectionContextSenderTest, LiveModeAcrossShows) {
  using State = GlicSidePanelCoordinator::State;
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  ON_CALL(instance, IsLiveMode()).WillByDefault(Return(true));
  SetInstance(&instance);
  std::vector<std::string> pushes;
  RecordPushes(instance, pushes);

  sender_->SetText(u"A");
  side_panel_coordinator_->SetState(State::kShown);
  side_panel_coordinator_->SetState(State::kShown);
  EXPECT_THAT(pushes, IsEmpty());

  ON_CALL(instance, IsLiveMode()).WillByDefault(Return(false));
  side_panel_coordinator_->SetState(State::kClosed);
  side_panel_coordinator_->SetState(State::kShown);
  EXPECT_THAT(pushes, ElementsAre("A"));
}

// Shows resend an invocation's text the way it was sent, in full, never an
// elided copy over it. A new selection goes back to elided.
TEST_F(SelectionContextSenderTest, ResendKeepsInvocationTextLength) {
  using State = GlicSidePanelCoordinator::State;
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  std::vector<std::string> pushes;
  RecordPushes(instance, pushes);

  const std::string long_text(200, 'a');
  sender_->MarkTextSent(base::UTF8ToUTF16(long_text),
                        SelectionTextLength::kFull);
  side_panel_coordinator_->SetState(State::kShown);
  side_panel_coordinator_->SetState(State::kClosed);
  side_panel_coordinator_->SetState(State::kShown);
  EXPECT_THAT(pushes, ElementsAre(long_text, long_text));

  const std::string other_text(200, 'b');
  sender_->SetText(base::UTF8ToUTF16(other_text));
  side_panel_coordinator_->SetState(State::kClosed);
  side_panel_coordinator_->SetState(State::kShown);
  ASSERT_EQ(pushes.size(), 4u);
  EXPECT_LT(pushes[2].size(), other_text.size());
  EXPECT_EQ(pushes[3], pushes[2]);
}

// A replaced instance gets the selection on its first show; the old one gets
// nothing more.
TEST_F(SelectionContextSenderTest, InstanceReplacedBetweenShows) {
  using State = GlicSidePanelCoordinator::State;
  NiceMock<MockGlicInstance> old_instance;
  ON_CALL(old_instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&old_instance);
  std::vector<std::string> old_pushes;
  RecordPushes(old_instance, old_pushes);

  side_panel_coordinator_->SetState(State::kShown);
  sender_->SetText(u"A");
  side_panel_coordinator_->SetState(State::kClosed);

  NiceMock<MockGlicInstance> new_instance;
  InstanceId new_id = InstanceId::Create(2, 0);
  ON_CALL(new_instance, id()).WillByDefault(ReturnRef(new_id));
  ON_CALL(new_instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&new_instance);
  std::vector<std::string> new_pushes;
  RecordPushes(new_instance, new_pushes);

  side_panel_coordinator_->SetState(State::kShown);
  EXPECT_THAT(old_pushes, ElementsAre("A"));
  EXPECT_THAT(new_pushes, ElementsAre("A"));
}

// A scripted mix of panel states, selections, eligibility and live mode.
// Whatever the order, every push is the page's selection at that moment (or
// "none" when it has none or is ineligible), and nothing is sent while the
// panel is hidden.
TEST_F(SelectionContextSenderTest, ScriptedChurnNeverSendsStaleSelection) {
  using State = GlicSidePanelCoordinator::State;
  using Status = optimization_guide::PageContextEligibilityStatus;
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);

  std::string selection;
  bool eligible = true;
  bool live = false;
  int pushes = 0;
  ON_CALL(instance, SendAdditionalContext)
      .WillByDefault([&](mojom::AdditionalContextPtr context) {
        ++pushes;
        EXPECT_TRUE(sender_->IsPanelShowing());
        EXPECT_FALSE(live);
        EXPECT_EQ(SentText(context), eligible ? selection : "");
      });

  auto select = [&](const std::string& text) {
    selection = text;
    sender_->SetText(base::UTF8ToUTF16(text));
  };
  auto set_eligible = [&](bool value) {
    eligible = value;
    NotifyEligibilityChanged(value ? Status::kEligible : Status::kNotEligible);
  };
  auto set_live = [&](bool value) {
    live = value;
    ON_CALL(instance, IsLiveMode()).WillByDefault(Return(value));
  };
  auto set_state = [&](State state) {
    side_panel_coordinator_->SetState(state);
  };

  select("A");
  set_state(State::kShown);
  select("B");
  set_state(State::kBackgrounded);
  select("");
  select("C");
  set_eligible(false);
  set_state(State::kShown);
  set_live(true);
  set_eligible(true);
  select("D");
  set_state(State::kClosed);
  set_state(State::kShown);
  set_live(false);
  set_state(State::kPeek);
  set_state(State::kShown);
  select("");
  set_state(State::kShown);
  set_state(State::kClosed);
  select("E");
  set_eligible(false);
  set_eligible(true);
  set_state(State::kShown);
  EXPECT_GT(pushes, 0);
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

// Several tabs can share one floating window, so only the active tab tells it
// its selection.
TEST_F(SelectionContextSenderTest, FloatingWindowOnlyFromActiveTab) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetFloating(instance, true);
  SetInstance(&instance);
  ON_CALL(mock_tab_, IsActivated()).WillByDefault(Return(false));

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  sender_->SetText(u"Selected text");
  global_show_hide_callbacks_.Notify();
  EXPECT_FALSE(sender_->has_sent_selection_context());

  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  ON_CALL(mock_tab_, IsActivated()).WillByDefault(Return(true));
  did_activate_callbacks_.Notify(&mock_tab_);
  EXPECT_EQ(SentText(sent), "Selected text");
  EXPECT_TRUE(sender_->has_sent_selection_context());
}

// Another tab may have put its chip in the shared floating window, so
// returning to a tab tells it this tab's selection, even "none".
TEST_F(SelectionContextSenderTest, FloatingWindowRetellsOnTabActivation) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetFloating(instance, true);
  SetInstance(&instance);

  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  did_activate_callbacks_.Notify(&mock_tab_);
  ASSERT_TRUE(sent);
  EXPECT_TRUE(sent->parts.empty());

  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");

  // Another tab replaces the chip while this one is away; coming back resends
  // the same text.
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  did_activate_callbacks_.Notify(&mock_tab_);
  EXPECT_EQ(SentText(sent), "Selected text");
}

// Activating a tab doesn't touch its side panel, which follows its own state.
TEST_F(SelectionContextSenderTest, TabActivationIgnoresSidePanel) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  did_activate_callbacks_.Notify(&mock_tab_);
}

// A discarded tab keeps its instance, so returning to it clears the chip.
TEST_F(SelectionContextSenderTest, WillDiscardContentsClearsChipOnReturn) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  EXPECT_CALL(instance, SendAdditionalContext).Times(1);
  sender_->SetText(u"Selected text");
  side_panel_coordinator_->SetState(
      GlicSidePanelCoordinator::State::kBackgrounded);

  discard_callbacks_.Notify(&mock_tab_, web_contents(), nullptr);
  EXPECT_TRUE(::selection::PageSelection::From(&mock_tab_)->GetText().empty());

  mojom::AdditionalContextPtr sent;
  EXPECT_CALL(instance, SendAdditionalContext)
      .WillOnce([&sent](mojom::AdditionalContextPtr context) {
        sent = std::move(context);
      });
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
  ASSERT_TRUE(sent);
  EXPECT_TRUE(sent->parts.empty());
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, IsTheTabsPageSelection) {
  EXPECT_EQ(::selection::PageSelection::From(&mock_tab_), sender_.get());

  sender_->SetText(u"Selected text");
  EXPECT_EQ(::selection::PageSelection::From(&mock_tab_)->GetText(),
            u"Selected text");
}

TEST_F(SelectionContextSenderTest, UnknownEligibilityClearsContext) {
  NiceMock<MockGlicInstance> instance;
  ON_CALL(instance, IsShowing()).WillByDefault(Return(true));
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);
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
  SetInstance(&instance);
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

  EXPECT_CALL(instance, SendAdditionalContext).Times(0);
  EXPECT_CALL(*mock_service_, Invoke).Times(0);
  sender_->SetText(u"Selected text");
  EXPECT_FALSE(sender_->has_sent_selection_context());
}

TEST_F(SelectionContextSenderTest, NoInstanceForTabDoesNotSend) {
  ON_CALL(*mock_service_, GetInstanceForTab(&mock_tab_))
      .WillByDefault(Return(nullptr));
  side_panel_coordinator_->SetState(GlicSidePanelCoordinator::State::kShown);

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
  ON_CALL(other_tab, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(other_host));
  FakePageContextEligibilityHelper other_helper(other_tab);

  SelectionContextSender sender(other_tab);
  EXPECT_FALSE(sender.IsPanelShowing());
  sender.SetText(u"Selected text");
  EXPECT_FALSE(sender.has_sent_selection_context());
}

}  // namespace glic
