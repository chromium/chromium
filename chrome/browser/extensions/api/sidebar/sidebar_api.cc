// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sidebar/sidebar_api.h"

#include "base/notreached.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/common/extensions/api/side_panel.h"
#include "chrome/common/extensions/api/sidebar.h"

namespace extensions {

namespace {

// Converts a `sidePanel` side to the matching `sidebar` side. The two schemas
// define their own `Side` enums so that `sidebar` does not depend on
// `sidePanel` types.
api::sidebar::Side ToSidebarSide(api::side_panel::Side side) {
  switch (side) {
    case api::side_panel::Side::kLeft:
      return api::sidebar::Side::kLeft;
    case api::side_panel::Side::kRight:
      return api::sidebar::Side::kRight;
    case api::side_panel::Side::kNone:
      break;
  }
  NOTREACHED();
}

}  // namespace

SidebarGetLayoutFunction::SidebarGetLayoutFunction() = default;
SidebarGetLayoutFunction::~SidebarGetLayoutFunction() = default;

ExtensionFunction::ResponseAction SidebarGetLayoutFunction::Run() {
  // Only available to extensions.
  EXTENSION_FUNCTION_VALIDATE(extension());
  api::sidebar::PanelLayout layout;
  layout.side = ToSidebarSide(
      SidePanelService::Get(browser_context())->GetSidePanelLayout().side);
  return RespondNow(
      ArgumentList(api::sidebar::GetLayout::Results::Create(layout)));
}

}  // namespace extensions
