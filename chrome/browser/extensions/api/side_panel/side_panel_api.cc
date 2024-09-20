// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/side_panel/side_panel_api.h"

#include <optional>

#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/common/extensions/api/side_panel.h"

namespace extensions {

SidePanelApiFunction::SidePanelApiFunction() = default;
SidePanelApiFunction::~SidePanelApiFunction() = default;
SidePanelService* SidePanelApiFunction::GetService() {
  return extensions::SidePanelService::Get(browser_context());
}

ExtensionFunction::ResponseAction SidePanelApiFunction::Run() {
  return RunFunction();
}

ExtensionFunction::ResponseAction SidePanelGetOptionsFunction::RunFunction() {
  std::optional<api::side_panel::GetOptions::Params> params =
      api::side_panel::GetOptions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto tab_id = params->options.tab_id
                    ? std::optional<int>(*(params->options.tab_id))
                    : std::nullopt;
  const api::side_panel::PanelOptions& options =
      GetService()->GetOptions(*extension(), tab_id);
  return RespondNow(WithArguments(options.ToValue()));
}

ExtensionFunction::ResponseAction SidePanelSetOptionsFunction::RunFunction() {
  std::optional<api::side_panel::SetOptions::Params> params =
      api::side_panel::SetOptions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // TODO(crbug.com/40226489): Validate the relative extension path exists.
  GetService()->SetOptions(*extension(), std::move(params->options));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SidePanelSetPanelBehaviorFunction::RunFunction() {
  std::optional<api::side_panel::SetPanelBehavior::Params> params =
      api::side_panel::SetPanelBehavior::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->behavior.open_panel_on_action_click.has_value()) {
    GetService()->SetOpenSidePanelOnIconClick(
        extension()->id(), *params->behavior.open_panel_on_action_click);
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SidePanelGetPanelBehaviorFunction::RunFunction() {
  api::side_panel::PanelBehavior behavior;
  behavior.open_panel_on_action_click =
      GetService()->OpenSidePanelOnIconClick(extension()->id());

  return RespondNow(WithArguments(behavior.ToValue()));
}

ExtensionFunction::ResponseAction SidePanelOpenFunction::RunFunction() {
  // Only available to extensions.
  EXTENSION_FUNCTION_VALIDATE(extension());

  // `sidePanel.open()` requires a user gesture.
  if (!user_gesture()) {
    return RespondNow(
        Error("`sidePanel.open()` may only be called in "
              "response to a user gesture."));
  }

  std::optional<api::side_panel::Open::Params> params =
      api::side_panel::Open::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->options.tab_id && !params->options.window_id) {
    return RespondNow(
        Error("At least one of `tabId` and `windowId` must be provided"));
  }

  SidePanelService* service = GetService();
  base::expected<bool, std::string> open_panel_result;
  if (params->options.tab_id) {
    open_panel_result = service->OpenSidePanelForTab(
        *extension(), browser_context(), *params->options.tab_id,
        params->options.window_id, include_incognito_information());
  } else {
    CHECK(params->options.window_id);
    open_panel_result = service->OpenSidePanelForWindow(
        *extension(), browser_context(), *params->options.window_id,
        include_incognito_information());
  }

  if (!open_panel_result.has_value()) {
    return RespondNow(Error(std::move(open_panel_result.error())));
  }

  CHECK_EQ(true, open_panel_result.value());

  // TODO(crbug.com/40064601): Should we wait for the side panel to be
  // created and load? That would probably be nice.

  return RespondNow(NoArguments());
}

}  // namespace extensions
