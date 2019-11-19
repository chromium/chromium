// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/login_screen_ui_api.h"

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/login_screen_extension_ui_handler.h"
#include "chrome/common/extensions/api/login_screen_ui.h"

namespace login_screen_ui = extensions::api::login_screen_ui;

namespace extensions {

LoginScreenUiShowFunction::LoginScreenUiShowFunction() = default;
LoginScreenUiShowFunction::~LoginScreenUiShowFunction() = default;

ExtensionFunction::ResponseAction LoginScreenUiShowFunction::Run() {
  std::unique_ptr<login_screen_ui::Show::Params> parameters =
      login_screen_ui::Show::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters);

  const login_screen_ui::ShowOptions& options = parameters->options;
  bool user_can_close =
      options.user_can_close ? *options.user_can_close : false;

  std::string error;
  bool success =
      chromeos::LoginScreenExtensionUiHandler::Get(true /*can_create*/)
          ->Show(extension(), options.url, user_can_close, &error);

  if (!success)
    return RespondNow(Error(error));
  return RespondNow(NoArguments());
}

LoginScreenUiCloseFunction::LoginScreenUiCloseFunction() = default;
LoginScreenUiCloseFunction::~LoginScreenUiCloseFunction() = default;

ExtensionFunction::ResponseAction LoginScreenUiCloseFunction::Run() {
  std::string error;
  bool success =
      chromeos::LoginScreenExtensionUiHandler::Get(true /*can_create*/)
          ->Close(extension(), &error);

  if (!success)
    return RespondNow(Error(error));
  return RespondNow(NoArguments());
}

}  // namespace extensions
