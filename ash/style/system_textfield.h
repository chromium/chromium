// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_TEXTFIELD_H_
#define ASH_STYLE_SYSTEM_TEXTFIELD_H_

#include <optional>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

// SystemTextfield is an extension of `Views::Textfield` used for system UIs. It
// has specific small, medium, and large types and applies dynamic colors.
class ASH_EXPORT SystemTextfield : public views::Textfield {
  METADATA_HEADER(SystemTextfield, views::Textfield)

 public:
  enum class Type {
    kSmall,
    kMedium,
    kLarge,
  };

  enum class BackgroundMode {
    kOnHoverOrFocus,  // Default: Only show background when hovered or focused.
    kAlways,          // Always show background.
    kNever,           // Never show background.
  };

  explicit SystemTextfield(Type type);
  SystemTextfield(const SystemTextfield&) = delete;
  SystemTextfield& operator=(const SystemTextfield&) = delete;
  ~SystemTextfield() override;

  void SetActiveStateChangedCallback(base::RepeatingClosure callback);

  // Activates or deactivates the textfield. The textfield can only be edited if
  // it is active.
  void SetActive(bool active);
  bool IsActive() const;
  // Sets if the focus ring should be shown despite the active state.
  void SetShowFocusRing(bool show);
  // Sets the background mode. Callers should use this method instead of
  // `SetBackgroundEnabled()` to configure whether and when the background
  // is displayed.
  void SetBackgroundMode(BackgroundMode mode);
  BackgroundMode GetBackgroundMode() const { return background_mode_; }
  // Restores to previous text when the changes are discarded.
  void RestoreText();
  // Creates themed or transparent background according to the textfield states.
  void UpdateBackground();

  // views::Textfield:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void SetBorder(std::unique_ptr<views::Border> b) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;

 private:
  // An event handler to handle the events outside the textfield.
  class EventHandler;

  // Called when the enabled state is changed.
  void OnEnabledStateChanged();

  Type type_;
  std::unique_ptr<EventHandler> event_handler_;

  // Text content to restore when changes are discarded.
  std::u16string restored_text_content_;
  // Indicates if the textfield should show focus ring.
  bool show_focus_ring_ = false;
  // Controls when the background is painted.
  BackgroundMode background_mode_ = BackgroundMode::kOnHoverOrFocus;

  // Active state changed callback.
  base::RepeatingClosure active_state_changed_callback_;

  // Enabled state changed callback.
  base::CallbackListSubscription enabled_changed_subscription_;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_TEXTFIELD_H_
