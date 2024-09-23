// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "base/memory/raw_ptr.h"

#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/assistant/test/test_assistant_setup.h"
#include "ash/assistant/ui/main_stage/assistant_onboarding_suggestion_view.h"
#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/test/assistant_test_api.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/assistant/test_support/scoped_assistant_browser_delegate.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

gfx::Point GetPointInside(const views::View* view) {
  return view->GetBoundsInScreen().CenterPoint();
}

bool CanProcessEvents(const views::View* view) {
  const views::View* ancestor = view;
  while (ancestor != nullptr) {
    if (!ancestor->GetCanProcessEventsWithinSubtree())
      return false;
    ancestor = ancestor->parent();
  }
  return true;
}

void CheckCanProcessEvents(const views::View* view) {
  if (!view->IsDrawn()) {
    ADD_FAILURE()
        << view->GetClassName()
        << " can not process events because it is not drawn on screen.";
  } else if (!CanProcessEvents(view)) {
    ADD_FAILURE() << view->GetClassName() << " can not process events.";
  }
}

void PressHomeButton() {
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      AppListShowSource::kShelfButton, base::TimeTicks::Now());
}

// Collects all child views of the given templated type.
// This includes direct and indirect children.
// For this class to work, _ChildView must:
//      * Inherit from |views::View|.
//      * Implement view metadata (see comments on views::View).
template <class _ChildView>
class ChildViewCollector {
 public:
  using Views = std::vector<_ChildView*>;

  explicit ChildViewCollector(const views::View* parent) : parent_(parent) {}

  Views Get() {
    Views result;
    for (views::View* child : parent_->children())
      Get(child, &result);
    return result;
  }

 private:
  void Get(views::View* view, Views* result) {
    if (views::IsViewClass<_ChildView>(view))
      result->push_back(static_cast<_ChildView*>(view));
    for (views::View* child : view->children())
      Get(child, result);
  }

  raw_ptr<const views::View> parent_;
};

}  // namespace

AssistantAshTestBase::AssistantAshTestBase()
    : AssistantAshTestBase(
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}

AssistantAshTestBase::AssistantAshTestBase(
    base::test::TaskEnvironment::TimeSource time)
    : AshTestBase(time),
      test_api_(AssistantTestApi::Create()),
      test_setup_(std::make_unique<TestAssistantSetup>()),
      test_web_view_factory_(std::make_unique<TestAshWebViewFactory>()),
      delegate_(std::make_unique<assistant::ScopedAssistantBrowserDelegate>()) {
}

AssistantAshTestBase::~AssistantAshTestBase() = default;

void AssistantAshTestBase::SetUp() {
  AshTestBase::SetUp();

  // Make the display big enough to hold the app list.
  UpdateDisplay("1024x768");

  test_api_->DisableAnimations();
  EnableKeyboard();

  if (set_up_active_user_in_test_set_up_) {
    SetUpActiveUser();
  }
}

void AssistantAshTestBase::TearDown() {
  windows_.clear();
  widgets_.clear();
  DisableKeyboard();
  AshTestBase::TearDown();
}

void AssistantAshTestBase::CreateAndSwitchActiveUser(
    const std::string& display_email,
    const std::string& given_name) {
  TestSessionControllerClient* session_controller_client =
      ash_test_helper()->test_session_controller_client();

  session_controller_client->Reset();

  session_controller_client->AddUserSession(
      display_email, user_manager::UserType::kRegular,
      /*provide_pref_service=*/true,
      /*is_new_profile=*/false, given_name);

  session_controller_client->SwitchActiveUser(Shell::Get()
                                                  ->session_controller()
                                                  ->GetUserSession(0)
                                                  ->user_info.account_id);

  session_controller_client->SetSessionState(
      session_manager::SessionState::ACTIVE);

  SetUpActiveUser();
}

void AssistantAshTestBase::ShowAssistantUi(AssistantEntryPoint entry_point) {
  if (entry_point == AssistantEntryPoint::kHotword) {
    // If the Assistant is triggered via Hotword, the interaction is triggered
    // by the Assistant service.
    assistant_service()->StartVoiceInteraction();
  } else {
    // Otherwise, the interaction is triggered by a call to ShowUi().
    AssistantUiController::Get()->ShowUi(entry_point);
  }
  // Send all mojom messages to/from the assistant service.
  base::RunLoop().RunUntilIdle();
  // Ensure assistant page is visible and has finished layout to non-zero size.
  ViewDrawnWaiter().Wait(page_view());
}

void AssistantAshTestBase::CloseAssistantUi(AssistantExitPoint exit_point) {
  AssistantUiController::Get()->CloseUi(exit_point);
}

void AssistantAshTestBase::OpenLauncher() {
  PressHomeButton();
}

void AssistantAshTestBase::CloseLauncher() {
  Shell::Get()->app_list_controller()->DismissAppList();
}

void AssistantAshTestBase::SetTabletMode(bool enable) {
  test_api_->SetTabletMode(enable);
}

void AssistantAshTestBase::SetConsentStatus(ConsentStatus consent_status) {
  test_api_->SetConsentStatus(consent_status);
}

void AssistantAshTestBase::SetNumberOfSessionsWhereOnboardingShown(
    int number_of_sessions) {
  test_api_->SetNumberOfSessionsWhereOnboardingShown(number_of_sessions);
}

