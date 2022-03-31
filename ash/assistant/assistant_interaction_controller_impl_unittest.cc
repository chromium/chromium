// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "ash/assistant/model/ui/assistant_error_element.h"
#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/main_stage/assistant_error_element_view.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/test/fake_android_intent_helper.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/test_support/mock_assistant_interaction_subscriber.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

using chromeos::assistant::AndroidAppInfo;
using chromeos::assistant::Assistant;
using chromeos::assistant::AssistantInteractionMetadata;
using chromeos::assistant::AssistantInteractionSubscriber;
using chromeos::assistant::AssistantInteractionType;
using chromeos::assistant::AssistantQuerySource;
using chromeos::assistant::AssistantSuggestion;
using chromeos::assistant::AssistantSuggestionType;
using chromeos::assistant::MockAssistantInteractionSubscriber;
using chromeos::assistant::ScopedAssistantInteractionSubscriber;

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

// Mocks -----------------------------------------------------------------------

class AssistantInteractionSubscriberMock
    : public chromeos::assistant::AssistantInteractionSubscriber {
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
  chromeos::assistant::ScopedAssistantInteractionSubscriber scoped_subscriber_{
      this};
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
      chromeos::assistant::AssistantInteractionResolution::kError);

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
}  // namespace ash
