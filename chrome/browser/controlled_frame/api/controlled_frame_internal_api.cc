// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/api/controlled_frame_internal_api.h"

#include <memory>

#include "chrome/browser/controlled_frame/controlled_frame_menu_icon_loader.h"
#include "chrome/browser/extensions/context_menu_helpers.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/common/extensions/api/chrome_web_view_internal.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace webview = extensions::api::chrome_web_view_internal;

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
          render_frame_host()->GetProcess()->GetID(),
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

}  // namespace controlled_frame
