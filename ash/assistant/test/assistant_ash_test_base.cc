// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/assistant_ash_test_base.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/assistant/assistant_main_view.h"
#include "ash/app_list/views/assistant/assistant_page_view.h"
#include "ash/assistant/assistant_controller.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/test/assistant_test_api.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {

using chromeos::assistant::mojom::AssistantInteractionMetadata;
using chromeos::assistant::mojom::AssistantInteractionType;

gfx::Point GetPointInside(const views::View* view) {
  return view->GetBoundsInScreen().CenterPoint();
}

bool CanProcessEvents(const views::View* view) {
  const views::View* ancestor = view;
  while (ancestor != nullptr) {
    if (!ancestor->CanProcessEventsWithinSubtree())
      return false;
    ancestor = ancestor->parent();
  }
  return true;
}

}  // namespace

AssistantAshTestBase::AssistantAshTestBase()
    : test_api_(AssistantTestApi::Create()) {}

AssistantAshTestBase::~AssistantAshTestBase() = default;

void AssistantAshTestBase::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(
      app_list_features::kEnableAssistantLauncherUI);

  // Enable virtual keyboard.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      keyboard::switches::kEnableVirtualKeyboard);

  AshTestBase::SetUp();

  // Make the display big enough to hold the app list.
  UpdateDisplay("1024x768");

  // Enable Assistant in settings.
  test_api_->EnableAssistant();

  // Cache controller.
  controller_ = Shell::Get()->assistant_controller();
  DCHECK(controller_);

  // At this point our Assistant service is ready for use.
  // Indicate this by changing status from NOT_READY to READY.
  AssistantState::Get()->NotifyStatusChanged(mojom::AssistantState::READY);

  test_api_->DisableAnimations();

  // Wait for virtual keyboard to load.
  SetTouchKeyboardEnabled(true);
}

void AssistantAshTestBase::TearDown() {
  windows_.clear();
  SetTouchKeyboardEnabled(false);
  AshTestBase::TearDown();
  scoped_feature_list_.Reset();
}

void AssistantAshTestBase::ShowAssistantUi(AssistantEntryPoint entry_point) {
  if (entry_point == AssistantEntryPoint::kHotword) {
    // If the Assistant is triggered via Hotword, the interaction is triggered
    // by the Assistant service.
    assistant_service()->StartVoiceInteraction();
  } else {
    // Otherwise, the interaction is triggered by a call to |ShowUi|.
    controller_->ui_controller()->ShowUi(entry_point);
  }
  // Send all mojom messages to/from the assistant service.
  base::RunLoop().RunUntilIdle();
}

void AssistantAshTestBase::CloseAssistantUi(AssistantExitPoint exit_point) {
  controller_->ui_controller()->CloseUi(exit_point);
}

void AssistantAshTestBase::CloseLauncher() {
  ash::Shell::Get()->app_list_controller()->ViewClosing();
}

void AssistantAshTestBase::SetTabletMode(bool enable) {
  test_api_->SetTabletMode(enable);
}

void AssistantAshTestBase::SetPreferVoice(bool prefer_voice) {
  test_api_->SetPreferVoice(prefer_voice);
}

views::View* AssistantAshTestBase::main_view() {
  return test_api_->main_view();
}

views::View* AssistantAshTestBase::page_view() {
  return test_api_->page_view();
}

void AssistantAshTestBase::MockAssistantInteractionWithResponse(
    const std::string& response_text) {
  const std::string query = std::string("input text");

  SendQueryThroughTextField(query);
  assistant_service()->SetInteractionResponse(
      InteractionResponse()
          .AddTextResponse(response_text)
          .AddResolution(InteractionResponse::Resolution::kNormal)
          .Clone());

  base::RunLoop().RunUntilIdle();
}

void AssistantAshTestBase::SendQueryThroughTextField(const std::string& query) {
  test_api_->SendTextQuery(query);
}

void AssistantAshTestBase::TapOnTextField() {
  if (!CanProcessEvents(input_text_field()))
    ADD_FAILURE() << "TextField can not process tap events";

  GetEventGenerator()->GestureTapAt(GetPointInside(input_text_field()));
}

aura::Window* AssistantAshTestBase::SwitchToNewAppWindow() {
  windows_.push_back(CreateAppWindow());

  aura::Window* window = windows_.back().get();
  window->SetName("<app-window>");
  return window;
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

void AssistantAshTestBase::ShowKeyboard() {
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(/*lock=*/false);
}

bool AssistantAshTestBase::IsKeyboardShowing() const {
  return keyboard::IsKeyboardShowing();
}

AssistantInteractionController* AssistantAshTestBase::interaction_controller() {
  return controller_->interaction_controller();
}

TestAssistantService* AssistantAshTestBase::assistant_service() {
  return ash_test_helper()->test_assistant_service();
}

}  // namespace ash
