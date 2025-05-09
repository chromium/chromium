// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/api/controlled_frame_internal_api.h"

#include <memory>

#include "chrome/browser/controlled_frame/controlled_frame_menu_icon_loader.h"
#include "chrome/browser/extensions/context_menu_helpers.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/common/controlled_frame/api/controlled_frame_internal.h"
#include "chrome/common/extensions/api/chrome_web_view_internal.h"
#include "components/guest_view/browser/guest_view.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace webview = extensions::api::chrome_web_view_internal;
namespace controlled_frame_internal =
    controlled_frame::api::controlled_frame_internal;

namespace controlled_frame {

ExtensionFunction::ResponseAction
ControlledFrameInternalContextMenusCreateFunction::Run() {
  CHECK(!extension())
      << "controlledFrame should only be available to Isolated Web Apps.";
  std::optional<webview::ContextMenusCreate::Params> params =
      webview::ContextMenusCreate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  extensions::MenuItem::Id id(
      Profile::FromBrowserContext(browser_context())->IsOffTheRecord(),
      extensions::MenuItem::ExtensionKey(
          /*extension_id=*/std::string(),
          render_frame_host()->GetProcess()->GetDeprecatedID(),
          render_frame_host()->GetRoutingID(), params->instance_id));

  auto* menu_manager = extensions::MenuManager::Get(browser_context());
  menu_manager->SetMenuIconLoader(
      id.extension_key, std::make_unique<ControlledFrameMenuIconLoader>());
  id.string_uid = *params->create_properties.id;

  std::string error;
  bool success = extensions::context_menu_helpers::CreateMenuItem(
      params->create_properties, Profile::FromBrowserContext(browser_context()),
      /*extension=*/nullptr, id, &error);
  return RespondNow(success ? NoArguments() : Error(error));
}

ExtensionFunction::ResponseAction
ControlledFrameInternalContextMenusUpdateFunction::Run() {
  std::optional<webview::ContextMenusUpdate::Params> params =
      webview::ContextMenusUpdate::Params::Create(args());

  extensions::MenuItem::Id id(
      browser_context()->IsOffTheRecord(),
      extensions::MenuItem::ExtensionKey(
          /*extension_id=*/std::string(),
          render_frame_host()->GetProcess()->GetDeprecatedID(),
          render_frame_host()->GetRoutingID(), params->instance_id));

  if (params->id.as_string) {
    id.string_uid = *params->id.as_string;
  } else if (params->id.as_integer) {
    id.uid = *params->id.as_integer;
  } else {
    NOTREACHED();
  }

  std::string error;
  bool success = extensions::context_menu_helpers::UpdateMenuItem(
      params->update_properties, Profile::FromBrowserContext(browser_context()),
      /*extension=*/nullptr, id, &error);
  return RespondNow(success ? NoArguments() : Error(error));
}

ExtensionFunction::ResponseAction
ControlledFrameInternalSetClientHintsEnabledFunction::Run() {
  std::optional<controlled_frame_internal::SetClientHintsEnabled::Params>
      params = controlled_frame_internal::SetClientHintsEnabled::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);

  extensions::WebViewGuest& webview = GetGuest();

  webview.SetClientHintsEnabled(params->enabled);

  return RespondNow(NoArguments());
}

bool ControlledFrameInternalSetClientHintsEnabledFunction::PreRunValidation(
    std::string* error) {
  // Controlled Frame does not have an associated extension.
  EXTENSION_FUNCTION_PRERUN_VALIDATE(!extension());
  return extensions::WebViewInternalExtensionFunction::PreRunValidation(error);
}

}  // namespace controlled_frame
