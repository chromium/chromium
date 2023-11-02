// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_TEST_API_IMPL_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_TEST_API_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/test/assistant_test_api.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {
class ScopedAnimationDurationScaleMode;
}  // namespace ui

namespace ash {

class ContentsView;

class AssistantTestApiImpl : public AssistantTestApi {
 public:
  AssistantTestApiImpl();

  AssistantTestApiImpl(const AssistantTestApiImpl&) = delete;
  AssistantTestApiImpl& operator=(const AssistantTestApiImpl&) = delete;

  ~AssistantTestApiImpl() override;

  // AssistantTestApi overrides:
  void DisableAnimations() override;
  bool IsVisible() override;
  void SendTextQuery(const std::string& query) override;
  void EnableAssistantAndWait() override;
  void SetAssistantEnabled(bool enable) override;
  void SetScreenContextEnabled(bool enabled) override;
  void SetTabletMode(bool enable) override;
  void SetConsentStatus(assistant::prefs::ConsentStatus) override;
  void SetNumberOfSessionsWhereOnboardingShown(int number_of_sessions) override;
  void SetOnboardingMode(assistant::prefs::AssistantOnboardingMode) override;
  void SetPreferVoice(bool value) override;
  void SetTimeOfLastInteraction(base::Time time) override;
  void StartOverview() override;
  AssistantState* GetAssistantState() override;
  void WaitUntilIdle() override;
  views::View* page_view() override;
  views::View* main_view() override;
  views::View* ui_element_container() override;
  views::Textfield* input_text_field() override;
  views::View* mic_view() override;
  views::View* greeting_label() override;
  views::View* voice_input_toggle() override;
  views::View* keyboard_input_toggle() override;
  views::View* suggestion_chip_container() override;
  views::View* onboarding_view() override;
  views::View* opt_in_view() override;
  aura::Window* window() override;
  AppListView* app_list_view() override;
  aura::Window* root_window() override;

 private:
  bool AppListViewsHaveBeenCreated() const;
  ContentsView* contents_view();
  ContentsView* contents_view_or_null() const;

  void SendKeyPress(ui::KeyboardCode key);

  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      scoped_animation_duration_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_TEST_API_IMPL_H_
