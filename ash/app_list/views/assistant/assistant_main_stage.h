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
#include "base/memory/raw_ptr.h"
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
  METADATA_HEADER(AppListAssistantMainStage, views::View)

 public:
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
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) override;

  void InitializeUIForBubbleView();

 private:
  void InitLayout();
  void InitLayoutWithIph();
  std::unique_ptr<views::View> CreateContentLayoutContainer();
  std::unique_ptr<views::View> CreateMainContentLayoutContainer();
  std::unique_ptr<views::View> CreateDividerLayoutContainer();
  std::unique_ptr<views::View> CreateFooterLayoutContainer();

  void AnimateInZeroState();
  void AnimateInFooter();

  void MaybeHideZeroStateAndShowFooter();
  void InitializeUIForStartingSession(bool from_search);

  AssistantQueryView* query_view() {
    return query_view_observation_.GetSource();
  }
  const AssistantQueryView* query_view() const {
    return query_view_observation_.GetSource();
  }

  UiElementContainerView* ui_element_container() {
    return ui_element_container_observation_.GetSource();
  }
  const UiElementContainerView* ui_element_container() const {
    return ui_element_container_observation_.GetSource();
  }

  AssistantFooterView* footer() { return footer_observation_.GetSource(); }
  const AssistantFooterView* footer() const {
    return footer_observation_.GetSource();
  }

  const raw_ptr<AssistantViewDelegate> delegate_;  // Owned by Shell.

  // Owned by view hierarchy.
  raw_ptr<AssistantProgressIndicator> progress_indicator_;
  raw_ptr<views::Separator> horizontal_separator_;
  // The observed views are owned by the view hierarchy. These could be a
  // raw_ptr to the view + ScopedObservation, but accessing the view through
  // the ScopedObservation saves a pointer.
  base::ScopedObservation<AssistantQueryView, ViewObserver>
      query_view_observation_{this};
  base::ScopedObservation<UiElementContainerView, ViewObserver>
      ui_element_container_observation_{this};
  raw_ptr<AssistantZeroStateView> zero_state_view_;
  base::ScopedObservation<AssistantFooterView, ViewObserver>
      footer_observation_{this};

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_ASSISTANT_ASSISTANT_MAIN_STAGE_H_
