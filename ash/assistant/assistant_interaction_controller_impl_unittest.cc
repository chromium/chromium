// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_interaction_controller_impl.h"

#include <algorithm>
#include <map>

#include "ash/assistant/assistant_suggestions_controller_impl.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_response_observer.h"
#include "ash/assistant/model/ui/assistant_card_element.h"
#include "ash/assistant/model/ui/assistant_error_element.h"
#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/assistant_error_element_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/test/fake_android_intent_helper.h"
#include "ash/test/test_ash_web_view.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/test_support/mock_assistant_interaction_subscriber.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

using assistant::AndroidAppInfo;
using assistant::Assistant;
using assistant::AssistantInteractionMetadata;
using assistant::AssistantInteractionSubscriber;
using assistant::AssistantInteractionType;
using assistant::AssistantQuerySource;
using assistant::AssistantSuggestion;
using assistant::AssistantSuggestionType;
using assistant::MockAssistantInteractionSubscriber;
using assistant::ScopedAssistantInteractionSubscriber;

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

// Mocks -----------------------------------------------------------------------

class AssistantInteractionSubscriberMock
    : public AssistantInteractionSubscriber {
 public:
  explicit AssistantInteractionSubscriberMock(Assistant* service) {
    scoped_subscriber_.Observe(service);
  }

  ~AssistantInteractionSubscriberMock() override = default;

  MOCK_METHOD(void,
              OnInteractionStarted,
              (const AssistantInteractionMetadata&),
              (override));

 private:
  ScopedAssistantInteractionSubscriber scoped_subscriber_{this};
};

// AssistantInteractionControllerImplTest --------------------------------------

class AssistantInteractionControllerImplTest : public AssistantAshTestBase {
 public:
  AssistantInteractionControllerImplTest() = default;

  AssistantInteractionControllerImpl* interaction_controller() {
    return static_cast<AssistantInteractionControllerImpl*>(
        AssistantInteractionController::Get());
  }

  AssistantSuggestionsControllerImpl* suggestion_controller() {
    return static_cast<AssistantSuggestionsControllerImpl*>(
        AssistantSuggestionsController::Get());
  }

  const AssistantInteractionModel* interaction_model() {
    return interaction_controller()->GetModel();
  }

  void StartInteraction() {
    interaction_controller()->OnInteractionStarted(
        AssistantInteractionMetadata());
  }

  AndroidAppInfo CreateAndroidAppInfo(const std::string& app_name = "unknown") {
    AndroidAppInfo result;
    result.localized_app_name = app_name;
    return result;
  }
};

AssistantCardElement* GetAssistantCardElement(
    const std::vector<std::unique_ptr<AssistantUiElement>>& ui_elements) {
  if (ui_elements.size() != 1lu ||
      ui_elements.front()->type() != AssistantUiElementType::kCard) {
    return nullptr;
  }

  return static_cast<AssistantCardElement*>(ui_elements.front().get());
}

}  // namespace

