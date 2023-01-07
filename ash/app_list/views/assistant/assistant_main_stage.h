// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_
#define ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

class AssistantFooterView;
class AssistantProgressIndicator;
class AssistantQueryView;
class AssistantViewDelegate;
class AssistantZeroStateView;
class UiElementContainerView;

// AppListAssistantMainStage is the child of AssistantMainView responsible for
// displaying the Assistant interaction to the user. This includes visual
// affordances for the query, response, as well as suggestions.
class ASH_EXPORT AppListAssistantMainStage
    : public views::View,
      public views::ViewObserver,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver {
 public:
  METADATA_HEADER(AppListAssistantMainStage);

  explicit AppListAssistantMainStage(AssistantViewDelegate* delegate);
  AppListAssistantMainStage(const AppListAssistantMainStage&) = delete;
  AppListAssistantMainStage& operator=(const AppListAssistantMainStage&) =
      delete;
  ~AppListAssistantMainStage() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnThemeChanged() override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // AssistantInteractionModelObserver:
  void OnCommittedQueryChanged(const AssistantQuery& query) override;
  void OnPendingQueryChanged(const AssistantQuery& query) override;
  void OnPendingQueryCleared(bool due_to_commit) override;
  void OnResponseChanged(
      const scoped_refptr<AssistantResponse>& response) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      absl::optional<AssistantEntryPoint> entry_point,
      absl::optional<AssistantExitPoint> exit_point) override;

  void InitializeUIForBubbleView();

 private:
  void InitLayout();
  std::unique_ptr<views::View> CreateContentLayoutContainer();
  std::unique_ptr<views::View> CreateMainContentLayoutContainer();
  std::unique_ptr<views::View> CreateDividerLayoutContainer();
  std::unique_ptr<views::View> CreateFooterLayoutContainer();

  void AnimateInZeroState();
  void AnimateInFooter();

  void MaybeHideZeroState();
  void InitializeUIForStartingSession(bool from_search);

  AssistantViewDelegate* const delegate_;  // Owned by Shell.

  // Owned by view hierarchy.
  AssistantProgressIndicator* progress_indicator_;
  views::Separator* horizontal_separator_;
  AssistantQueryView* query_view_;
  UiElementContainerView* ui_element_container_;
  AssistantZeroStateView* zero_state_view_;
  AssistantFooterView* footer_;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_
