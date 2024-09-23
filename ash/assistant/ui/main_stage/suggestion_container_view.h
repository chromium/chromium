// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_

#include <memory>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/scroll_view.h"

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// SuggestionContainerView is the child of AssistantMainView concerned with
// laying out SuggestionChipViews in response to Assistant interaction model
// suggestion events.
class COMPONENT_EXPORT(ASSISTANT_UI) SuggestionContainerView
    : public AnimatedContainerView,
      public AssistantSuggestionsModelObserver,
      public AssistantUiModelObserver {
  METADATA_HEADER(SuggestionContainerView, AnimatedContainerView)

 public:
  using AssistantSuggestion = assistant::AssistantSuggestion;

  explicit SuggestionContainerView(AssistantViewDelegate* delegate);
  SuggestionContainerView(const SuggestionContainerView&) = delete;
  SuggestionContainerView& operator=(const SuggestionContainerView&) = delete;
  ~SuggestionContainerView() override;

  // AnimatedContainerView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnContentsPreferredSizeChanged(views::View* content_view) override;
  void OnAssistantControllerDestroying() override;
  void OnCommittedQueryChanged(const AssistantQuery& query) override;

  // AssistantSuggestionsModelObserver:
  void OnConversationStartersChanged(
      const std::vector<AssistantSuggestion>& conversation_starters) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      std::optional<AssistantEntryPoint> entry_point,
      std::optional<AssistantExitPoint> exit_point) override;

  void InitializeUIForBubbleView();

  // The suggestion chip that was pressed by the user. May be |nullptr|.
  const SuggestionChipView* selected_chip() const { return selected_chip_; }

 private:
  void InitLayout();

  // AnimatedContainerView:
  std::unique_ptr<ElementAnimator> HandleSuggestion(
      const AssistantSuggestion& suggestion) override;
  void OnAllViewsRemoved() override;

  std::unique_ptr<ElementAnimator> AddSuggestionChip(
      const AssistantSuggestion& suggestion);

  void OnButtonPressed(SuggestionChipView* chip_view);

  raw_ptr<views::BoxLayout> layout_manager_;  // Owned by view hierarchy.

  // Whether or not we have committed a query during this Assistant session.
  bool has_committed_query_ = false;

  // The suggestion chip that was pressed by the user. May be |nullptr|.
  raw_ptr<const SuggestionChipView> selected_chip_ = nullptr;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CONTAINER_VIEW_H_
