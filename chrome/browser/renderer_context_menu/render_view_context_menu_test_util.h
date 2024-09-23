// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_

#include <stddef.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/compose/chrome_compose_client.h"
#endif

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
      const GURL& frame_url,
      const GURL& link_url = GURL(),
      bool is_subframe = false);

  static std::unique_ptr<TestRenderViewContextMenu> Create(
      content::RenderFrameHost* render_frame_host,
      const GURL& frame_url,
      const GURL& link_url = GURL(),
      bool is_subframe = false);

  // Returns true if the command specified by |command_id| is present
  // in the menu.
  // A list of command ids can be found in chrome/app/chrome_command_ids.h.
  bool IsItemPresent(int command_id) const;

  // Returns true if the command specified by |command_id| is checked in the
  // menu.
  bool IsItemChecked(int command_id) const;

  // Returns true if the command specified by |command_id| is enabled in the
  // menu.
  bool IsItemEnabled(int command_id) const;

  // Returns true if a command specified by any command id between
  // |command_id_first| and |command_id_last| (inclusive) is present in the
  // menu.
  bool IsItemInRangePresent(int command_id_first, int command_id_last) const;

  // Searches for an menu item with |command_id|. If it's found, the return
  // value is true and the model and index where it appears in that model are
  // returned in |found_model| and |found_index|. Otherwise returns false.
  bool GetMenuModelAndItemIndex(int command_id,
                                raw_ptr<ui::MenuModel>* found_model,
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

  // RenderViewContextMenu:
  void Show() override;
#if BUILDFLAG(IS_CHROMEOS)
  const policy::DlpRulesManager* GetDlpRulesManager() const override;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  void set_dlp_rules_manager(policy::DlpRulesManager* dlp_rules_manager);
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
  void SetChromeComposeClient(ChromeComposeClient* compose_client);
#endif
  // If `browser` is not null, sets it as the return value of GetBrowser(),
  // overriding the base class behavior. If the Browser object is destroyed
  // before this class is, then SetBrowser(nullptr) should be called. If
  // `browser` is null, restores the base class behavior of GetBrowser().
  void SetBrowser(Browser* browser);

 protected:
  // RenderViewContextMenu:
  Browser* GetBrowser() const override;

#if BUILDFLAG(ENABLE_COMPOSE)
  ChromeComposeClient* GetChromeComposeClient() const override;
#endif

 private:
  raw_ptr<Browser> browser_ = nullptr;

#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<policy::DlpRulesManager> dlp_rules_manager_ = nullptr;
#endif

#if BUILDFLAG(ENABLE_COMPOSE)
  raw_ptr<ChromeComposeClient> compose_client_ = nullptr;
#endif
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_TEST_UTIL_H_
