// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_ASSISTANT_ASH_TEST_BASE_H_
#define ASH_ASSISTANT_TEST_ASSISTANT_ASH_TEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/mocked_assistant_interaction.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Textfield;
class View;
class Widget;
}  // namespace views

namespace ash {
namespace assistant {
class ScopedAssistantBrowserDelegate;
}

class AppListView;
class AssistantOnboardingSuggestionView;
class AssistantTestApi;
class SuggestionChipView;
class TestAssistantService;
class TestAssistantSetup;
class TestAshWebViewFactory;

// Helper class to make testing the Assistant Ash UI easier.
class AssistantAshTestBase : public AshTestBase {
 public:
  using AssistantEntryPoint = assistant::AssistantEntryPoint;
  using AssistantExitPoint = assistant::AssistantExitPoint;
  using AssistantOnboardingMode = assistant::prefs::AssistantOnboardingMode;
  using ConsentStatus = assistant::prefs::ConsentStatus;

  AssistantAshTestBase();
  explicit AssistantAshTestBase(base::test::TaskEnvironment::TimeSource time);

  AssistantAshTestBase(const AssistantAshTestBase&) = delete;
  AssistantAshTestBase& operator=(const AssistantAshTestBase&) = delete;

  ~AssistantAshTestBase() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Creates and switches to a new active user.
  void CreateAndSwitchActiveUser(const std::string& display_email,
                                 const std::string& given_name);

  // Show the Assistant UI. The optional |entry_point| can be used to emulate
  // the different ways of launching the Assistant.
  void ShowAssistantUi(
      AssistantEntryPoint entry_point = AssistantEntryPoint::kUnspecified);
  // Close the Assistant UI. The optional |exit_point| can be used to emulate
  // the different ways of closing the Assistant, such as without closing the
  // launcher.
  void CloseAssistantUi(
      AssistantExitPoint exit_point = AssistantExitPoint::kUnspecified);

  // Open the launcher (but do not open the Assistant UI).
  void OpenLauncher();
  // Close the Assistant UI by closing the launcher.
  void CloseLauncher();

  void SetTabletMode(bool enable);

  // Change the user preference controlling the status of user consent.
  void SetConsentStatus(ConsentStatus consent_status);

  // Sets the number of user sessions where Assistant onboarding was shown.
  void SetNumberOfSessionsWhereOnboardingShown(int number_of_sessions);

  // Changes the user preference controlling the mode of the onboarding UX.
  void SetOnboardingMode(AssistantOnboardingMode onboarding_mode);

  // Change the user setting controlling whether the user prefers voice or
  // keyboard.
  void SetPreferVoice(bool value);

  // Sets the time of the user's last interaction with Assistant.
  void SetTimeOfLastInteraction(const base::Time& time);

  void StartOverview();

  // Return true if the Assistant UI is visible.
  bool IsVisible();

  // This is the top-level Assistant specific view.
  // Can only be used after |ShowAssistantUi| has been called.
  // Exists for both bubble launcher and fullscreen launcher.
  views::View* page_view();

  // Return the app list view hosting the Assistant page view.
  // Can only be used after |ShowAssistantUi| has been called.
  // Only exists for fullscreen launcher.
  AppListView* app_list_view();

  // Return the root view hosting the Assistant page view.
  // Can only be used after |ShowAssistantUi| has been called.
  views::View* root_view();

  // Simulate the user entering a query.
  // Returns a builder object that allows you to specify the query and the
  // responses.  The interaction will be auto submitted in the destructor,
  // meaning you should just use it and let it go out of scope.
  // Example usage:
  //
  //    MockTextInteraction()
  //       .WithQuery("a query")
  //       .WithTextResponse("First response")
  //       .WithTextResponse("Second response");
  MockedAssistantInteraction MockTextInteraction();

  // Simulate the user entering a query followed by <return>.
  void SendQueryThroughTextField(const std::string& query);

  // Simulate the user tapping on the given view.
  // Waits for the event to be processed.
  void TapOnAndWait(const views::View* view);

  // Simulate the user tapping at the given position.
  // Waits for the event to be processed.
  void TapAndWait(gfx::Point position);

  // Simulate a mouse click on the given view.
  // Waits for the event to be processed.
  void ClickOnAndWait(const views::View* view,
                      bool check_if_view_can_process_events = true);

  // Return the current interaction. Returns |std::nullopt| if no interaction
  // is in progress.
  std::optional<ash::assistant::AssistantInteractionMetadata>
  current_interaction();

  // Create a new App window, and activate it.
  // Returns a pointer to the newly created window.
  // The window will be destroyed when the test is finished.
  aura::Window* SwitchToNewAppWindow();

  // Create a new Widget, and activate it.
  // Returns a pointer to the newly created widget.
  // The widget will be destroyed when the test is finished.
  views::Widget* SwitchToNewWidget();

  // Return the window containing the Assistant UI.
  // Note that this window is shared for all components of the |AppList|.
  aura::Window* window();

  // Return the text field used for inputting new queries.
  views::Textfield* input_text_field();

  // Return the mic field used for dictating new queries.
  views::View* mic_view();

  // Return the greeting label shown when you first open the Assistant.
  views::View* greeting_label();

  // Return the button to enable voice mode.
  views::View* voice_input_toggle();

  // Return the button to enable text mode.
  views::View* keyboard_input_toggle();

  // Return the Assistant onboarding view.
  views::View* onboarding_view();

  // Return the button to launch Assistant setup.
  views::View* opt_in_view();

  // Return the container with all the suggestion chips.
  views::View* suggestion_chip_container();

  // Return the onboarding suggestions that are currently displayed.
  std::vector<AssistantOnboardingSuggestionView*>
  GetOnboardingSuggestionViews();

  // Return the suggestion chips that are currently displayed.
  std::vector<SuggestionChipView*> GetSuggestionChips();

  // Show/Dismiss the on-screen keyboard.
  void ShowKeyboard();
  void DismissKeyboard();

  // Returns if the on-screen keyboard is being displayed.
  bool IsKeyboardShowing() const;

  // Enable/Disable the on-screen keyboard.
  void EnableKeyboard() { SetVirtualKeyboardEnabled(true); }
  void DisableKeyboard() { SetVirtualKeyboardEnabled(false); }

  TestAssistantService* assistant_service();

 protected:
  // Sets up an active user for a test. Note that this function is called in
  // `SetUp` by default. You can change this behavior by setting
  // `set_up_active_user_in_test_set_up_`.
  void SetUpActiveUser();

  // This variable must be set before `SetUp` function call.
  bool set_up_active_user_in_test_set_up_ = true;

 private:
  std::unique_ptr<AssistantTestApi> test_api_;
  std::unique_ptr<TestAssistantSetup> test_setup_;
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_;

  std::vector<std::unique_ptr<aura::Window>> windows_;
  std::vector<std::unique_ptr<views::Widget>> widgets_;

  std::unique_ptr<assistant::ScopedAssistantBrowserDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_ASSISTANT_ASH_TEST_BASE_H_
