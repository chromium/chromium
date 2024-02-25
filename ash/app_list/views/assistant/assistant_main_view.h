// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_VIEW_H_

#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {
class AssistantDialogPlate;
class AppListAssistantMainStage;
class AssistantViewDelegate;

// Manages the dialog plate (input area) and main stage (output area),
// including animation when a new assistant session starts.
class ASH_EXPORT AssistantMainView : public views::View,
                                     public AssistantControllerObserver,
                                     public AssistantUiModelObserver {
  METADATA_HEADER(AssistantMainView, views::View)

 public:
  explicit AssistantMainView(AssistantViewDelegate* delegate);
  AssistantMainView(const AssistantMainView&) = delete;
  AssistantMainView& operator=(const AssistantMainView&) = delete;
  ~AssistantMainView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void RequestFocus() override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) override;

  // Returns the first focusable view or nullptr to defer to views::FocusSearch.
  views::View* FindFirstFocusableView();

 private:
  void InitLayout();

  const raw_ptr<AssistantViewDelegate> delegate_;

  raw_ptr<AssistantDialogPlate> dialog_plate_;     // Owned by view hierarchy.
  raw_ptr<AppListAssistantMainStage> main_stage_;  // Owned by view hierarchy.

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_VIEW_H_
