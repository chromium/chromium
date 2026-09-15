// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_menu_model_android.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/context_menu_helpers.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"

namespace extensions {

ExtensionMenuModel::ExtensionMenuModel(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params)
    : ui::SimpleMenuModel(this),
      context_type_(ContextType::kRenderFrame),
      browser_context_(render_frame_host.GetBrowserContext()),
      web_contents_(
          content::WebContents::FromRenderFrameHost(&render_frame_host)
              ->GetWeakPtr()),
      rfh_ptr_(&render_frame_host),
      params_(params),
      matcher_(browser_context_,
               this,  // Delegate for SimpleMenuModel
               this,  // The model to populate
               base::BindRepeating(context_menu_helpers::MenuItemMatchesParams,
                                   params)) {  // Filter
  CHECK(browser_context_);
  CHECK(web_contents_);
}

ExtensionMenuModel::ExtensionMenuModel(content::BrowserContext* browser_context,
                                       content::WebContents* web_contents)
    : ui::SimpleMenuModel(this),
      context_type_(ContextType::kTab),
      browser_context_(browser_context),
      web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr),
      matcher_(browser_context,
               this,
               this,
               base::BindRepeating([](const MenuItem* item) {
                 return item->contexts().Contains(MenuItem::TAB);
               })) {
  CHECK(browser_context_);
}

ExtensionMenuModel::~ExtensionMenuModel() = default;

void ExtensionMenuModel::PopulateModel() {
  switch (context_type_) {
    case ContextType::kRenderFrame:
      CHECK(params_.has_value());
      // Note: Do not call Clear() here, as subclasses (such as
      // ChromeSelectionDropdownMenuModel) may have already added items to the
      // model before populating extension items.
      context_menu_helpers::PopulateExtensionItems(browser_context_, *params_,
                                                   matcher_);
      break;
    case ContextType::kTab: {
      // Clear out any potentially-stale state. Not all contexts reuse models,
      // but this ensures that any that do have a clean slate. Unlike in the
      // kRenderFrame case, the model for tabs is used compositively, so this
      // doesn't have risk of clearing out unrelated menu items.
      Clear();
      matcher_.Clear();
      if (!base::FeatureList::IsEnabled(
              extensions_features::kExtensionTabContextMenu)) {
        return;
      }
      int extension_index = 0;
      auto* menu_manager = MenuManager::Get(browser_context_);
      if (!menu_manager) {
        return;
      }
      for (const auto& key : menu_manager->ExtensionIds()) {
        matcher_.AppendExtensionItems(key, std::u16string(), &extension_index,
                                      /*is_action_menu=*/false);
      }
      break;
    }
  }
}

bool ExtensionMenuModel::HasVisibleItems() const {
  return matcher_.HasVisibleItems(const_cast<ExtensionMenuModel*>(this));
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
  switch (context_type_) {
    case ContextType::kRenderFrame:
      // Ensure rfh_ptr_ is valid before use.
      if (rfh_ptr_ && rfh_ptr_->IsRenderFrameLive()) {
        CHECK(web_contents_);
        CHECK(params_.has_value());
        matcher_.ExecuteCommand(command_id, web_contents_.get(), rfh_ptr_,
                                *params_);
      }
      break;
    case ContextType::kTab:
      if (web_contents_) {
        content::ContextMenuParams params;
        params.page_url = web_contents_->GetLastCommittedURL();
        matcher_.ExecuteCommand(command_id, web_contents_.get(), nullptr,
                                params);
      }
      break;
  }
}

}  // namespace extensions
