// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/browser/browser_api.h"

#include <string>

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/apps/platform_apps/api/browser.h"

namespace chrome_apps {
namespace api {

BrowserOpenTabFunction::~BrowserOpenTabFunction() {}

ExtensionFunction::ResponseAction BrowserOpenTabFunction::Run() {
  std::optional<browser::OpenTab::Params> params(
      browser::OpenTab::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  extensions::ExtensionTabUtil::OpenTabParams options;
  options.create_browser_if_needed = true;
  options.url = params->options.url;

  const auto result =
      extensions::ExtensionTabUtil::OpenTab(this, options, user_gesture());
  return RespondNow(result.has_value() ? NoArguments() : Error(result.error()));
}

}  // namespace api
}  // namespace chrome_apps
