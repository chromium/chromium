// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_VIEW_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_VIEW_H_

#include <memory>

#include "ash/app_list/app_list_export.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {
class AssistantDialogPlate;
class AppListAssistantMainStage;
class AssistantViewDelegate;

class APP_LIST_EXPORT AssistantMainView : public views::View,
                                          public AssistantUiModelObserver {
 public:
  explicit AssistantMainView(AssistantViewDelegate* delegate);
  ~AssistantMainView() override;

  // views::View:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void RequestFocus() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // Returns the first focusable view or nullptr to defer to views::FocusSearch.
  views::View* FindFirstFocusableView();

 private:
  void InitLayout();

  AssistantViewDelegate* const delegate_;

  AssistantDialogPlate* dialog_plate_;     // Owned by view hierarchy.
  AppListAssistantMainStage* main_stage_;  // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantMainView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_VIEW_H_
