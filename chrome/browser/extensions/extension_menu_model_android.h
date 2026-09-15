// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MODEL_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MODEL_ANDROID_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
// items, supporting both render frame context menus (e.g. text selection,
// links) and tab strip context menus.
class ExtensionMenuModel : public ui::SimpleMenuModel,
                           public ui::SimpleMenuModel::Delegate {
 public:
  // Constructs an ExtensionMenuModel for a render frame context menu.
  ExtensionMenuModel(content::RenderFrameHost& render_frame_host,
                     const content::ContextMenuParams& params);

  // Constructs an ExtensionMenuModel for a tab strip context menu.
  ExtensionMenuModel(content::BrowserContext* browser_context,
                     content::WebContents* web_contents);

  ExtensionMenuModel(const ExtensionMenuModel&) = delete;
  ExtensionMenuModel& operator=(const ExtensionMenuModel&) = delete;

  ~ExtensionMenuModel() override;

  // Populates the internal SimpleMenuModel with relevant extension items.
  // This should be called after construction.
  void PopulateModel();

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Returns true if any menu item in this model is visible.
  bool HasVisibleItems() const;

  ContextMenuMatcher& matcher_for_test() { return matcher_; }

 private:
  friend class TestExtensionMenuModel;

  enum class ContextType {
    kRenderFrame,
    kTab,
  };

  ContextType context_type_;
  raw_ptr<content::BrowserContext> browser_context_;
  base::WeakPtr<content::WebContents> web_contents_;

  // Store the RenderFrameHost; assumes this delegate is relatively short-lived
  // or the menu is used while the RFH is valid. Only used for kRenderFrame.
  raw_ptr<content::RenderFrameHost> rfh_ptr_{nullptr};

  // `params_` are expected to be present when this is bridging a renderer
  // context menu, but may be missing for the tab strip context menu.
  std::optional<content::ContextMenuParams> params_;

  ContextMenuMatcher matcher_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_MODEL_ANDROID_H_
