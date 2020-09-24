// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/main_stage/assistant_onboarding_suggestion_view.h"
#include "ash/assistant/ui/test_support/mock_assistant_view_delegate.h"
#include "ash/assistant/util/test_support/macros.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

using chromeos::assistant::Assistant;
using chromeos::assistant::AssistantInteractionMetadata;
using chromeos::assistant::AssistantInteractionType;
using chromeos::assistant::AssistantQuerySource;
using chromeos::assistant::AssistantSuggestion;
using chromeos::assistant::AssistantSuggestionType;

// Helpers ---------------------------------------------------------------------

AssistantSuggestion CreateSuggestionWithIconUrl(const std::string& icon_url) {
  AssistantSuggestion suggestion;
  suggestion.icon_url = GURL(icon_url);
  return suggestion;
}

template <typename T>
void FindDescendentByClassName(views::View* parent, T** result) {
  DCHECK_EQ(nullptr, *result);
  std::queue<views::View*> children({parent});
  while (!children.empty()) {
    auto* candidate = children.front();
    children.pop();

    if (candidate->GetClassName() == T::kClassName) {
      *result = static_cast<T*>(candidate);
      return;
    }

    for (auto* child : candidate->children())
      children.push(child);
  }
}

// Mocks -----------------------------------------------------------------------

class MockAssistantInteractionSubscriber
    : public testing::NiceMock<
          chromeos::assistant::AssistantInteractionSubscriber> {
 public:
  explicit MockAssistantInteractionSubscriber(Assistant* service) {
    scoped_subscriber_.Add(service);
  }

  ~MockAssistantInteractionSubscriber() override = default;

  MOCK_METHOD(void,
              OnInteractionStarted,
              (const AssistantInteractionMetadata&),
              (override));

 private:
  chromeos::assistant::ScopedAssistantInteractionSubscriber scoped_subscriber_{
      this};
};

// ScopedShowUi ----------------------------------------------------------------

class ScopedShowUi {
 public:
  ScopedShowUi()
      : original_visibility_(
            AssistantUiController::Get()->GetModel()->visibility()) {
    AssistantUiController::Get()->ShowUi(
        chromeos::assistant::AssistantEntryPoint::kUnspecified);
  }

  ScopedShowUi(const ScopedShowUi&) = delete;
  ScopedShowUi& operator=(const ScopedShowUi&) = delete;

  ~ScopedShowUi() {
    switch (original_visibility_) {
      case AssistantVisibility::kClosed:
        AssistantUiController::Get()->CloseUi(
            chromeos::assistant::AssistantExitPoint::kUnspecified);
        return;
      case AssistantVisibility::kVisible:
        // No action necessary.
        return;
    }
  }

 private:
  const AssistantVisibility original_visibility_;
};

// AssistantOnboardingViewTest -------------------------------------------------

