// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/browser/browser_api.h"

#include <string>

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/open_tab_helper.h"
#include "chrome/common/apps/platform_apps/api/browser.h"

namespace chrome_apps {
namespace api {

BrowserOpenTabFunction::~BrowserOpenTabFunction() = default;

ExtensionFunction::ResponseAction BrowserOpenTabFunction::Run() {
  std::optional<browser::OpenTab::Params> params(
      browser::OpenTab::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.has_value());

  extensions::OpenTabHelper::Params options;
  options.create_browser_if_needed = true;

  base::expected<GURL, std::string> maybe_url =
      extensions::ExtensionTabUtil::PrepareURLForNavigation(
          params->options.url, extension(), browser_context());
  if (!maybe_url.has_value()) {
    return RespondNow(Error(maybe_url.error()));
  }
  GURL validated_url = std::move(maybe_url.value());

  const auto result = extensions::OpenTabHelper::OpenTab(
      validated_url, this, options, user_gesture());
  return RespondNow(result.has_value() ? NoArguments() : Error(result.error()));
}

}  // namespace api
}  // namespace chrome_apps
