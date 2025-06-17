// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_DELEGATE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/menus/simple_menu_model.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace extensions {
// Delegate for a SimpleMenuModel that only contains extension context menu
// items.
class ExtensionMenuDelegate : public ui::SimpleMenuModel::Delegate {
 public:
  ExtensionMenuDelegate(content::RenderFrameHost& render_frame_host,
                        const content::ContextMenuParams& params);

  ExtensionMenuDelegate(const ExtensionMenuDelegate&) = delete;
  ExtensionMenuDelegate& operator=(const ExtensionMenuDelegate&) = delete;

  ~ExtensionMenuDelegate() override;

  // Populates the internal SimpleMenuModel with relevant extension items.
  // This should be called after construction.
  void PopulateModel();

  // Returns the SimpleMenuModel populated with extension items.
  // The model is owned by this delegate.
  ui::SimpleMenuModel* GetModel();

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<content::WebContents> web_contents_;
  // Store the RenderFrameHost; assumes this delegate is relatively short-lived
  // or the menu is used while the RFH is valid.
  raw_ptr<content::RenderFrameHost> rfh_ptr_;
  content::ContextMenuParams params_;
  ui::SimpleMenuModel model_;
  ContextMenuMatcher matcher_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_DELEGATE_ANDROID_H_
