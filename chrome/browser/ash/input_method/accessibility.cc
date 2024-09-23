// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/accessibility.h"

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash {
namespace input_method {

Accessibility::Accessibility(InputMethodManager* imm) {
  DCHECK(imm);
  observed_input_method_manager_.Observe(imm);
}

Accessibility::~Accessibility() = default;

void Accessibility::InputMethodChanged(InputMethodManager* imm,
                                       Profile* profile,
                                       bool show_message) {
  if (!show_message) {
    return;
  }

  // Get the localized display name of the changed input method.
  const InputMethodDescriptor descriptor =
      imm->GetActiveIMEState()->GetCurrentInputMethod();
  const std::string medium_name =
      imm->GetInputMethodUtil()->GetLocalizedDisplayName(descriptor);

  AutomationManagerAura::GetInstance()->HandleAlert(medium_name);
}

}  // namespace input_method
}  // namespace ash
