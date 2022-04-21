// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/info_private_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/extension_info_private_ash.h"
#include "extensions/browser/extension_function.h"

namespace {

// Key which corresponds to the timezone property in JS.
const char kPropertyTimezone[] = "timezone";

// Property not found error message.
const char kPropertyNotFound[] = "Property '*' does not exist.";
}

namespace extensions {

ChromeosInfoPrivateGetFunction::ChromeosInfoPrivateGetFunction() {
}

ChromeosInfoPrivateGetFunction::~ChromeosInfoPrivateGetFunction() {
}

ExtensionFunction::ResponseAction ChromeosInfoPrivateGetFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(!args().empty() && args()[0].is_list());
  base::Value::ConstListView list = args()[0].GetListDeprecated();

  base::Value result(base::Value::Type::DICTIONARY);
  std::vector<std::string> property_names;
  for (size_t i = 0; i < list.size(); ++i) {
    EXTENSION_FUNCTION_VALIDATE(list[i].is_string());
    std::string property_name = list[i].GetString();
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
  Respond(OneArgument(std::move(result)));
}

ChromeosInfoPrivateSetFunction::ChromeosInfoPrivateSetFunction() {
}

ChromeosInfoPrivateSetFunction::~ChromeosInfoPrivateSetFunction() {
}

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
    ChromeosInfoPrivateIsTabletModeEnabledFunction() {}

ChromeosInfoPrivateIsTabletModeEnabledFunction::
    ~ChromeosInfoPrivateIsTabletModeEnabledFunction() {}

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
  Respond(OneArgument(base::Value(enabled)));
}

}  // namespace extensions
