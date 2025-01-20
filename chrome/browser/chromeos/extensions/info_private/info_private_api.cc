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
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/extension_info_private_ash.h"
#include "extensions/browser/extension_function.h"

namespace {

// Key which corresponds to the timezone property in JS.
const char kPropertyTimezone[] = "timezone";

// Property not found error message.
const char kPropertyNotFound[] = "Property '*' does not exist.";

}  // namespace

namespace extensions {

ChromeosInfoPrivateGetFunction::ChromeosInfoPrivateGetFunction() = default;

ChromeosInfoPrivateGetFunction::~ChromeosInfoPrivateGetFunction() = default;

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
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->extension_info_private_ash()
      ->GetSystemProperties(std::move(property_names), std::move(callback));
  return RespondLater();
}

void ChromeosInfoPrivateGetFunction::RespondWithResult(base::Value result) {
  Respond(WithArguments(std::move(result)));
}

ChromeosInfoPrivateSetFunction::ChromeosInfoPrivateSetFunction() = default;

ChromeosInfoPrivateSetFunction::~ChromeosInfoPrivateSetFunction() = default;

ExtensionFunction::ResponseAction ChromeosInfoPrivateSetFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  param_name_ = args()[0].GetString();

  if (param_name_ == kPropertyTimezone) {
    EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
    EXTENSION_FUNCTION_VALIDATE(args()[1].is_string());
    const std::string& param_value = args()[1].GetString();
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->extension_info_private_ash()
        ->SetTimezone(param_value);
    return RespondNow(NoArguments());
  }

  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_bool());
  bool param_value = args()[1].GetBool();

  auto callback =
      base::BindOnce(&ChromeosInfoPrivateSetFunction::RespondWithResult, this);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->extension_info_private_ash()
      ->SetBool(param_name_, param_value, std::move(callback));

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
    ChromeosInfoPrivateIsTabletModeEnabledFunction() = default;

ChromeosInfoPrivateIsTabletModeEnabledFunction::
    ~ChromeosInfoPrivateIsTabletModeEnabledFunction() = default;

ExtensionFunction::ResponseAction
ChromeosInfoPrivateIsTabletModeEnabledFunction::Run() {
  auto callback = base::BindOnce(
      &ChromeosInfoPrivateIsTabletModeEnabledFunction::RespondWithResult, this);
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->extension_info_private_ash()
      ->IsTabletModeEnabled(std::move(callback));
  return RespondLater();
}

void ChromeosInfoPrivateIsTabletModeEnabledFunction::RespondWithResult(
    bool enabled) {
  Respond(WithArguments(enabled));
}

ChromeosInfoPrivateIsRunningOnLacrosFunction::
    ChromeosInfoPrivateIsRunningOnLacrosFunction() = default;

ChromeosInfoPrivateIsRunningOnLacrosFunction::
    ~ChromeosInfoPrivateIsRunningOnLacrosFunction() = default;

ExtensionFunction::ResponseAction
ChromeosInfoPrivateIsRunningOnLacrosFunction::Run() {
  return RespondNow(WithArguments(false));
}

}  // namespace extensions
