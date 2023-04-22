// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ZERO_STATE_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ZERO_STATE_VIEW_H_

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AppListToastView;
class AssistantOnboardingView;
class AssistantViewDelegate;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantZeroStateView
    : public views::View,
      public AssistantControllerObserver,
      public AssistantUiModelObserver {
 public:
  explicit AssistantZeroStateView(AssistantViewDelegate* delegate);
  AssistantZeroStateView(const AssistantZeroStateView&) = delete;
  AssistantZeroStateView& operator=(const AssistantZeroStateView&) = delete;
  ~AssistantZeroStateView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;

  // AssistantController:
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      absl::optional<AssistantEntryPoint> entry_point,
      absl::optional<AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();
  void UpdateLayout();
  void OnLearnMoreButtonPressed();

  // Owned by AssistantController.
  const raw_ptr<AssistantViewDelegate, ExperimentalAsh> delegate_;

  // Owned by view hierarchy;
  raw_ptr<AssistantOnboardingView, ExperimentalAsh> onboarding_view_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> greeting_label_ = nullptr;
  base::raw_ptr<views::View> spacer_ = nullptr;
  base::raw_ptr<AppListToastView> learn_more_toast_ = nullptr;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ZERO_STATE_VIEW_H_
