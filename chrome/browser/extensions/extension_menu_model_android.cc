// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_menu_model_android.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/context_menu_helpers.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "third_party/blink/public/common/context_menu_data/context_menu_data.h"
#include "third_party/blink/public/common/context_menu_data/menu_item_info.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-shared.h"
#include "ui/gfx/text_elider.h"

using blink::mojom::ContextMenuDataMediaType;

namespace extensions {

ExtensionMenuModel::ExtensionMenuModel(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params)
    : ui::SimpleMenuModel(this),
      browser_context_(render_frame_host.GetBrowserContext()),
      web_contents_(
          content::WebContents::FromRenderFrameHost(&render_frame_host)),
      rfh_ptr_(&render_frame_host),
      params_(params),
      matcher_(browser_context_,
               this,  // Delegate for SimpleMenuModel
               this,  // The model to populate
               base::BindRepeating(context_menu_helpers::MenuItemMatchesParams,
                                   params_)) {  // Filter
  CHECK(browser_context_);
  CHECK(web_contents_);
}

ExtensionMenuModel::~ExtensionMenuModel() = default;

void ExtensionMenuModel::PopulateModel() {
  context_menu_helpers::PopulateExtensionItems(browser_context_, params_,
                                               matcher_);
}

bool ExtensionMenuModel::IsCommandIdChecked(int command_id) const {
  return matcher_.IsCommandIdChecked(command_id);
}

bool ExtensionMenuModel::IsCommandIdEnabled(int command_id) const {
  return matcher_.IsCommandIdEnabled(command_id);
}

bool ExtensionMenuModel::IsCommandIdVisible(int command_id) const {
  return matcher_.IsCommandIdVisible(command_id);
}

void ExtensionMenuModel::ExecuteCommand(int command_id, int event_flags) {
  // Ensure rfh_ptr_ is valid before use.
  if (rfh_ptr_ && rfh_ptr_->IsRenderFrameLive()) {
    matcher_.ExecuteCommand(command_id, web_contents_, rfh_ptr_, params_);
  }
}

}  // namespace extensions
