// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_MAIN_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_MAIN_VIEW_H_

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class AssistantController;
class AssistantMainStage;
class CaptionBar;
class DialogPlate;

class AssistantMainView : public views::View, public AssistantUiModelObserver {
 public:
  explicit AssistantMainView(AssistantController* assistant_controller);
  ~AssistantMainView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;
  void RequestFocus() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(AssistantVisibility new_visibility,
                             AssistantVisibility old_visibility,
                             AssistantSource source) override;

  // Returns the first focusable view or nullptr to defer to views::FocusSearch.
  views::View* FindFirstFocusableView();

 private:
  void InitLayout();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  CaptionBar* caption_bar_;                         // Owned by view hierarchy.
  DialogPlate* dialog_plate_;                       // Owned by view hierarchy.
  AssistantMainStage* main_stage_;                  // Owned by view hierarchy.

  int min_height_dip_;

  DISALLOW_COPY_AND_ASSIGN(AssistantMainView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_MAIN_VIEW_H_
