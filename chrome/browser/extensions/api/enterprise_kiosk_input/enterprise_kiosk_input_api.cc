// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_kiosk_input/enterprise_kiosk_input_api.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/api/enterprise_kiosk_input.h"
#include "chromeos/crosapi/mojom/input_methods.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/input_methods_ash.h"
#include "ui/base/ime/ash/input_method_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

namespace SetCurrentInputMethod =
    ::extensions::api::enterprise_kiosk_input::SetCurrentInputMethod;

constexpr char kErrorMessageTemplate[] =
    "Could not change current input method. Invalid input method id: %s.";

crosapi::mojom::InputMethods* GetInputMethodsApi() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::InputMethods>()
      .get();
#else
  return crosapi::CrosapiManager::Get()->crosapi_ash()->input_methods_ash();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::InputMethods>()) {
    // Lacros should work with ash-chrome 2 versions below where the
    // InputMethods crosapi is not available yet.
    // TODO(b/337793096): Remove this check.
    LOG(ERROR) << "InputMethods crosapi is not available in ash-chrome";
    return RespondNow(NoArguments());
  }
#endif

  GetInputMethodsApi()->ChangeInputMethod(
      params->options.input_method_id,
      base::BindOnce(&EnterpriseKioskInputSetCurrentInputMethodFunction::
                         OnChangeInputMethodDone,
                     this, params->options.input_method_id));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void EnterpriseKioskInputSetCurrentInputMethodFunction::OnChangeInputMethodDone(
    std::string input_method_id,
    bool succeeded) {
  if (succeeded) {
    Respond(NoArguments());
  } else {
    Respond(Error(
        base::StringPrintf(kErrorMessageTemplate, input_method_id.c_str())));
  }
}

}  // namespace extensions
