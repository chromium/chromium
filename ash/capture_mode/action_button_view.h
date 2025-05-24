// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_ACTION_BUTTON_VIEW_H_
#define ASH_CAPTURE_MODE_ACTION_BUTTON_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class BoxLayout;
class ImageView;
class InkDropContainerView;
class Label;
}  // namespace views

namespace ash {

class SystemShadow;

// A view that displays an action button. The action button may show both an
// icon and text or be collapsed into just an icon.
class ASH_EXPORT ActionButtonView : public views::Button {
  METADATA_HEADER(ActionButtonView, views::Button)

 public:
  ActionButtonView(views::Button::PressedCallback callback,
                   std::u16string text,
                   const gfx::VectorIcon* icon,
                   ActionButtonRank rank);
  ActionButtonView(const ActionButtonView&) = delete;
  ActionButtonView& operator=(const ActionButtonView&) = delete;
  ~ActionButtonView() override;

  ActionButtonRank rank() const { return rank_; }

  // views::Button:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override;
  void RemoveLayerFromRegions(ui::Layer* layer) override;

  // Collapses the action button, hiding its label so that only the icon
  // shows.
  void CollapseToIconButton();

  // Fades in the action button from fully transparent to fully opaque.
  void PerformFadeInAnimation(base::TimeDelta fade_in_duration);

  const views::ImageView* image_view_for_testing() const { return image_view_; }
  const views::Label* label_for_testing() const { return label_; }

 private:
  // Rank used to determine ordering of action buttons.
  const ActionButtonRank rank_;

  std::unique_ptr<SystemShadow> shadow_;

  raw_ptr<views::BoxLayout> box_layout_ = nullptr;

  // The image view for the action button icon. May be `nullptr` if the action
  // button does not have an icon.
  raw_ptr<views::ImageView> image_view_ = nullptr;

  // The label containing the action button text. This label is hidden when the
  // action button is collapsed.
  raw_ptr<views::Label> label_ = nullptr;

  // Ensures the ink drop is painted above the button's background.
  raw_ptr<views::InkDropContainerView> ink_drop_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_ACTION_BUTTON_VIEW_H_
