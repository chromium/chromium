// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_SUGGESTION_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_SUGGESTION_VIEW_H_

#include "base/component_export.h"
#include "base/unguessable_token.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class InkDropContainerView;
class Label;
}  // namespace views

namespace ash {

namespace assistant {
struct AssistantSuggestion;
}

class AssistantViewDelegate;

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantOnboardingSuggestionView
    : public views::Button {
 public:
  METADATA_HEADER(AssistantOnboardingSuggestionView);

  AssistantOnboardingSuggestionView(
      AssistantViewDelegate* delegate,
      const assistant::AssistantSuggestion& suggestion,
      int index);

  AssistantOnboardingSuggestionView(const AssistantOnboardingSuggestionView&) =
      delete;
  AssistantOnboardingSuggestionView& operator=(
      const AssistantOnboardingSuggestionView&) = delete;
  ~AssistantOnboardingSuggestionView() override;

  // views::View:
  int GetHeightForWidth(int width) const override;
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnThemeChanged() override;

  // Returns the icon for the suggestion.
  gfx::ImageSkia GetIcon() const;

  // Returns the text for the suggestion.
  const std::u16string& GetText() const;

 private:
  void InitLayout(const assistant::AssistantSuggestion& suggestion);
  void UpdateIcon(const gfx::ImageSkia& icon);

  void OnButtonPressed();

  AssistantViewDelegate* const delegate_;  // Owned by AssistantController.
  const base::UnguessableToken suggestion_id_;
  const int index_;
  GURL url_;

  // Owned by view hierarchy.
  views::ImageView* icon_ = nullptr;
  views::Label* label_ = nullptr;
  views::InkDropContainerView* ink_drop_container_ = nullptr;

  base::WeakPtrFactory<AssistantOnboardingSuggestionView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_ONBOARDING_SUGGESTION_VIEW_H_