TEST_F(AssistantInteractionControllerImplTest,
       ShouldBecomeActiveWhenInteractionStarts) {
  EXPECT_EQ(interaction_model()->interaction_state(),
            InteractionState::kInactive);

  interaction_controller()->OnInteractionStarted(
      AssistantInteractionMetadata());

  EXPECT_EQ(interaction_model()->interaction_state(),
            InteractionState::kActive);
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldBeNoOpWhenOpenAppIsCalledWhileInactive) {
  EXPECT_EQ(interaction_model()->interaction_state(),
            InteractionState::kInactive);

  FakeAndroidIntentHelper fake_helper;
  fake_helper.AddApp("app-name", "app-intent");
  interaction_controller()->OnOpenAppResponse(CreateAndroidAppInfo("app-name"));

  EXPECT_FALSE(fake_helper.last_launched_android_intent().has_value());
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldBeNoOpWhenOpenAppIsCalledForUnknownAndroidApp) {
  StartInteraction();
  FakeAndroidIntentHelper fake_helper;
  interaction_controller()->OnOpenAppResponse(
      CreateAndroidAppInfo("unknown-app-name"));

  EXPECT_FALSE(fake_helper.last_launched_android_intent().has_value());
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldLaunchAppAndReturnSuccessWhenOpenAppIsCalled) {
  const std::string app_name = "AppName";
  const std::string intent = "intent://AppName";

  StartInteraction();
  FakeAndroidIntentHelper fake_helper;
  fake_helper.AddApp(app_name, intent);

  interaction_controller()->OnOpenAppResponse(CreateAndroidAppInfo(app_name));

  EXPECT_EQ(intent, fake_helper.last_launched_android_intent());
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldAddSchemeToIntentWhenLaunchingAndroidApp) {
  const std::string app_name = "AppName";
  const std::string intent = "#Intent-without-a-scheme";
  const std::string intent_with_scheme = "intent://" + intent;

  StartInteraction();
  FakeAndroidIntentHelper fake_helper;
  fake_helper.AddApp(app_name, intent);

  interaction_controller()->OnOpenAppResponse(CreateAndroidAppInfo(app_name));

  EXPECT_EQ(intent_with_scheme, fake_helper.last_launched_android_intent());
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldCorrectlyMapSuggestionTypeToQuerySource) {
  // Mock Assistant interaction subscriber.
  StrictMock<AssistantInteractionSubscriberMock> mock(assistant_service());

  // Configure the expected mappings between suggestion type and query source.
  const std::map<AssistantSuggestionType, AssistantQuerySource>
      types_to_sources = {{AssistantSuggestionType::kConversationStarter,
                           AssistantQuerySource::kConversationStarter},
                          {AssistantSuggestionType::kBetterOnboarding,
                           AssistantQuerySource::kBetterOnboarding},
                          {AssistantSuggestionType::kUnspecified,
                           AssistantQuerySource::kSuggestionChip}};

  // Iterate over all expected mappings.
  for (const auto& type_to_source : types_to_sources) {
    base::RunLoop run_loop;

    // Confirm subscribers are delivered the expected query source...
    EXPECT_CALL(mock, OnInteractionStarted)
        .WillOnce(Invoke([&](const AssistantInteractionMetadata& metadata) {
          EXPECT_EQ(type_to_source.second, metadata.source);
          run_loop.QuitClosure().Run();
        }));

    AssistantSuggestion suggestion{/*id=*/base::UnguessableToken::Create(),
                                   /*type=*/type_to_source.first,
                                   /*text=*/""};
    const_cast<AssistantSuggestionsModel*>(suggestion_controller()->GetModel())
        ->SetConversationStarters({suggestion});

    // ...when an Assistant suggestion of a given type is pressed.
    interaction_controller()->OnSuggestionPressed(suggestion.id);

    run_loop.Run();
  }
}

TEST_F(AssistantInteractionControllerImplTest, ShouldDisplayGenericErrorOnce) {
  StartInteraction();

  // Call OnTtsStarted twice to mimic the behavior of libassistant when network
  // is disconnected.
  interaction_controller()->OnTtsStarted(/*due_to_error=*/true);
  interaction_controller()->OnTtsStarted(/*due_to_error=*/true);

  base::RunLoop().RunUntilIdle();

  auto& ui_elements =
      interaction_controller()->GetModel()->response()->GetUiElements();

  EXPECT_EQ(ui_elements.size(), 1ul);
  EXPECT_EQ(ui_elements.front()->type(), AssistantUiElementType::kError);

  base::RunLoop().RunUntilIdle();

  interaction_controller()->OnInteractionFinished(
      assistant::AssistantInteractionResolution::kError);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ui_elements.size(), 1ul);
  EXPECT_EQ(ui_elements.front()->type(), AssistantUiElementType::kError);
}

TEST_F(AssistantInteractionControllerImplTest,
       ShouldUpdateTimeOfLastInteraction) {
  MockAssistantInteractionSubscriber mock_subscriber;
  ScopedAssistantInteractionSubscriber scoped_subscriber{&mock_subscriber};
  scoped_subscriber.Observe(assistant_service());

  base::RunLoop run_loop;
  base::Time actual_time_of_last_interaction;
  EXPECT_CALL(mock_subscriber, OnInteractionStarted)
      .WillOnce(Invoke([&](const AssistantInteractionMetadata& metadata) {
        actual_time_of_last_interaction = base::Time::Now();
        run_loop.QuitClosure().Run();
      }));

  ShowAssistantUi();
  MockTextInteraction().WithTextResponse("<Any-Text-Response>");
  run_loop.Run();

  auto actual = interaction_controller()->GetTimeDeltaSinceLastInteraction();
  auto expected = base::Time::Now() - actual_time_of_last_interaction;

  EXPECT_NEAR(actual.InSeconds(), expected.InSeconds(), 1);
}

