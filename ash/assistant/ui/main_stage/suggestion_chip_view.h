// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_

#include "base/component_export.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class BoxLayout;
class ImageView;
class Label;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// View representing a suggestion chip.
class COMPONENT_EXPORT(ASSISTANT_UI) SuggestionChipView : public views::Button {
 public:
  using AssistantSuggestion = assistant::AssistantSuggestion;

  METADATA_HEADER(SuggestionChipView);

  SuggestionChipView(AssistantViewDelegate* delegate,
                     const AssistantSuggestion& suggestion);
  SuggestionChipView(const SuggestionChipView&) = delete;
  SuggestionChipView& operator=(const SuggestionChipView&) = delete;
  ~SuggestionChipView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildVisibilityChanged(views::View* child) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnThemeChanged() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  void SetIcon(const gfx::ImageSkia& icon);
  gfx::ImageSkia GetIcon() const;

  void SetText(const std::u16string& text);
  const std::u16string& GetText() const;

  const base::UnguessableToken& suggestion_id() const { return suggestion_id_; }

 private:
  void InitLayout(const AssistantSuggestion& suggestion);

  AssistantViewDelegate* const delegate_;

  const base::UnguessableToken suggestion_id_;

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.
  views::ImageView* icon_view_;       // Owned by view hierarchy.
  views::Label* text_view_;           // Owned by view hierarchy.

  base::WeakPtrFactory<SuggestionChipView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_
