// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_OPEN_WITH_MENU_H_
#define CHROME_BROWSER_CHROMEOS_ARC_OPEN_WITH_MENU_H_

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/arc/common/intent_helper/link_handler_model.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class RenderViewContextMenuProxy;

namespace content {
class BrowserContext;
}

namespace arc {

// An observer class which populates the "Open with <app>" menu items either
// synchronously or asynchronously.
class OpenWithMenu : public RenderViewContextMenuObserver,
                     public LinkHandlerModel::Observer {
 public:
  using HandlerMap = std::unordered_map<int, LinkHandlerInfo>;

  class SubMenuDelegate : public ui::SimpleMenuModel::Delegate {
   public:
    explicit SubMenuDelegate(OpenWithMenu* parent) : parent_(parent) {}
    SubMenuDelegate(const SubMenuDelegate&) = delete;
    SubMenuDelegate& operator=(const SubMenuDelegate&) = delete;
    ~SubMenuDelegate() override = default;

    bool IsCommandIdChecked(int command_id) const override;
    bool IsCommandIdEnabled(int command_id) const override;
    void ExecuteCommand(int command_id, int event_flags) override;

   private:
    const raw_ptr<OpenWithMenu> parent_;
  };

  OpenWithMenu(content::BrowserContext* context,
               RenderViewContextMenuProxy* proxy);
  OpenWithMenu(const OpenWithMenu&) = delete;
  OpenWithMenu& operator=(const OpenWithMenu&) = delete;
  ~OpenWithMenu() override;

  // RenderViewContextMenuObserver overrides:
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

  // LinkHandlerModel::Observer overrides:
  void ModelChanged(const std::vector<LinkHandlerInfo>& handlers) override;

  static void AddPlaceholderItemsForTesting(RenderViewContextMenuProxy* proxy,
                                            ui::SimpleMenuModel* submenu);
  static std::pair<HandlerMap, int> BuildHandlersMapForTesting(
      const std::vector<LinkHandlerInfo>& handlers);

 private:
  // Adds placeholder items and the `submenu` to the `proxy`.
  static void AddPlaceholderItems(RenderViewContextMenuProxy* proxy,
                                  ui::SimpleMenuModel* submenu);

  // Converts `handlers` into HandlerMap which is a map from a command ID to a
  // LinkHandlerInfo and returns the map. Also returns a command id for the
  // parent of the submenu. When the submenu is not needed, the function
  // returns `kInvalidCommandId`.
  static std::pair<OpenWithMenu::HandlerMap, int> BuildHandlersMap(
      const std::vector<LinkHandlerInfo>& handlers);

  static const int kNumMainMenuCommands;
  static const int kNumSubMenuCommands;

  raw_ptr<content::BrowserContext> context_;
  const raw_ptr<RenderViewContextMenuProxy> proxy_;
  SubMenuDelegate submenu_delegate_{this};
  const std::u16string more_apps_label_;

  // A menu model received from Ash side.
  std::unique_ptr<LinkHandlerModel> menu_model_;
  OpenWithMenu::HandlerMap handlers_;
  // A submenu passed to Chrome side.
  std::unique_ptr<ui::MenuModel> submenu_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_OPEN_WITH_MENU_H_
