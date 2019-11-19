// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_indicator/system_indicator_api.h"

#include "base/no_destructor.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_manager_factory.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/common/extensions/api/system_indicator/system_indicator_handler.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {

// Returns true if the extension has a system indicator.
bool HasSystemIndicator(const Extension& extension) {
  return SystemIndicatorHandler::GetSystemIndicatorIcon(extension) != nullptr;
}

}  // namespace

ExtensionFunction::ResponseAction SystemIndicatorSetIconFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(extension());
  EXTENSION_FUNCTION_VALIDATE(HasSystemIndicator(*extension()));

  EXTENSION_FUNCTION_VALIDATE(args_->GetList().size() == 1);
  EXTENSION_FUNCTION_VALIDATE(args_->GetList()[0].is_dict());

  const base::Value& set_icon_details = args_->GetList()[0];

  // NOTE: For historical reasons, this code is primarily taken from
  // ExtensionActionSetIconFunction.
  // setIcon can take a variant argument: either a dictionary of canvas
  // ImageData, or an icon index.
  if (const base::Value* canvas_set = set_icon_details.FindKeyOfType(
          "imageData", base::Value::Type::DICTIONARY)) {
    gfx::ImageSkia icon;
    EXTENSION_FUNCTION_VALIDATE(ExtensionAction::ParseIconFromCanvasDictionary(
        static_cast<const base::DictionaryValue&>(*canvas_set), &icon));

    if (icon.isNull())
      return RespondNow(Error("Icon invalid."));

    SystemIndicatorManagerFactory::GetForContext(browser_context())
        ->SetSystemIndicatorDynamicIcon(*extension(), gfx::Image(icon));
  } else if (set_icon_details.FindKeyOfType("iconIndex",
                                            base::Value::Type::INTEGER)) {
    // Obsolete argument: ignore it.
    // TODO(devlin): Do we need this here? Does any systemIndicator extension
    // use it?
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SystemIndicatorEnableFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(extension());
  EXTENSION_FUNCTION_VALIDATE(HasSystemIndicator(*extension()));

  SystemIndicatorManagerFactory::GetForContext(browser_context())
      ->SetSystemIndicatorEnabled(*extension(), true);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction SystemIndicatorDisableFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(extension());
  EXTENSION_FUNCTION_VALIDATE(HasSystemIndicator(*extension()));

  SystemIndicatorManagerFactory::GetForContext(browser_context())
      ->SetSystemIndicatorEnabled(*extension(), false);
  return RespondNow(NoArguments());
}

}  // namespace extensions
