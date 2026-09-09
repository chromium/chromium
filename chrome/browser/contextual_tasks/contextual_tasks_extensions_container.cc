// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extensions_container.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace contextual_tasks {

ContextualTasksExtensionsContainer::ContextualTasksExtensionsContainer(
    BrowserWindowInterface* browser,
    content::WebContents* web_contents)
    : browser_(browser),
      web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr),
      extensions_menu_coordinator_(
          base::FeatureList::IsEnabled(
              extensions_features::kExtensionsMenuAccessControl)
              ? std::make_unique<ExtensionsMenuCoordinator>(browser, this)
              : nullptr) {}

ContextualTasksExtensionsContainer::~ContextualTasksExtensionsContainer() =
    default;

void ContextualTasksExtensionsContainer::ShowExtensionsMenu(
    views::BubbleAnchor anchor) {
  anchor_ = anchor;
  if (extensions_menu_coordinator_ &&
      extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
    return;
  }
  if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
    return;
  }

  if (extensions_menu_coordinator_) {
    extensions_menu_coordinator_->Show(anchor, this);
  } else {
    ExtensionsMenuView::ShowBubble(anchor, browser_, this, this);
  }
}

bool ContextualTasksExtensionsContainer::IsExtensionsMenuShowing() const {
  if (extensions_menu_coordinator_) {
    return extensions_menu_coordinator_->IsShowing();
  }
  return ExtensionsMenuView::IsShowing();
}

void ContextualTasksExtensionsContainer::SetWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents ? web_contents->GetWeakPtr() : nullptr;
}

ToolbarActionViewModel* ContextualTasksExtensionsContainer::GetActionForId(
    const std::string& action_id) {
  return nullptr;
}

void ContextualTasksExtensionsContainer::HideActivePopup() {
  if (popup_owner_) {
    popup_owner_->HidePopup();
  }
}

void ContextualTasksExtensionsContainer::CloseExtensionsMenuIfOpen() {
  if (extensions_menu_coordinator_ &&
      extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
  } else if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
  }
}

bool ContextualTasksExtensionsContainer::ShowToolbarActionPopupForAPICall(
    const std::string& action_id,
    ShowPopupCallback callback) {
  return false;
}

void ContextualTasksExtensionsContainer::ToggleExtensionsMenu() {
  ShowExtensionsMenu(anchor_);
}

bool ContextualTasksExtensionsContainer::HasAnyExtensions() const {
  return false;
}

content::WebContents* ContextualTasksExtensionsContainer::GetActiveWebContents()
    const {
  return web_contents_.get();
}

bool ContextualTasksExtensionsContainer::IsVisible() const {
  return true;
}

std::optional<extensions::ExtensionId>
ContextualTasksExtensionsContainer::GetPoppedOutActionId() const {
  return std::nullopt;
}

bool ContextualTasksExtensionsContainer::IsActionVisibleOnToolbar(
    const std::string& action_id) const {
  return false;
}

void ContextualTasksExtensionsContainer::UndoPopOut() {}

void ContextualTasksExtensionsContainer::SetPopupOwner(
    ToolbarActionViewModel* popup_owner) {
  popup_owner_ = popup_owner;
}

void ContextualTasksExtensionsContainer::PopOutAction(
    const extensions::ExtensionId& action_id,
    base::OnceClosure closure) {
  if (closure) {
    std::move(closure).Run();
  }
}

void ContextualTasksExtensionsContainer::ShowWidgetForExtension(
    views::Widget* widget,
    const std::string& extension_id) {
  if (widget) {
    widget->Show();
  }
}

void ContextualTasksExtensionsContainer::CollapseConfirmation() {}

void ContextualTasksExtensionsContainer::ShowContextMenuAsFallback(
    const extensions::ExtensionId& action_id) {}

void ContextualTasksExtensionsContainer::OnPopupShown(
    const extensions::ExtensionId& action_id,
    bool by_user) {}

void ContextualTasksExtensionsContainer::OnPopupClosed(
    const extensions::ExtensionId& action_id) {}

views::FocusManager*
ContextualTasksExtensionsContainer::GetFocusManagerForAccelerator() {
  if (browser_) {
    if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser_)) {
      return browser_view->GetFocusManager();
    }
  }
  return nullptr;
}

views::BubbleAnchor
ContextualTasksExtensionsContainer::GetReferenceButtonForPopup(
    const extensions::ExtensionId& action_id) {
  return anchor_;
}

views::BubbleAnchor
ContextualTasksExtensionsContainer::GetExtensionsButtonAnchor() {
  return anchor_;
}

views::BubbleBorder::Arrow ContextualTasksExtensionsContainer::GetPopupArrow()
    const {
  return views::BubbleBorder::TOP_LEFT;
}

}  // namespace contextual_tasks