void AssistantAshTestBase::SetOnboardingMode(
    AssistantOnboardingMode onboarding_mode) {
  test_api_->SetOnboardingMode(onboarding_mode);
}

void AssistantAshTestBase::SetPreferVoice(bool prefer_voice) {
  test_api_->SetPreferVoice(prefer_voice);
}

void AssistantAshTestBase::SetTimeOfLastInteraction(const base::Time& time) {
  test_api_->SetTimeOfLastInteraction(time);
}

void AssistantAshTestBase::StartOverview() {
  test_api_->StartOverview();
}

bool AssistantAshTestBase::IsVisible() {
  return test_api_->IsVisible();
}

views::View* AssistantAshTestBase::page_view() {
  return test_api_->page_view();
}

AppListView* AssistantAshTestBase::app_list_view() {
  return test_api_->app_list_view();
}

views::View* AssistantAshTestBase::root_view() {
  views::View* result = app_list_view();
  while (result && result->parent())
    result = result->parent();
  return result;
}

MockedAssistantInteraction AssistantAshTestBase::MockTextInteraction() {
  return MockedAssistantInteraction(test_api_.get(), assistant_service());
}

void AssistantAshTestBase::SendQueryThroughTextField(const std::string& query) {
  test_api_->SendTextQuery(query);
}

void AssistantAshTestBase::TapOnAndWait(const views::View* view) {
  CheckCanProcessEvents(view);
  TapAndWait(GetPointInside(view));
}

void AssistantAshTestBase::TapAndWait(gfx::Point position) {
  GetEventGenerator()->GestureTapAt(position);

  base::RunLoop().RunUntilIdle();
}

void AssistantAshTestBase::ClickOnAndWait(
    const views::View* view,
    bool check_if_view_can_process_events) {
  if (check_if_view_can_process_events)
    CheckCanProcessEvents(view);
  GetEventGenerator()->MoveMouseTo(GetPointInside(view));
  GetEventGenerator()->ClickLeftButton();

  base::RunLoop().RunUntilIdle();
}

std::optional<assistant::AssistantInteractionMetadata>
AssistantAshTestBase::current_interaction() {
  return assistant_service()->current_interaction();
}

aura::Window* AssistantAshTestBase::SwitchToNewAppWindow() {
  windows_.push_back(CreateAppWindow());

  aura::Window* window = windows_.back().get();
  window->SetName("<app-window>");
  return window;
}

views::Widget* AssistantAshTestBase::SwitchToNewWidget() {
  widgets_.push_back(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));

  views::Widget* result = widgets_.back().get();
  // Give the widget a non-zero size, otherwise things like tapping and clicking
  // on it do not work.
  result->SetBounds(gfx::Rect(500, 100));
  return result;
}

aura::Window* AssistantAshTestBase::window() {
  return test_api_->window();
}

views::Textfield* AssistantAshTestBase::input_text_field() {
  return test_api_->input_text_field();
}

views::View* AssistantAshTestBase::mic_view() {
  return test_api_->mic_view();
}

views::View* AssistantAshTestBase::greeting_label() {
  return test_api_->greeting_label();
}

views::View* AssistantAshTestBase::voice_input_toggle() {
  return test_api_->voice_input_toggle();
}

views::View* AssistantAshTestBase::keyboard_input_toggle() {
  return test_api_->keyboard_input_toggle();
}

views::View* AssistantAshTestBase::onboarding_view() {
  return test_api_->onboarding_view();
}

views::View* AssistantAshTestBase::opt_in_view() {
  return test_api_->opt_in_view();
}

views::View* AssistantAshTestBase::suggestion_chip_container() {
  return test_api_->suggestion_chip_container();
}

std::vector<AssistantOnboardingSuggestionView*>
AssistantAshTestBase::GetOnboardingSuggestionViews() {
  const views::View* container = onboarding_view();
  return ChildViewCollector<AssistantOnboardingSuggestionView>{container}.Get();
}

std::vector<SuggestionChipView*> AssistantAshTestBase::GetSuggestionChips() {
  const views::View* container = suggestion_chip_container();
  return ChildViewCollector<SuggestionChipView>{container}.Get();
}

void AssistantAshTestBase::ShowKeyboard() {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/false);
}

void AssistantAshTestBase::DismissKeyboard() {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->HideKeyboardImplicitlyByUser();
  EXPECT_FALSE(IsKeyboardShowing());
}

bool AssistantAshTestBase::IsKeyboardShowing() const {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  return keyboard_controller->IsEnabled() &&
         keyboard::test::IsKeyboardShowing();
}

TestAssistantService* AssistantAshTestBase::assistant_service() {
  return ash_test_helper()->test_assistant_service();
}

void AssistantAshTestBase::SetUpActiveUser() {
  // Enable Assistant in settings.
  test_api_->SetAssistantEnabled(true);

  // Enable screen context in settings.
  test_api_->SetScreenContextEnabled(true);

  // Set AssistantAllowedState to ALLOWED.
  test_api_->GetAssistantState()->NotifyFeatureAllowed(
      assistant::AssistantAllowedState::ALLOWED);

  // Set user consent so the suggestion chips are displayed.
  SetConsentStatus(ConsentStatus::kActivityControlAccepted);

  // At this point our Assistant service is ready for use.
  // Indicate this by changing status from NOT_READY to READY.
  test_api_->GetAssistantState()->NotifyStatusChanged(
      assistant::AssistantStatus::READY);
}

}  // namespace ash
