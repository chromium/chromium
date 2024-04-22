// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ZERO_STATE_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ZERO_STATE_VIEW_H_

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AssistantOnboardingView;
class AssistantViewDelegate;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantZeroStateView
    : public views::View,
      public AssistantControllerObserver,
      public AssistantUiModelObserver,
      public LauncherSearchIphView::Delegate {
  METADATA_HEADER(AssistantZeroStateView, views::View)

 public:
  explicit AssistantZeroStateView(AssistantViewDelegate* delegate);
  AssistantZeroStateView(const AssistantZeroStateView&) = delete;
  AssistantZeroStateView& operator=(const AssistantZeroStateView&) = delete;
  ~AssistantZeroStateView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;

  // AssistantController:
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) override;

  // LauncherSearchIphView::Delegate:
  void RunLauncherSearchQuery(const std::u16string& query) override;
  void OpenAssistantPage() override;

 private:
  void InitLayout();
  void UpdateLayout();

  // Owned by AssistantController.
  const raw_ptr<AssistantViewDelegate> delegate_;

  // Owned by view hierarchy;
  raw_ptr<AssistantOnboardingView> onboarding_view_ = nullptr;
  raw_ptr<views::Label> greeting_label_ = nullptr;
  raw_ptr<views::View> spacer_ = nullptr;
  raw_ptr<LauncherSearchIphView> iph_view_ = nullptr;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ZERO_STATE_VIEW_H_
