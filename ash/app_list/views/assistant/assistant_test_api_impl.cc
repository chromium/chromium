// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_test_api_impl.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_page.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

std::unique_ptr<AssistantTestApi> AssistantTestApi::Create() {
  return std::make_unique<AssistantTestApiImpl>();
}

AssistantTestApiImpl::AssistantTestApiImpl() = default;

AssistantTestApiImpl::~AssistantTestApiImpl() {
  EnableAnimations();
}

void AssistantTestApiImpl::DisableAnimations() {
  AppListView::SetShortAnimationForTesting(true);

  scoped_animation_duration_ =
      std::make_unique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
}

bool AssistantTestApiImpl::IsVisible() {
  return page_view()->GetVisible();
}

void AssistantTestApiImpl::SendTextQuery(const std::string& query) {
  if (!input_text_field()->HasFocus()) {
    ADD_FAILURE()
        << "The TextField should be focussed before we can send a query";
  }

  input_text_field()->SetText(base::UTF8ToUTF16(query));
  // Send <return> to commit the query.
  SendKeyPress(ui::KeyboardCode::VKEY_RETURN);
}

views::View* AssistantTestApiImpl::page_view() {
  const int index = contents_view()->GetPageIndexForState(
      AppListState::kStateEmbeddedAssistant);
  return static_cast<views::View*>(contents_view()->GetPageView(index));
}

views::View* AssistantTestApiImpl::main_view() {
  return page_view()->GetViewByID(AssistantViewID::kMainView);
}

views::Textfield* AssistantTestApiImpl::input_text_field() {
  return static_cast<views::Textfield*>(
      page_view()->GetViewByID(AssistantViewID::kTextQueryField));
}

views::View* AssistantTestApiImpl::mic_view() {
  return page_view()->GetViewByID(AssistantViewID::kMicView);
}

views::View* AssistantTestApiImpl::greeting_label() {
  return page_view()->GetViewByID(AssistantViewID::kGreetingLabel);
}

aura::Window* AssistantTestApiImpl::window() {
  return main_view()->GetWidget()->GetNativeWindow();
}

void AssistantTestApiImpl::EnableAssistant() {
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
      chromeos::assistant::prefs::kAssistantEnabled, true);
}

void AssistantTestApiImpl::SetTabletMode(bool enable) {
  TabletMode::Get()->SetEnabledForTest(enable);
}

void AssistantTestApiImpl::SetPreferVoice(bool value) {
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
      chromeos::assistant::prefs::kAssistantLaunchWithMicOpen, value);
}

void AssistantTestApiImpl::EnableAnimations() {
  scoped_animation_duration_ = nullptr;
  AppListView::SetShortAnimationForTesting(false);
}

ContentsView* AssistantTestApiImpl::contents_view() {
  auto* app_list_view =
      Shell::Get()->app_list_controller()->presenter()->GetView();

  DCHECK(app_list_view) << "AppListView has not been initialized yet. "
                           "Be sure to display the Assistant UI first.";

  return app_list_view->app_list_main_view()->contents_view();
}

void AssistantTestApiImpl::SendKeyPress(ui::KeyboardCode key) {
  ui::test::EventGenerator event_generator(window()->GetRootWindow());
  event_generator.PressKey(key, /*flags=*/ui::EF_NONE);
}

}  // namespace ash
