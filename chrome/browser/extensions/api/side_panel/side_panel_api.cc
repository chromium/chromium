// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/side_panel/side_panel_api.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "extensions/common/extension_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
namespace {

bool IsSidePanelApiAvailable() {
  return base::FeatureList::IsEnabled(
      extensions_features::kExtensionSidePanelIntegration);
}

}  // namespace

SidePanelApiFunction::SidePanelApiFunction() = default;
SidePanelApiFunction::~SidePanelApiFunction() = default;
SidePanelService* SidePanelApiFunction::GetService() {
  return extensions::SidePanelService::Get(browser_context());
}

ExtensionFunction::ResponseAction SidePanelApiFunction::Run() {
  if (!IsSidePanelApiAvailable())
    return RespondNow(Error("API Unavailable"));
  return RunFunction();
}

ExtensionFunction::ResponseAction SidePanelGetOptionsFunction::RunFunction() {
  absl::optional<api::side_panel::GetOptions::Params> params =
      api::side_panel::GetOptions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto tab_id = params->options.tab_id
                    ? absl::optional<int>(*(params->options.tab_id))
                    : absl::nullopt;
  const api::side_panel::PanelOptions& options =
      GetService()->GetOptions(*extension(), tab_id);
  return RespondNow(WithArguments(options.ToValue()));
}

ExtensionFunction::ResponseAction SidePanelSetOptionsFunction::RunFunction() {
  absl::optional<api::side_panel::SetOptions::Params> params =
      api::side_panel::SetOptions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // TODO(crbug.com/1328645): Validate the relative extension path exists.
  GetService()->SetOptions(*extension(), std::move(params->options));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
SidePanelSetPanelBehaviorFunction::RunFunction() {
  absl::optional<api::side_panel::SetPanelBehavior::Params> params =
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
  // This API is only available with the corresponding feature enabled.
  EXTENSION_FUNCTION_VALIDATE(
      base::FeatureList::IsEnabled(extensions_features::kApiSidePanelOpen));

  // `sidePanel.open()` requires a user gesture.
  if (!user_gesture()) {
    return RespondNow(
        Error("`sidePanel.open()` may only be called in "
              "response to a user gesture."));
  }

  absl::optional<api::side_panel::Open::Params> params =
      api::side_panel::Open::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->options.tab_id;
  Browser* browser = nullptr;
  if (!ExtensionTabUtil::GetTabById(tab_id, browser_context(),
                                    include_incognito_information(), &browser,
                                    nullptr, nullptr, nullptr)) {
    return RespondNow(
        Error(base::StringPrintf("No tab with tabId: %d", tab_id)));
  }
  CHECK(browser);

  SidePanelService* service = GetService();
  api::side_panel::PanelOptions panel_options =
      service->GetOptions(*extension(), tab_id);
  if (!panel_options.path || !panel_options.enabled.has_value() ||
      !(*panel_options.enabled)) {
    return RespondNow(Error(base::StringPrintf(
        "No active side panel for tabId: %d", params->options.tab_id)));
  }

  // TODO(https://crbug.com/1446022): This doesn't work for opening a contextual
  // entry on the non-active tab.
  service->OpenSidePanel(*extension(), *browser);

  // TODO(https://crbug.com/1446022): Should we wait for the side panel to be
  // created and load? That would probably be nice.

  return RespondNow(NoArguments());
}

}  // namespace extensions
