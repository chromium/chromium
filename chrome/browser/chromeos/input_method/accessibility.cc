// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/accessibility.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "ui/base/ime/chromeos/input_method_util.h"

namespace chromeos {
namespace input_method {

Accessibility::Accessibility(InputMethodManager* imm)
    : imm_(imm) {
  DCHECK(imm_);
  imm_->AddObserver(this);
}

Accessibility::~Accessibility() {
  DCHECK(imm_);
  imm_->RemoveObserver(this);
}

void Accessibility::InputMethodChanged(InputMethodManager* imm,
                                       Profile* profile,
                                       bool show_message) {
  DCHECK_EQ(imm, imm_);
  if (!show_message)
    return;

  // Get the medium name of the changed input method (e.g. US, INTL, etc.)
  const InputMethodDescriptor descriptor =
      imm_->GetActiveIMEState()->GetCurrentInputMethod();
  const std::string medium_name = base::UTF16ToUTF8(
      imm_->GetInputMethodUtil()->GetInputMethodMediumName(descriptor));

  AutomationManagerAura::GetInstance()->HandleAlert(medium_name);
}

}  // namespace input_method
}  // namespace chromeos
