// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INTENT_HELPER_START_SMART_SELECTION_ACTION_MENU_H_
#define CHROME_BROWSER_ASH_ARC_INTENT_HELPER_START_SMART_SELECTION_ACTION_MENU_H_

#include <memory>
#include <vector>

#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

class RenderViewContextMenuProxy;

namespace arc {

// An observer class which populates the menu item that shows an action
// obtained from a text selection.
class StartSmartSelectionActionMenu : public RenderViewContextMenuObserver {
 public:
  explicit StartSmartSelectionActionMenu(RenderViewContextMenuProxy* proxy);
  StartSmartSelectionActionMenu(const StartSmartSelectionActionMenu&) = delete;
  StartSmartSelectionActionMenu& operator=(
      const StartSmartSelectionActionMenu&) = delete;
  ~StartSmartSelectionActionMenu() override;

  // RenderViewContextMenuObserver overrides:
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  void HandleTextSelectionActions(
      std::vector<mojom::TextSelectionActionPtr> actions);

  void UpdateMenuIcon(int command_id, mojom::ActivityIconPtr icon);

  void SetMenuIcon(int command_id, const gfx::ImageSkia& image);

  RenderViewContextMenuProxy* const proxy_;  // Owned by RenderViewContextMenu.

  // The text selection actions passed from ARC.
  std::vector<mojom::TextSelectionActionPtr> actions_;

  base::WeakPtrFactory<StartSmartSelectionActionMenu> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INTENT_HELPER_START_SMART_SELECTION_ACTION_MENU_H_
