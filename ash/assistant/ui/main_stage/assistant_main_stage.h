// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_MAIN_STAGE_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_MAIN_STAGE_H_

#include <memory>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ui {
class CallbackLayerAnimationObserver;
}  // namespace ui

namespace views {
class Label;
}  // namespace views

namespace ash {

class AssistantFooterView;
class AssistantHeaderView;
class AssistantProgressIndicator;
class AssistantQueryView;
class AssistantViewDelegate;
class UiElementContainerView;

// AssistantMainStage is the child of AssistantMainView responsible for
// displaying the Assistant interaction to the user. This includes visual
// affordances for the query, response, as well as suggestions.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantMainStage
    : public views::View,
      public views::ViewObserver,
      public AssistantInteractionModelObserver,
      public AssistantUiModelObserver {
 public:
  explicit AssistantMainStage(AssistantViewDelegate* delegate);
  ~AssistantMainStage() override;

  // views::View:
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override;
  void OnViewPreferredSizeChanged(views::View* view) override;
  void OnViewVisibilityChanged(views::View* view,
                               views::View* starting_view) override;

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
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

 private:
  void InitLayout();
  void InitContentLayoutContainer();
  void InitQueryLayoutContainer();
  void InitOverlayLayoutContainer();

  void UpdateTopPadding();
  void UpdateQueryViewTransform(views::View* query_view);
  void UpdateFooter();

  void OnActivateQuery();
  void OnActiveQueryCleared();
  bool OnActiveQueryExitAnimationEnded(
      const ui::CallbackLayerAnimationObserver& observer);

  void AnimateInGreetingLabel();

  void OnFooterAnimationStarted(
      const ui::CallbackLayerAnimationObserver& observer);
  bool OnFooterAnimationEnded(
      const ui::CallbackLayerAnimationObserver& observer);

  AssistantViewDelegate* const delegate_;  // Owned by Shell.

  // Content layout container and children. Owned by view hierarchy.
  AssistantHeaderView* header_;
  views::View* content_layout_container_;
  UiElementContainerView* ui_element_container_;
  AssistantFooterView* footer_;

  // Query layout container and children. Owned by view hierarchy.
  views::View* query_layout_container_;
  AssistantQueryView* active_query_view_ = nullptr;
  AssistantQueryView* committed_query_view_ = nullptr;
  AssistantQueryView* pending_query_view_ = nullptr;

  // Overlay layout container and children. Owned by view hierarchy.
  views::View* overlay_layout_container_;
  views::Label* greeting_label_;
  AssistantProgressIndicator* progress_indicator_;

  std::unique_ptr<ui::CallbackLayerAnimationObserver>
      active_query_exit_animation_observer_;

  std::unique_ptr<ui::CallbackLayerAnimationObserver>
      footer_animation_observer_;

  // True if this is the first query received for the current Assistant UI
  // session, false otherwise.
  bool is_first_query_ = true;

  DISALLOW_COPY_AND_ASSIGN(AssistantMainStage);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_MAIN_STAGE_H_
