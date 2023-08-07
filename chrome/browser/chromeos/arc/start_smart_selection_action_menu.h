// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_START_SMART_SELECTION_ACTION_MENU_H_
#define CHROME_BROWSER_CHROMEOS_ARC_START_SMART_SELECTION_ACTION_MENU_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/common/intent_helper/arc_intent_helper_mojo_delegate.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

class RenderViewContextMenuProxy;

namespace content {
class BrowserContext;
}

namespace arc {

// An observer class which populates the menu item that shows an action
// obtained from a text selection.
class StartSmartSelectionActionMenu : public RenderViewContextMenuObserver {
 public:
  explicit StartSmartSelectionActionMenu(
      content::BrowserContext* context,
      RenderViewContextMenuProxy* proxy,
      std::unique_ptr<ArcIntentHelperMojoDelegate> delegate);
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
  void OnContextMenuShown(const content::ContextMenuParams& params,
                          const gfx::Rect& bounds_in_screen) override;

 private:
  void HandleTextSelectionActions(
      std::vector<ArcIntentHelperMojoDelegate::TextSelectionAction> actions);

  raw_ptr<content::BrowserContext> context_;
  const raw_ptr<RenderViewContextMenuProxy>
      proxy_;  // Owned by RenderViewContextMenu.

  // The text selection actions passed from ARC.
  std::vector<ArcIntentHelperMojoDelegate::TextSelectionAction> actions_;

  // The delegate instance to handle TextSelectionActions.
  std::unique_ptr<ArcIntentHelperMojoDelegate> delegate_;

  base::WeakPtrFactory<StartSmartSelectionActionMenu> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_START_SMART_SELECTION_ACTION_MENU_H_
