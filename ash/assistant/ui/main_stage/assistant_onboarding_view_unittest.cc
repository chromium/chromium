// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/assistant_onboarding_suggestion_view.h"
#include "ash/assistant/ui/test_support/mock_assistant_view_delegate.h"
#include "ash/assistant/util/test_support/macros.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using assistant::AssistantInteractionMetadata;
using assistant::AssistantInteractionType;
using assistant::AssistantQuerySource;
using assistant::AssistantSuggestion;
using assistant::AssistantSuggestionType;

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

    if (views::IsViewClass<T>(candidate)) {
      *result = static_cast<T*>(candidate);
      return;
    }

    for (views::View* child : candidate->children()) {
      children.push(child);
    }
  }
}

// Mocks -----------------------------------------------------------------------

class MockAssistantInteractionSubscriber
    : public testing::NiceMock<assistant::AssistantInteractionSubscriber> {
 public:
  explicit MockAssistantInteractionSubscriber(assistant::Assistant* service) {
    scoped_subscriber_.Observe(service);
  }

  ~MockAssistantInteractionSubscriber() override = default;

  MOCK_METHOD(void,
              OnInteractionStarted,
              (const AssistantInteractionMetadata&),
              (override));

 private:
  assistant::ScopedAssistantInteractionSubscriber scoped_subscriber_{this};
};

// ScopedShowUi ----------------------------------------------------------------

class ScopedShowUi {
 public:
  ScopedShowUi()
      : original_visibility_(
            AssistantUiController::Get()->GetModel()->visibility()) {
    AssistantUiController::Get()->ShowUi(
        assistant::AssistantEntryPoint::kUnspecified);
  }

  ScopedShowUi(const ScopedShowUi&) = delete;
  ScopedShowUi& operator=(const ScopedShowUi&) = delete;

  ~ScopedShowUi() {
    switch (original_visibility_) {
      case AssistantVisibility::kClosed:
        AssistantUiController::Get()->CloseUi(
            assistant::AssistantExitPoint::kUnspecified);
        return;
      case AssistantVisibility::kVisible:
        // No action necessary.
        return;
      case AssistantVisibility::kClosing:
        // No action necessary.
        return;
    }
  }

 private:
  const AssistantVisibility original_visibility_;
};

// DISABLED_AssistantOnboardingViewTest
// -------------------------------------------------

class DISABLED_AssistantOnboardingViewTest : public AssistantAshTestBase {
 public:
  DISABLED_AssistantOnboardingViewTest()
      : AssistantAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~DISABLED_AssistantOnboardingViewTest() override = default;

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
};

// Tests -----------------------------------------------------------------------

TEST_F(DISABLED_AssistantOnboardingViewTest, ShouldHaveExpectedGreeting) {
  struct ExpectedGreeting {
    std::u16string for_morning;
    std::u16string for_afternoon;
    std::u16string for_evening;
    std::u16string for_night;
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
                   /*for_morning=*/u"Good morning,",
                   /*for_afternoon=*/u"Good afternoon,",
                   /*for_evening=*/u"Good evening,",
                   /*for_night=*/u"Good night,",
               }},
      TestCase{/*display_email=*/"david@test",
               /*given_name=*/"David",
               ExpectedGreeting{
                   /*for_morning=*/u"Good morning David,",
                   /*for_afternoon=*/u"Good afternoon David,",
                   /*for_evening=*/u"Good evening David,",
                   /*for_night=*/u"Good night David,",
               }}};

  for (const auto& test_case : test_cases) {
    CreateAndSwitchActiveUser(test_case.display_email, test_case.given_name);

    // Advance clock to midnight tomorrow.
    AdvanceClock(base::Time::Now().LocalMidnight() + base::Hours(24) -
                 base::Time::Now());

    {
      // Verify 4:59 AM.
      AdvanceClock(base::Hours(4) + base::Minutes(59));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                test_case.expected_greeting.for_night);
    }

    {
      // Verify 5:00 AM.
      AdvanceClock(base::Minutes(1));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                test_case.expected_greeting.for_morning);
    }

    {
      // Verify 11:59 AM.
      AdvanceClock(base::Hours(6) + base::Minutes(59));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                test_case.expected_greeting.for_morning);
    }

    {
      // Verify 12:00 PM.
      AdvanceClock(base::Minutes(1));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                test_case.expected_greeting.for_afternoon);
    }

    {
      // Verify 4:59 PM.
      AdvanceClock(base::Hours(4) + base::Minutes(59));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                test_case.expected_greeting.for_afternoon);
    }

    {
      // Verify 5:00 PM.
      AdvanceClock(base::Minutes(1));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                test_case.expected_greeting.for_evening);
    }

    {
      // Verify 10:59 PM.
      AdvanceClock(base::Hours(5) + base::Minutes(59));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                test_case.expected_greeting.for_evening);
    }

    {
      // Verify 11:00 PM.
      AdvanceClock(base::Minutes(1));
      ScopedShowUi scoped_show_ui;
      EXPECT_EQ(greeting_label()->GetText(),
                test_case.expected_greeting.for_night);
    }
  }
}