class AssistantOnboardingViewTest : public AssistantAshTestBase {
 public:
  AssistantOnboardingViewTest()
      : AssistantAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeature(
        chromeos::assistant::features::kAssistantBetterOnboarding);
  }

  ~AssistantOnboardingViewTest() override = default;

  void AdvanceClock(base::TimeDelta time_delta) {
    task_environment()->AdvanceClock(time_delta);
  }

  void SetOnboardingSuggestions(
      std::vector<AssistantSuggestion> onboarding_suggestions) {
    const_cast<AssistantSuggestionsModel*>(
        AssistantSuggestionsController::Get()->GetModel())
        ->SetOnboardingSuggestions(std::move(onboarding_suggestions));
  }

  views::Label* greeting_label() {
    return static_cast<views::Label*>(onboarding_view()->children().at(0));
  }

  views::Label* intro_label() {
    return static_cast<views::Label*>(onboarding_view()->children().at(1));
  }

 private:
  base::test::ScopedRestoreICUDefaultLocale locale_{"en_US"};
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Tests -----------------------------------------------------------------------

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedGreeting) {
  struct ExpectedGreeting {
    std::string for_morning;
    std::string for_afternoon;
    std::string for_evening;
    std::string for_night;
  };

  struct TestCase {
    std::string display_email;
    std::string given_name;
    ExpectedGreeting expected_greeting;
  };

  const std::vector<TestCase> test_cases = {
      TestCase{/*display_email=*/"empty@test",
               /*given_name=*/std::string(),
               ExpectedGreeting{
                   /*for_morning=*/"Good morning,",
                   /*for_afternoon=*/"Good afternoon,",
                   /*for_evening=*/"Good evening,",
                   /*for_night=*/"Good night,",
               }},
      TestCase{/*display_email=*/"david@test",
               /*given_name=*/"David",
               ExpectedGreeting{
                   /*for_morning=*/"Good morning David,",
                   /*for_afternoon=*/"Good afternoon David,",
                   /*for_evening=*/"Good evening David,",
                   /*for_night=*/"Good night David,",
               }}};

  for (const auto& test_case : test_cases) {
    CreateAndSwitchActiveUser(test_case.display_email, test_case.given_name);

    // Advance clock to midnight tomorrow.
    AdvanceClock(base::Time::Now().LocalMidnight() +
                 base::TimeDelta::FromHours(24) - base::Time::Now());

    {
      // Verify 4:59 AM.
      AdvanceClock(base::TimeDelta::FromHours(4) +
                   base::TimeDelta::FromMinutes(59));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                base::UTF8ToUTF16(test_case.expected_greeting.for_night));
    }

    {
      // Verify 5:00 AM.
      AdvanceClock(base::TimeDelta::FromMinutes(1));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                base::UTF8ToUTF16(test_case.expected_greeting.for_morning));
    }

    {
      // Verify 11:59 AM.
      AdvanceClock(base::TimeDelta::FromHours(6) +
                   base::TimeDelta::FromMinutes(59));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                base::UTF8ToUTF16(test_case.expected_greeting.for_morning));
    }

    {
      // Verify 12:00 PM.
      AdvanceClock(base::TimeDelta::FromMinutes(1));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                base::UTF8ToUTF16(test_case.expected_greeting.for_afternoon));
    }

    {
      // Verify 4:59 PM.
      AdvanceClock(base::TimeDelta::FromHours(4) +
                   base::TimeDelta::FromMinutes(59));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                base::UTF8ToUTF16(test_case.expected_greeting.for_afternoon));
    }

    {
      // Verify 5:00 PM.
      AdvanceClock(base::TimeDelta::FromMinutes(1));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                base::UTF8ToUTF16(test_case.expected_greeting.for_evening));
    }

    {
      // Verify 10:59 PM.
      AdvanceClock(base::TimeDelta::FromHours(5) +
                   base::TimeDelta::FromMinutes(59));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                base::UTF8ToUTF16(test_case.expected_greeting.for_evening));
    }

    {
      // Verify 11:00 PM.
      AdvanceClock(base::TimeDelta::FromMinutes(1));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                base::UTF8ToUTF16(test_case.expected_greeting.for_night));
    }
  }
}

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedIntro) {
  ShowAssistantUi();
  EXPECT_EQ(intro_label()->GetText(),
            base::UTF8ToUTF16(
                "I'm your Google Assistant, here to help you throughout your "
                "day!\nHere are some things you can try to get started."));
}

TEST_F(AssistantOnboardingViewTest, ShouldHaveExpectedSuggestions) {
  struct VectorIconWithColor {
    VectorIconWithColor(const gfx::VectorIcon& icon, SkColor color)
        : icon(icon), color(color) {}

    const gfx::VectorIcon& icon;
    SkColor color;
  };

  struct ExpectedSuggestion {
    std::string message;
    std::unique_ptr<VectorIconWithColor> icon_with_color;
  };

  // Iterate over each onboarding mode.
  for (int mode = 0;
       mode <= static_cast<int>(AssistantOnboardingMode::kMaxValue); ++mode) {
    auto onboarding_mode = static_cast<AssistantOnboardingMode>(mode);
    SetOnboardingMode(onboarding_mode);

    // Determine expected suggestions based on onboarding mode.
    std::vector<ExpectedSuggestion> expected_suggestions;
    switch (onboarding_mode) {
      case AssistantOnboardingMode::kEducation:
        expected_suggestions.push_back(
            {/*message=*/"Square root of 71",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kCalculateIcon, gfx::kGoogleBlue800)});
        expected_suggestions.push_back(
            {/*message=*/"How far is Venus",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kStraightenIcon, gfx::kGoogleRed800)});
        expected_suggestions.push_back(
            {/*message=*/"Set timer",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kTimerIcon, SkColorSetRGB(0xBF, 0x50, 0x00))});
        expected_suggestions.push_back(
            {/*message=*/"Tell me a joke",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kSentimentVerySatisfiedIcon, gfx::kGoogleGreen800)});
        expected_suggestions.push_back(
            {/*message=*/"\"Hello\" in Chinese",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kTranslateIcon, SkColorSetRGB(0x8A, 0x0E, 0x9E))});
        expected_suggestions.push_back(
            {/*message=*/"Take a screenshot",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kScreenshotIcon, gfx::kGoogleBlue800)});
        break;
      case AssistantOnboardingMode::kDefault:
        expected_suggestions.push_back(
            {/*message=*/"5K in miles",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kConversionPathIcon, gfx::kGoogleBlue800)});
        expected_suggestions.push_back(
            {/*message=*/"Population in Nigeria",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kPersonPinCircleIcon, gfx::kGoogleRed800)});
        expected_suggestions.push_back(
            {/*message=*/"Set timer",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kTimerIcon, SkColorSetRGB(0xBF, 0x50, 0x00))});
        expected_suggestions.push_back(
            {/*message=*/"Tell me a joke",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kSentimentVerySatisfiedIcon, gfx::kGoogleGreen800)});
        expected_suggestions.push_back(
            {/*message=*/"\"Hello\" in Chinese",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kTranslateIcon, SkColorSetRGB(0x8A, 0x0E, 0x9E))});
        expected_suggestions.push_back(
            {/*message=*/"Take a screenshot",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kScreenshotIcon, gfx::kGoogleBlue800)});
        break;
    }

    ShowAssistantUi();

    // Verify the expected number of suggestion views.
    auto suggestion_views = GetOnboardingSuggestionViews();
    ASSERT_EQ(suggestion_views.size(), expected_suggestions.size());

    // Verify that each suggestion view has the expected message and icon.
    for (size_t i = 0; i < expected_suggestions.size(); ++i) {
      const auto* suggestion_view = suggestion_views.at(i);
      const auto& expected_suggestion = expected_suggestions.at(i);

      EXPECT_EQ(suggestion_view->GetText(),
                base::UTF8ToUTF16(expected_suggestion.message));

      ASSERT_PIXELS_EQ(
          suggestion_view->GetIcon(),
          gfx::CreateVectorIcon(expected_suggestion.icon_with_color->icon,
                                /*size=*/24,
                                expected_suggestion.icon_with_color->color));
    }
  }
}

