// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/browser/browser_api.h"

#include <memory>
#include <string>

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/apps/platform_apps/api/browser.h"

namespace chrome_apps {
namespace api {

BrowserOpenTabFunction::~BrowserOpenTabFunction() {}

ExtensionFunction::ResponseAction BrowserOpenTabFunction::Run() {
  std::unique_ptr<browser::OpenTab::Params> params(
      browser::OpenTab::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  extensions::ExtensionTabUtil::OpenTabParams options;
  options.create_browser_if_needed = true;
  options.url.reset(new std::string(params->options.url));

  std::string error;
  std::unique_ptr<base::DictionaryValue> result(
      extensions::ExtensionTabUtil::OpenTab(this, options, user_gesture(),
                                            &error));
  if (!result)
    return RespondNow(Error(error));

  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace chrome_apps
