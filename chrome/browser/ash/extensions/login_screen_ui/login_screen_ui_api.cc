// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/login_screen_ui/login_screen_ui_api.h"

#include "chrome/browser/ash/extensions/login_screen_ui/ui_handler.h"
#include "chrome/common/extensions/api/login_screen_ui.h"

namespace login_screen_ui = extensions::api::login_screen_ui;

namespace extensions {

LoginScreenUiShowFunction::LoginScreenUiShowFunction() = default;
LoginScreenUiShowFunction::~LoginScreenUiShowFunction() = default;

ExtensionFunction::ResponseAction LoginScreenUiShowFunction::Run() {
  std::optional<login_screen_ui::Show::Params> parameters =
      login_screen_ui::Show::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  const login_screen_ui::ShowOptions& options = parameters->options;
  bool user_can_close =
      options.user_can_close ? *options.user_can_close : false;

  std::string error;
  bool success =
      chromeos::login_screen_extension_ui::UiHandler::Get(true /*can_create*/)
          ->Show(extension(), options.url, user_can_close, &error);

  if (!success)
    return RespondNow(Error(error));
  return RespondNow(NoArguments());
}

LoginScreenUiCloseFunction::LoginScreenUiCloseFunction() = default;
LoginScreenUiCloseFunction::~LoginScreenUiCloseFunction() = default;

ExtensionFunction::ResponseAction LoginScreenUiCloseFunction::Run() {
  chromeos::login_screen_extension_ui::UiHandler::Get(true /*can_create*/)
      ->Close(extension(),
              base::BindOnce(&LoginScreenUiCloseFunction::OnClosed, this));
  // UiHandler::Close() repsonds asynchronously for success, but not for error.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void LoginScreenUiCloseFunction::OnClosed(
    bool success,
    const std::optional<std::string>& error) {
  if (!success) {
    Respond(Error(error.value()));
    return;
  }
  Respond(NoArguments());
}

}  // namespace extensions
