// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SETTINGS_ENTRY_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SETTINGS_ENTRY_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageView;
class Label;
class ToggleButton;
}  // namespace views

namespace ash {

// A view that is part of the Settings Bar view, from which the user can toggle
// each of the settings on/off.
class ASH_EXPORT CaptureModeSettingsEntryView
    : public views::View,
      public CaptureModeSessionFocusCycler::HighlightableView {
 public:
  METADATA_HEADER(CaptureModeSettingsEntryView);

  CaptureModeSettingsEntryView(views::Button::PressedCallback callback,
                               const gfx::VectorIcon& icon,
                               int string_id);
  CaptureModeSettingsEntryView(const CaptureModeSettingsEntryView&) = delete;
  CaptureModeSettingsEntryView& operator=(const CaptureModeSettingsEntryView&) =
      delete;
  ~CaptureModeSettingsEntryView() override;

  views::ToggleButton* toggle_button_view() const {
    return toggle_button_view_;
  }

  void SetIcon(const gfx::VectorIcon& icon);

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override;

 private:
  // Owned by the views hierarchy.
  views::ImageView* icon_view_ = nullptr;
  views::Label* text_view_ = nullptr;
  views::ToggleButton* toggle_button_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SETTINGS_ENTRY_VIEW_H_
