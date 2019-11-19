// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_MAIN_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_MAIN_VIEW_H_

#include <memory>
#include <vector>

#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class AssistantMainStage;
class AssistantOverlay;
class AssistantViewDelegate;
class CaptionBar;
class DialogPlate;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantMainViewDeprecated
    : public views::View,
      public AssistantUiModelObserver {
 public:
  explicit AssistantMainViewDeprecated(AssistantViewDelegate* delegate);
  ~AssistantMainViewDeprecated() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;
  void VisibilityChanged(views::View* starting_from, bool visible) override;
  void RequestFocus() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // Returns the first focusable view or nullptr to defer to views::FocusSearch.
  views::View* FindFirstFocusableView();

  // Returns the overlays that behave as pseudo-children of AssistantMainView.
  std::vector<AssistantOverlay*> GetOverlays();

 private:
  void InitLayout();

  AssistantViewDelegate* const delegate_;

  CaptionBar* caption_bar_;                         // Owned by view hierarchy.
  DialogPlate* dialog_plate_;                       // Owned by view hierarchy.
  AssistantMainStage* main_stage_;                  // Owned by view hierarchy.

  // Overlays behave as pseudo-children of AssistantMainView. They paint to a
  // higher lever in the layer tree so they are visible over the top of
  // Assistant cards.
  std::vector<std::unique_ptr<AssistantOverlay>> overlays_;

  int min_height_dip_;

  DISALLOW_COPY_AND_ASSIGN(AssistantMainViewDeprecated);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_MAIN_VIEW_H_
