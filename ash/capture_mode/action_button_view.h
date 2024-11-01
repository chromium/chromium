// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_ACTION_BUTTON_VIEW_H_
#define ASH_CAPTURE_MODE_ACTION_BUTTON_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/style/pill_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

class SystemShadow;

// A view that displays an action button. The action button may show both an
// icon and text or be collapsed into just an icon.
// TODO(crbug.com/374356291): Implement functionality to collapse the action
// button.
class ASH_EXPORT ActionButtonView : public PillButton {
  METADATA_HEADER(ActionButtonView, PillButton)

 public:
  ActionButtonView(views::Button::PressedCallback callback,
                   std::u16string text,
                   const gfx::VectorIcon* icon,
                   ActionButtonRank rank);
  ActionButtonView(const ActionButtonView&) = delete;
  ActionButtonView& operator=(const ActionButtonView&) = delete;
  ~ActionButtonView() override;

  ActionButtonRank rank() const { return rank_; }

  // PillButton:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

 private:
  // Rank used to determine ordering of action buttons.
  const ActionButtonRank rank_;

  std::unique_ptr<SystemShadow> shadow_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_ACTION_BUTTON_VIEW_H_