TEST_F(DISABLED_AssistantOnboardingViewTest, ShouldHaveExpectedIntro) {
  ShowAssistantUi();
  EXPECT_EQ(intro_label()->GetText(),
            u"I'm your Google Assistant, here to help you throughout your day!"
            u"\nHere are some things you can try to get started.");
}

TEST_F(DISABLED_AssistantOnboardingViewTest, ShouldHaveExpectedSuggestions) {
  struct VectorIconWithColor {
    VectorIconWithColor(const gfx::VectorIcon& icon, SkColor color)
        : icon(icon), color(color) {}

    const raw_ref<const gfx::VectorIcon> icon;
    SkColor color;
  };

  struct ExpectedSuggestion {
    std::u16string message;
    std::unique_ptr<VectorIconWithColor> icon_with_color;
  };

  auto get_color = [](int index) {
    constexpr SkColor kForegroundColors[6][3] = {
        // Colors of dark/light mode is disabled, dark mode, light mode.
        {gfx::kGoogleBlue800, gfx::kGoogleBlue200, gfx::kGoogleBlue800},
        {gfx::kGoogleRed800, gfx::kGoogleRed200, gfx::kGoogleRed800},
        {SkColorSetRGB(0xBF, 0x50, 0x00), gfx::kGoogleYellow200,
         SkColorSetRGB(0xBF, 0x50, 0x00)},
        {gfx::kGoogleGreen800, gfx::kGoogleGreen200, gfx::kGoogleGreen800},
        {SkColorSetRGB(0x8A, 0x0E, 0x9E), SkColorSetRGB(0xf8, 0x82, 0xff),
         SkColorSetRGB(0xaa, 0x00, 0xb8)},
        {gfx::kGoogleBlue800, gfx::kGoogleBlue200, gfx::kGoogleBlue800}};
    const int color_index =
        DarkLightModeControllerImpl::Get()->IsDarkModeEnabled() ? 1 : 2;
    return kForegroundColors[index][color_index];
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
            {/*message=*/u"Square root of 71",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kCalculateIcon, get_color(0))});
        expected_suggestions.push_back(
            {/*message=*/u"How far is Venus",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kStraightenIcon, get_color(1))});
        expected_suggestions.push_back(
            {/*message=*/u"Set timer",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kTimerIcon, get_color(2))});
        expected_suggestions.push_back(
            {/*message=*/u"Tell me a joke",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kSentimentVerySatisfiedIcon, get_color(3))});
        expected_suggestions.push_back(
            {/*message=*/u"\"Hello\" in Chinese",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kTranslateIcon, get_color(4))});
        expected_suggestions.push_back(
            {/*message=*/u"Take a screenshot",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kScreenshotIcon, get_color(5))});
        break;
      case AssistantOnboardingMode::kDefault:
        expected_suggestions.push_back(
            {/*message=*/u"5K in miles",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kConversionPathIcon, get_color(0))});
        expected_suggestions.push_back(
            {/*message=*/u"Population in Nigeria",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kPersonPinCircleIcon, get_color(1))});
        expected_suggestions.push_back(
            {/*message=*/u"Set timer",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kTimerIcon, get_color(2))});
        expected_suggestions.push_back(
            {/*message=*/u"Tell me a joke",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kSentimentVerySatisfiedIcon, get_color(3))});
        expected_suggestions.push_back(
            {/*message=*/u"\"Hello\" in Chinese",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kTranslateIcon, get_color(4))});
        expected_suggestions.push_back(
            {/*message=*/u"Take a screenshot",
             /*icon_with_color=*/std::make_unique<VectorIconWithColor>(
                 chromeos::kScreenshotIcon, get_color(5))});
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

      EXPECT_EQ(suggestion_view->GetText(), expected_suggestion.message);

      ASSERT_PIXELS_EQ(
          suggestion_view->GetIcon(),
          gfx::CreateVectorIcon(*expected_suggestion.icon_with_color->icon,
                                /*size=*/24,
                                expected_suggestion.icon_with_color->color));
    }
  }
}

