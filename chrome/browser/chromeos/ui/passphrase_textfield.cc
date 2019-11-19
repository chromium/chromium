// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/passphrase_textfield.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"

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

void PassphraseTextfield::SetFakePassphrase() {
  static base::NoDestructor<base::string16> fake_passphrase(
      base::ASCIIToUTF16("********"));
  SetText(*fake_passphrase);
  changed_ = false;
}

void PassphraseTextfield::ClearFakePassphrase() {
  SetText(base::string16());
  changed_ = true;
}

}  // namespace chromeos