TEST_F(AssistantOnboardingViewTest, ShouldHandleSuggestionPresses) {
  ShowAssistantUi();

  // Verify onboarding suggestions exist.
  auto suggestion_views = GetOnboardingSuggestionViews();
  ASSERT_FALSE(suggestion_views.empty());

  // Expect a text interaction originating from the onboarding feature...
  MockAssistantInteractionSubscriber subscriber(assistant_service());
  EXPECT_CALL(subscriber, OnInteractionStarted)
      .WillOnce(
          testing::Invoke([](const AssistantInteractionMetadata& metadata) {
            EXPECT_EQ(AssistantInteractionType::kText, metadata.type);
            EXPECT_EQ(AssistantQuerySource::kBetterOnboarding, metadata.source);
          }));

  // ...when an onboarding suggestion is pressed.
  TapOnAndWait(suggestion_views.at(0));
}

TEST_F(AssistantOnboardingViewTest, ShouldHandleSuggestionUpdates) {
  // Show Assistant UI and verify suggestions exist.
  ShowAssistantUi();
  ASSERT_FALSE(GetOnboardingSuggestionViews().empty());

  // Manually create a suggestion.
  AssistantSuggestion suggestion;
  suggestion.id = base::UnguessableToken();
  suggestion.type = AssistantSuggestionType::kBetterOnboarding;
  suggestion.text = "Forced suggestion";

  // Force a model update.
  std::vector<AssistantSuggestion> suggestions;
  suggestions.push_back(std::move(suggestion));
  SetOnboardingSuggestions(std::move(suggestions));

  // Verify view state is updated to reflect model state.
  auto suggestion_views = GetOnboardingSuggestionViews();
  ASSERT_EQ(suggestion_views.size(), 1u);
  EXPECT_EQ(suggestion_views.at(0)->GetText(),
            base::UTF8ToUTF16("Forced suggestion"));
}

TEST_F(AssistantOnboardingViewTest, ShouldHandleLocalIcons) {
  SetOnboardingSuggestions({CreateSuggestionWithIconUrl(
      "googleassistant://resource?type=icon&name=assistant")});

  ShowAssistantUi();
  auto suggestion_views = GetOnboardingSuggestionViews();
  ASSERT_EQ(suggestion_views.size(), 1u);

  const auto& actual = suggestion_views.at(0)->GetIcon();
  gfx::ImageSkia expected = gfx::CreateVectorIcon(
      gfx::IconDescription(chromeos::kAssistantIcon, /*size=*/24));

  ASSERT_PIXELS_EQ(actual, expected);
}

TEST_F(AssistantOnboardingViewTest, ShouldHandleRemoteIcons) {
  const gfx::ImageSkia expected =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);

  MockAssistantViewDelegate delegate;
  EXPECT_CALL(delegate, GetPrimaryUserGivenName)
      .WillOnce(testing::Return("Primary User Given Name"));

  AssistantOnboardingView onboarding_view(&delegate);
  EXPECT_CALL(delegate, DownloadImage)
      .WillOnce(testing::Invoke(
          [&](const GURL& url, ImageDownloader::DownloadCallback callback) {
            std::move(callback).Run(expected);
          }));

  SetOnboardingSuggestions({CreateSuggestionWithIconUrl(
      "https://www.gstatic.com/images/branding/product/2x/googleg_48dp.png")});

  AssistantOnboardingSuggestionView* suggestion_view = nullptr;
  FindDescendentByClassName(&onboarding_view, &suggestion_view);
  ASSERT_NE(nullptr, suggestion_view);

  const auto& actual = suggestion_view->GetIcon();
  EXPECT_TRUE(actual.BackedBySameObjectAs(expected));
}

}  // namespace ash