TEST_F(DISABLED_AssistantOnboardingViewTest, ShouldHandleSuggestionPresses) {
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

TEST_F(DISABLED_AssistantOnboardingViewTest, ShouldHandleSuggestionUpdates) {
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
  EXPECT_EQ(suggestion_views.at(0)->GetText(), u"Forced suggestion");
}

TEST_F(DISABLED_AssistantOnboardingViewTest, ShouldHandleLocalIcons) {
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

TEST_F(DISABLED_AssistantOnboardingViewTest, ShouldHandleRemoteIcons) {
  const gfx::ImageSkia expected =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);

  MockAssistantViewDelegate delegate;
  EXPECT_CALL(delegate, GetPrimaryUserGivenName)
      .WillOnce(testing::Return("Primary User Given Name"));

  auto widget = CreateFramelessTestWidget();
  auto* onboarding_view = widget->SetContentsView(
      std::make_unique<AssistantOnboardingView>(&delegate));
  EXPECT_CALL(delegate, DownloadImage)
      .WillOnce(testing::Invoke(
          [&](const GURL& url, ImageDownloader::DownloadCallback callback) {
            std::move(callback).Run(expected);
          }));

  SetOnboardingSuggestions({CreateSuggestionWithIconUrl(
      "https://www.gstatic.com/images/branding/product/2x/googleg_48dp.png")});

  AssistantOnboardingSuggestionView* suggestion_view = nullptr;
  FindDescendentByClassName(onboarding_view, &suggestion_view);
  ASSERT_NE(nullptr, suggestion_view);

  const auto& actual = suggestion_view->GetIcon();
  EXPECT_TRUE(actual.BackedBySameObjectAs(expected));
}

TEST_F(DISABLED_AssistantOnboardingViewTest, DarkAndLightTheme) {
  AshColorProvider* color_provider = AshColorProvider::Get();
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  ShowAssistantUi();

  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();
  const SkColor initial_greeting_label_color =
      greeting_label()->GetEnabledColor();
  const SkColor initial_intro_label_color = intro_label()->GetEnabledColor();
  const SkColor intial_text_primary_color =
      color_provider->GetContentLayerColor(
          ColorProvider::ContentLayerType::kTextColorPrimary);
  EXPECT_EQ(initial_greeting_label_color, intial_text_primary_color);
  EXPECT_EQ(initial_intro_label_color, intial_text_primary_color);

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());
  const SkColor text_primary_color = color_provider->GetContentLayerColor(
      ColorProvider::ContentLayerType::kTextColorPrimary);
  EXPECT_NE(intial_text_primary_color, text_primary_color);

  // Check that both label colors are updated to the text primary color,
  // calculated based on the new color mode.
  const SkColor greeting_label_color = greeting_label()->GetEnabledColor();
  const SkColor intro_label_color = intro_label()->GetEnabledColor();
  EXPECT_EQ(greeting_label_color, text_primary_color);
  EXPECT_EQ(intro_label_color, text_primary_color);
}

}  // namespace
}  // namespace ash
