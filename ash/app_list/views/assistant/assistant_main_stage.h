// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_

#include <memory>

#include "ash/app_list/app_list_export.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class Label;
}  // namespace views

namespace ash {
class AssistantFooterView;
class AssistantProgressIndicator;
class AssistantQueryView;
class AssistantViewDelegate;
class UiElementContainerView;

// AppListAssistantMainStage is the child of AssistantMainView responsible for
// displaying the Assistant interaction to the user. This includes visual
// affordances for the query, response, as well as suggestions.
class APP_LIST_EXPORT AppListAssistantMainStage
    : public views::View,
      public views::ViewObserver,
      public ash::AssistantInteractionModelObserver,
      public ash::AssistantUiModelObserver {
 public:
  explicit AppListAssistantMainStage(ash::AssistantViewDelegate* delegate);
  ~AppListAssistantMainStage() override;

  // views::View:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override;

  // AssistantInteractionModelObserver:
  void OnCommittedQueryChanged(const ash::AssistantQuery& query) override;
  void OnPendingQueryChanged(const ash::AssistantQuery& query) override;
  void OnPendingQueryCleared(bool due_to_commit) override;
  void OnResponseChanged(
      const scoped_refptr<ash::AssistantResponse>& response) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      ash::AssistantVisibility new_visibility,
      ash::AssistantVisibility old_visibility,
      base::Optional<ash::AssistantEntryPoint> entry_point,
      base::Optional<ash::AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();
  views::View* CreateContentLayoutContainer();
  void InitGreetingLabel();
  views::View* CreateMainContentLayoutContainer();
  views::View* CreateDividerLayoutContainer();
  views::View* CreateFooterLayoutContainer();

  void AnimateInGreetingLabel();
  void AnimateInFooter();

  void MaybeHideGreetingLabel();

  ash::AssistantViewDelegate* const delegate_;  // Owned by Shell.

  // Owned by view hierarchy.
  ash::AssistantProgressIndicator* progress_indicator_;
  views::View* horizontal_separator_;
  ash::AssistantQueryView* query_view_;
  ash::UiElementContainerView* ui_element_container_;
  views::Label* greeting_label_;
  ash::AssistantFooterView* footer_;

  DISALLOW_COPY_AND_ASSIGN(AppListAssistantMainStage);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_
