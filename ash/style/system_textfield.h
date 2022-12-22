// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_TEXTFIELD_H_
#define ASH_STYLE_SYSTEM_TEXTFIELD_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

class SystemTextfieldController;

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

  // The delegate that handles the textfield behaviors on focused and blurred.
  class Delegate {
   public:
    virtual void OnTextfieldFocused(SystemTextfield* textfield) = 0;
    virtual void OnTextfieldBlurred(SystemTextfield* textfield) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  explicit SystemTextfield(Type type);
  SystemTextfield(const SystemTextfield&) = delete;
  SystemTextfield& operator=(const SystemTextfield&) = delete;
  ~SystemTextfield() override;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Activates or deactivates the textfield. The method is mainly used by
  // `SystemTextfieldController`.
  void SetActive(bool active);
  bool active() const { return active_; }
  // Restores to previous text when the changes are discarded.
  void RestoreText();

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
  // Updates text and selection text colors.
  void UpdateTextColor();
  // Creates themed or transparent background according to the textfield states.
  void UpdateBackground();

  Type type_;
  bool active_ = false;
  // Text content to restore when changes are discarded.
  std::u16string restored_text_content_;
  Delegate* delegate_ = nullptr;
  // Enabled state changed callback.
  base::CallbackListSubscription enabled_changed_subscription_;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_TEXTFIELD_H_
