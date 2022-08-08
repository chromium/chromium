// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/context_menu_matcher.h"
#endif

namespace content {
class WebContents;
}
namespace ui {
class MenuModel;
}
#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace policy {
class DlpRulesManager;
}
#endif

class TestRenderViewContextMenu : public RenderViewContextMenu {
 public:
  TestRenderViewContextMenu(content::RenderFrameHost& render_frame_host,
                            content::ContextMenuParams params);

  TestRenderViewContextMenu(const TestRenderViewContextMenu&) = delete;
  TestRenderViewContextMenu& operator=(const TestRenderViewContextMenu&) =
      delete;

  ~TestRenderViewContextMenu() override;

  // Factory.
  // This is a lightweight method to create a test RenderViewContextMenu
  // instance.
  // Use the constructor if you want to create menu with fine-grained params.
  static std::unique_ptr<TestRenderViewContextMenu> Create(
      content::WebContents* web_contents,
      const GURL& page_url,
      const GURL& link_url,
      const GURL& frame_url);

  // Returns true if the command specified by |command_id| is present
  // in the menu.
  // A list of command ids can be found in chrome/app/chrome_command_ids.h.
  bool IsItemPresent(int command_id) const;

  // Returns true if the command specified by |command_id| is checked in the
  // menu.
  bool IsItemChecked(int command_id) const;

  // Returns true if a command specified by any command id between
  // |command_id_first| and |command_id_last| (inclusive) is present in the
  // menu.
  bool IsItemInRangePresent(int command_id_first, int command_id_last) const;

  // Searches for an menu item with |command_id|. If it's found, the return
  // value is true and the model and index where it appears in that model are
  // returned in |found_model| and |found_index|. Otherwise returns false.
  bool GetMenuModelAndItemIndex(int command_id,
                                ui::MenuModel** found_model,
                                size_t* found_index);

  // Returns the command id of the menu item with the specified |path|.
  int GetCommandIDByProfilePath(const base::FilePath& path) const;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ContextMenuMatcher& extension_items() { return extension_items_; }
#endif

  void set_protocol_handler_registry(
      custom_handlers::ProtocolHandlerRegistry* registry) {
    protocol_handler_registry_ = registry;
  }

  void set_selection_navigation_url(GURL url) {
    selection_navigation_url_ = url;
  }

  using RenderViewContextMenu::AppendImageItems;

  void Show() override;

#if BUILDFLAG(IS_CHROMEOS)
  const policy::DlpRulesManager* GetDlpRulesManager() const override;

  void set_dlp_rules_manager(policy::DlpRulesManager* dlp_rules_manager);
#endif

 private:
#if BUILDFLAG(IS_CHROMEOS)
  policy::DlpRulesManager* dlp_rules_manager_ = nullptr;
#endif
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_