TEST_F(AssistantInteractionControllerImplTest, CompactBubbleLauncher) {
  static constexpr int kStandardLayoutAshWebViewWidth = 592;
  static constexpr int kNarrowLayoutAshWebViewWidth = 496;

  UpdateDisplay("1200x800");
  ShowAssistantUi();
  StartInteraction();

  interaction_controller()->OnHtmlResponse("<html></html>", "fallback");

  base::RunLoop().RunUntilIdle();

  AssistantCardElement* card_element = GetAssistantCardElement(
      interaction_controller()->GetModel()->response()->GetUiElements());
  ASSERT_TRUE(card_element);
  EXPECT_EQ(card_element->viewport_width(), 638);
  EXPECT_EQ(
      page_view()->GetViewByID(AssistantViewID::kAshWebView)->size().width(),
      kStandardLayoutAshWebViewWidth);

  ASSERT_TRUE(page_view()->GetViewByID(AssistantViewID::kAshWebView) !=
              nullptr);
  TestAshWebView* ash_web_view = static_cast<TestAshWebView*>(
      page_view()->GetViewByID(AssistantViewID::kAshWebView));
  // max_size and min_size in AshWebView::InitParams are different from the view
  // size. min_size affects to the size of rendered content, i.e. renderer will
  // try to render the content to the size. But View::Size() doesn't.
  ASSERT_TRUE(ash_web_view->init_params_for_testing().max_size);
  ASSERT_TRUE(ash_web_view->init_params_for_testing().min_size);
  EXPECT_EQ(ash_web_view->init_params_for_testing().max_size.value().width(),
            kStandardLayoutAshWebViewWidth);
  EXPECT_EQ(ash_web_view->init_params_for_testing().min_size.value().width(),
            kStandardLayoutAshWebViewWidth);

  CloseAssistantUi();

  // Change work area width < 1200 and confirm that the viewport width gets
  // updated to narrow layout one.
  UpdateDisplay("1199x800");
  ShowAssistantUi();
  StartInteraction();

  interaction_controller()->OnHtmlResponse("<html></html>", "fallback");

  base::RunLoop().RunUntilIdle();

  card_element = GetAssistantCardElement(
      interaction_controller()->GetModel()->response()->GetUiElements());
  ASSERT_TRUE(card_element);
  ASSERT_TRUE(page_view()->GetViewByID(AssistantViewID::kAshWebView) !=
              nullptr);
  EXPECT_EQ(card_element->viewport_width(), 542);
  EXPECT_EQ(
      page_view()->GetViewByID(AssistantViewID::kAshWebView)->size().width(),
      kNarrowLayoutAshWebViewWidth);

  ASSERT_TRUE(page_view()->GetViewByID(AssistantViewID::kAshWebView) !=
              nullptr);
  ash_web_view = static_cast<TestAshWebView*>(
      page_view()->GetViewByID(AssistantViewID::kAshWebView));
  ASSERT_TRUE(ash_web_view->init_params_for_testing().max_size);
  ASSERT_TRUE(ash_web_view->init_params_for_testing().min_size);
  EXPECT_EQ(ash_web_view->init_params_for_testing().max_size.value().width(),
            kNarrowLayoutAshWebViewWidth);
  EXPECT_EQ(ash_web_view->init_params_for_testing().min_size.value().width(),
            kNarrowLayoutAshWebViewWidth);
}

TEST_F(AssistantInteractionControllerImplTest, FixedZoomLevel) {
  ShowAssistantUi();
  StartInteraction();

  interaction_controller()->OnHtmlResponse("<html></html>", "fallback");

  base::RunLoop().RunUntilIdle();

  TestAshWebView* ash_web_view = static_cast<TestAshWebView*>(
      page_view()->GetViewByID(AssistantViewID::kAshWebView));
  EXPECT_TRUE(ash_web_view->init_params_for_testing().fix_zoom_level_to_one);
}
}  // namespace ash
