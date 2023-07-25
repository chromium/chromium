// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_TEXTFIELD_H_
#define ASH_STYLE_SYSTEM_TEXTFIELD_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

// SystemTextfield is an extension of `Views::Textfield` used for system UIs. It
// has specific small, medium, and large types and applies dynamic colors.
class ASH_EXPORT SystemTextfield : public views::Textfield {
 public:
  METADATA_HEADER(SystemTextfield);

  enum class Type {
    kSmall,
    kMedium,
    kLarge,
  };

  explicit SystemTextfield(Type type);
  SystemTextfield(const SystemTextfield&) = delete;
  SystemTextfield& operator=(const SystemTextfield&) = delete;
  ~SystemTextfield() override;

  // Set custom colors of text, selected text, selection background, and
  // textfield background color.
  void SetTextColorId(ui::ColorId color_id);
  void SetSelectedTextColorId(ui::ColorId color_id);
  void SetSelectionBackgroundColorId(ui::ColorId color_id);
  void SetBackgroundColorId(ui::ColorId color_id);

  // Activates or deactivates the textfield. The textfield can only be edited if
  // it is active.
  void SetActive(bool active);
  bool IsActive() const;
  // Sets if the focus ring should be shown despite the active state.
  void SetShowFocusRing(bool show);
  // Sets if the themed rounded rect background should be shown.
  // TODO(zxdan): Move this to be determined by textfield controller.
  void SetShowBackground(bool show);
  // Restores to previous text when the changes are discarded.
  void RestoreText();
  // Enables/disables background color.
  // With disabled state the background will be transparent.
  void SetBackgroundColorEnabled(bool enabled);

  // views::Textfield:
  gfx::Size CalculatePreferredSize() const override;
  void SetBorder(std::unique_ptr<views::Border> b) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;
  void OnFocus() override;
  void OnBlur() override;

 private:
  // Called when the enabled state is changed.
  void OnEnabledStateChanged();
  // Update custom color ID.
  void UpdateColorId(absl::optional<ui::ColorId>& src,
                     ui::ColorId dst,
                     bool is_background_color);
  // Updates text and selection text colors.
  void UpdateTextColor();
  // Creates themed or transparent background according to the textfield states.
  void UpdateBackground();

  Type type_;
  // Text content to restore when changes are discarded.
  std::u16string restored_text_content_;
  // Indicates if the textfield should show focus ring.
  bool show_focus_ring_ = false;
  // Indicates if the textfield should show background.
  bool show_background_ = false;
  // Indicates if the textfield background coloring is enabled.
  bool is_background_color_enabled_ = true;

  // custom color IDs for text, selected text, selection background, and
  // textfield background.
  absl::optional<ui::ColorId> text_color_id_;
  absl::optional<ui::ColorId> selected_text_color_id_;
  absl::optional<ui::ColorId> selection_background_color_id_;
  absl::optional<ui::ColorId> background_color_id_;

  // Enabled state changed callback.
  base::CallbackListSubscription enabled_changed_subscription_;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_TEXTFIELD_H_
