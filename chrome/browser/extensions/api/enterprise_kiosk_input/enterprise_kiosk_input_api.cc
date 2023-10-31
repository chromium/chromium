// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_kiosk_input/enterprise_kiosk_input_api.h"

#include "chrome/common/extensions/api/enterprise_kiosk_input.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"

#include "ui/base/ime/ash/input_method_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

namespace SetCurrentInputMethod =
    ::extensions::api::enterprise_kiosk_input::SetCurrentInputMethod;

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kErrorMessageTemplate[] =
    "Could not change current input method. Invalid input method id: %s.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace input_method = ::ash::input_method;
using input_method::InputMethodDescriptors;
using input_method::InputMethodManager;

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

EnterpriseKioskInputSetCurrentInputMethodFunction::
    EnterpriseKioskInputSetCurrentInputMethodFunction() = default;

EnterpriseKioskInputSetCurrentInputMethodFunction::
    ~EnterpriseKioskInputSetCurrentInputMethodFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseKioskInputSetCurrentInputMethodFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  absl::optional<SetCurrentInputMethod::Params> params =
      SetCurrentInputMethod::Params::Create(args());

  const std::string& params_input_id = params->options.input_method_id;
  const std::string migrated_input_method_id =
      InputMethodManager::Get()->GetMigratedInputMethodID(params_input_id);

  scoped_refptr<InputMethodManager::State> ime_state =
      InputMethodManager::Get()->GetActiveIMEState();
  const std::vector<std::string>& enabled_input_method_ids =
      ime_state->GetEnabledInputMethodIds();
  if (auto it = base::ranges::find(enabled_input_method_ids,
                                   migrated_input_method_id);
      it != enabled_input_method_ids.end()) {
    ime_state->ChangeInputMethod(*it,
                                 /* show_message=*/false);
    return RespondNow(NoArguments());
  }
  return RespondNow(Error(
      base::StringPrintf(kErrorMessageTemplate, params_input_id.c_str())));
#else
  return RespondNow(Error("Not implemented."));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace extensions
