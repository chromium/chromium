// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_kiosk_input/enterprise_kiosk_input_api.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/api/enterprise_kiosk_input.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace {

namespace SetCurrentInputMethod =
    ::extensions::api::enterprise_kiosk_input::SetCurrentInputMethod;

constexpr char kErrorMessageTemplate[] =
    "Could not change current input method. Invalid input method id: %s.";

}  // namespace

namespace extensions {

EnterpriseKioskInputSetCurrentInputMethodFunction::
    EnterpriseKioskInputSetCurrentInputMethodFunction() = default;

EnterpriseKioskInputSetCurrentInputMethodFunction::
    ~EnterpriseKioskInputSetCurrentInputMethodFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseKioskInputSetCurrentInputMethodFunction::Run() {
  std::optional<SetCurrentInputMethod::Params> params =
      SetCurrentInputMethod::Params::Create(args());
  const std::string input_method_id = params->options.input_method_id;

  auto& input_method_manager =
      CHECK_DEREF(ash::input_method::InputMethodManager::Get());
  ash::input_method::InputMethodManager::State& ime_state =
      CHECK_DEREF(input_method_manager.GetActiveIMEState().get());

  const std::string migrated_input_method_id =
      input_method_manager.GetMigratedInputMethodID(input_method_id);

  const bool is_input_method_enabled = base::Contains(
      ime_state.GetEnabledInputMethodIds(), migrated_input_method_id);
  if (!is_input_method_enabled) {
    return RespondNow(Error(
        base::StringPrintf(kErrorMessageTemplate, input_method_id.c_str())));
  }

  ime_state.ChangeInputMethod(migrated_input_method_id,
                              /*show_message=*/false);
  return RespondNow(NoArguments());
}

}  // namespace extensions
