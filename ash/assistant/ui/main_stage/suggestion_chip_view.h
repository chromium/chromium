// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_SUGGESTION_CHIP_VIEW_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/metadata_header_macros.h"

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
  using AssistantSuggestion = chromeos::assistant::AssistantSuggestion;

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
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  void SetIcon(const gfx::ImageSkia& icon);
  const gfx::ImageSkia& GetIcon() const;

  void SetText(const base::string16& text);
  const base::string16& GetText() const;

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
