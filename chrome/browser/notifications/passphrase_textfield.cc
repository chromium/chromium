// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/passphrase_textfield.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace chromeos {

PassphraseTextfield::PassphraseTextfield()
    : Textfield(), show_fake_(false), changed_(true) {
  SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
}

void PassphraseTextfield::SetShowFake(bool show_fake) {
  show_fake_ = show_fake;
  if (show_fake_)
    SetFakePassphrase();
  else
    ClearFakePassphrase();
}

bool PassphraseTextfield::GetShowFake() const {
  return show_fake_;
}

void PassphraseTextfield::OnFocus() {
  // If showing the fake password, then clear it when focused.
  if (show_fake_ && !changed_)
    ClearFakePassphrase();
  Textfield::OnFocus();
}

void PassphraseTextfield::OnBlur() {
  // If password is not changed, then show the fake password when blurred.
  if (show_fake_ && GetText().empty())
    SetFakePassphrase();
  Textfield::OnBlur();
}

std::string PassphraseTextfield::GetPassphrase() {
  return changed_ ? base::UTF16ToUTF8(GetText()) : std::string();
}

bool PassphraseTextfield::GetChanged() const {
  return changed_;
}

void PassphraseTextfield::SetFakePassphrase() {
  static base::NoDestructor<std::u16string> fake_passphrase(u"********");
  SetText(*fake_passphrase);
  changed_ = false;
}

void PassphraseTextfield::ClearFakePassphrase() {
  SetText(std::u16string());
  changed_ = true;
}

BEGIN_METADATA(PassphraseTextfield)
ADD_PROPERTY_METADATA(bool, ShowFake)
ADD_READONLY_PROPERTY_METADATA(bool, Changed)
END_METADATA

}  // namespace chromeos
