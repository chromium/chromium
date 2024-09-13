// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/info_private/info_private_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/extension_function.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/extension_info_private_ash.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/extension_info_private.mojom.h"  // nogncheck
#include "chromeos/lacros/lacros_service.h"
#endif

namespace {

// Key which corresponds to the timezone property in JS.
const char kPropertyTimezone[] = "timezone";

// Property not found error message.
const char kPropertyNotFound[] = "Property '*' does not exist.";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
mojo::Remote<crosapi::mojom::ExtensionInfoPrivate>*
GetExtensionInfoPrivateRemote() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::ExtensionInfoPrivate>()) {
    return &(lacros_service->GetRemote<crosapi::mojom::ExtensionInfoPrivate>());
  }
  return nullptr;
}
#endif

}  // namespace

namespace extensions {

ChromeosInfoPrivateGetFunction::ChromeosInfoPrivateGetFunction() {}

ChromeosInfoPrivateGetFunction::~ChromeosInfoPrivateGetFunction() {}

ExtensionFunction::ResponseAction ChromeosInfoPrivateGetFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(!args().empty() && args()[0].is_list());
  const base::Value::List& list = args()[0].GetList();

  std::vector<std::string> property_names;
  for (const auto& property : list) {
    EXTENSION_FUNCTION_VALIDATE(property.is_string());
    std::string property_name = property.GetString();
    property_names.push_back(std::move(property_name));
  }
  auto callback =
      base::BindOnce(&ChromeosInfoPrivateGetFunction::RespondWithResult, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->extension_info_private_ash()
      ->GetSystemProperties(std::move(property_names), std::move(callback));
#else
  auto* remote = GetExtensionInfoPrivateRemote();
  if (!remote) {
    return RespondNow(Error("ExtensionInfoPrivate unavailable."));
  }
  (*remote)->GetSystemProperties(std::move(property_names),
                                 std::move(callback));
#endif
  return RespondLater();
}

void ChromeosInfoPrivateGetFunction::RespondWithResult(base::Value result) {
  Respond(WithArguments(std::move(result)));
}

ChromeosInfoPrivateSetFunction::ChromeosInfoPrivateSetFunction() {}

ChromeosInfoPrivateSetFunction::~ChromeosInfoPrivateSetFunction() {}

ExtensionFunction::ResponseAction ChromeosInfoPrivateSetFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  param_name_ = args()[0].GetString();

  if (param_name_ == kPropertyTimezone) {
    EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
    EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());
    const std::string& param_value = args()[1].GetString();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->extension_info_private_ash()
        ->SetTimezone(param_value);
#else
    auto* remote = GetExtensionInfoPrivateRemote();
    if (!remote) {
      return RespondNow(Error("ExtensionInfoPrivate unavailable."));
    }
    (*remote)->SetTimezone(param_value);
#endif
    return RespondNow(NoArguments());
  }

  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_bool());
  bool param_value = args()[1].GetBool();

  auto callback =
      base::BindOnce(&ChromeosInfoPrivateSetFunction::RespondWithResult, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->extension_info_private_ash()
      ->SetBool(param_name_, param_value, std::move(callback));
#else
  auto* remote = GetExtensionInfoPrivateRemote();
  if (!remote) {
    return RespondNow(Error("ExtensionInfoPrivate unavailable."));
  }
  (*remote)->SetBool(param_name_, param_value, std::move(callback));
#endif

  return RespondLater();
}

void ChromeosInfoPrivateSetFunction::RespondWithResult(bool found) {
  if (found) {
    Respond(NoArguments());
  } else {
    Respond(Error(kPropertyNotFound, param_name_));
  }
}

ChromeosInfoPrivateIsTabletModeEnabledFunction::
    ChromeosInfoPrivateIsTabletModeEnabledFunction() {}

ChromeosInfoPrivateIsTabletModeEnabledFunction::
    ~ChromeosInfoPrivateIsTabletModeEnabledFunction() {}

ExtensionFunction::ResponseAction
ChromeosInfoPrivateIsTabletModeEnabledFunction::Run() {
  auto callback = base::BindOnce(
      &ChromeosInfoPrivateIsTabletModeEnabledFunction::RespondWithResult, this);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->extension_info_private_ash()
      ->IsTabletModeEnabled(std::move(callback));
#else
  auto* remote = GetExtensionInfoPrivateRemote();
  if (!remote) {
    return RespondNow(Error("ExtensionInfoPrivate unavailable."));
  }
  (*remote)->IsTabletModeEnabled(std::move(callback));
#endif
  return RespondLater();
}

void ChromeosInfoPrivateIsTabletModeEnabledFunction::RespondWithResult(
    bool enabled) {
  Respond(WithArguments(enabled));
}

ChromeosInfoPrivateIsRunningOnLacrosFunction::
    ChromeosInfoPrivateIsRunningOnLacrosFunction() {}

ChromeosInfoPrivateIsRunningOnLacrosFunction::
    ~ChromeosInfoPrivateIsRunningOnLacrosFunction() {}

ExtensionFunction::ResponseAction
ChromeosInfoPrivateIsRunningOnLacrosFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return RespondNow(WithArguments(false));
#else
  return RespondNow(WithArguments(true));
#endif
}

}  // namespace extensions
