// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSIONS_CONTAINER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSIONS_CONTAINER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/views/extensions/extensions_container_views.h"
#include "ui/views/bubble/bubble_anchor.h"

class BrowserWindowInterface;
class ExtensionsMenuCoordinator;
class ToolbarActionViewModel;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

// A container for managing extensions UI in Contextual Tasks side panel.
// Implements ExtensionsContainer and ExtensionsContainerViews so that the
// standard desktop extensions menu can be scoped to the side panel's
// WebContents and anchored to the side panel toolbar.
class ContextualTasksExtensionsContainer : public ExtensionsContainer,
                                           public ExtensionsContainerViews {
 public:
  // Returns the anchor that the extensions menu and extension popups should be
  // attached to, or a null anchor if no suitable anchor currently exists.
  //
  // This is resolved on demand rather than cached because the anchor may be a
  // `ui::TrackedElement` owned by the side panel WebUI document, which is
  // destroyed whenever that document goes away (side panel close, navigation,
  // renderer crash or browser window teardown).
  using AnchorProvider = base::RepeatingCallback<views::BubbleAnchor()>;

  ContextualTasksExtensionsContainer(BrowserWindowInterface* browser,
                                     content::WebContents* web_contents,
                                     AnchorProvider anchor_provider);
  ContextualTasksExtensionsContainer(
      const ContextualTasksExtensionsContainer&) = delete;
  ContextualTasksExtensionsContainer& operator=(
      const ContextualTasksExtensionsContainer&) = delete;
  virtual ~ContextualTasksExtensionsContainer();

  // Shows the extensions menu, anchored to the anchor returned by the
  // container's `AnchorProvider`. Does nothing if there is no valid anchor.
  void ShowExtensionsMenu();

  // Returns whether the extensions menu is currently showing.
  bool IsExtensionsMenuShowing() const;

  // Updates the WebContents associated with this container.
  void SetWebContents(content::WebContents* web_contents);

  // ExtensionsContainer:
  ToolbarActionViewModel* GetActionForId(const std::string& action_id) override;
  void HideActivePopup() override;
  void CloseExtensionsMenuIfOpen() override;
  bool ShowToolbarActionPopupForAPICall(const std::string& action_id,
                                        ShowPopupCallback callback) override;
  void ToggleExtensionsMenu() override;
  bool HasAnyExtensions() const override;
  content::WebContents* GetActiveWebContents() const override;

  // ExtensionsContainerViews:
  bool IsVisible() const override;
  std::optional<extensions::ExtensionId> GetPoppedOutActionId() const override;
  bool IsActionVisibleOnToolbar(const std::string& action_id) const override;
  void UndoPopOut() override;
  void SetPopupOwner(ToolbarActionViewModel* popup_owner) override;
  void PopOutAction(const extensions::ExtensionId& action_id,
                    base::OnceClosure closure) override;
  void ShowWidgetForExtension(views::Widget* widget,
                              const std::string& extension_id) override;
  void CollapseConfirmation() override;
  void ShowContextMenuAsFallback(
      const extensions::ExtensionId& action_id) override;
  void OnPopupShown(const extensions::ExtensionId& action_id,
                    bool by_user) override;
  void OnPopupClosed(const extensions::ExtensionId& action_id) override;
  views::FocusManager* GetFocusManagerForAccelerator() override;
  views::BubbleAnchor GetReferenceButtonForPopup(
      const extensions::ExtensionId& action_id) override;
  views::BubbleAnchor GetExtensionsButtonAnchor() override;
  views::BubbleBorder::Arrow GetPopupArrow() const override;

 private:
  // Runs `anchor_provider_`, returning a null anchor if no provider was
  // supplied. The result must never be stored: see `AnchorProvider`.
  views::BubbleAnchor GetAnchor() const;

  const raw_ptr<BrowserWindowInterface> browser_;
  base::WeakPtr<content::WebContents> web_contents_;
  const AnchorProvider anchor_provider_;
  raw_ptr<ToolbarActionViewModel> popup_owner_ = nullptr;
  std::unique_ptr<ExtensionsMenuCoordinator> extensions_menu_coordinator_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_EXTENSIONS_CONTAINER_H_
