// Copyright 2014 The Chromium Authors
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
      browser::OpenTab::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  extensions::ExtensionTabUtil::OpenTabParams options;
  options.create_browser_if_needed = true;
  options.url = params->options.url;

  auto result =
      extensions::ExtensionTabUtil::OpenTab(this, options, user_gesture());
  if (!result.has_value())
    return RespondNow(Error(result.error()));

  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace chrome_apps
