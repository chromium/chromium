// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_delegate_impl.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace input_method {

InputMethodDelegateImpl::InputMethodDelegateImpl() = default;

InputMethodDelegateImpl::~InputMethodDelegateImpl() = default;

std::string InputMethodDelegateImpl::GetHardwareKeyboardLayouts() const {
  if (!g_browser_process) {
    return "";
  }

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    return "";
  }

  return local_state->GetString(prefs::kHardwareKeyboardLayout);
}

std::u16string InputMethodDelegateImpl::GetLocalizedString(
    int resource_id) const {
  return l10n_util::GetStringUTF16(resource_id);
}

void InputMethodDelegateImpl::SetHardwareKeyboardLayoutForTesting(
    const std::string& layout) {
  NOTREACHED_IN_MIGRATION()
      << "Use FakeInputMethodDelegate for hardware keyboard layout "
      << "testing purpose.";
}

}  // namespace input_method
}  // namespace ash
