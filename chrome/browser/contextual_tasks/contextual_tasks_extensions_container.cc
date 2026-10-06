// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extensions_container.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/extensions/extension_action_delegate_desktop.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"
#include "ui/views/widget/widget.h"

namespace contextual_tasks {

ContextualTasksExtensionsContainer::ContextualTasksExtensionsContainer(
    BrowserWindowInterface* browser,
    content::WebContents* web_contents,
    AnchorProvider anchor_provider)
    : browser_(browser),
      web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr),
      anchor_provider_(std::move(anchor_provider)),
      toolbar_model_(browser && browser->GetProfile()
                         ? ToolbarActionsModel::Get(browser->GetProfile())
                         : nullptr),
      extensions_menu_coordinator_(
          base::FeatureList::IsEnabled(
              extensions_features::kExtensionsMenuAccessControl)
              ? std::make_unique<ExtensionsMenuCoordinator>(browser, this)
              : nullptr) {
  if (toolbar_model_) {
    toolbar_model_observation_.Observe(toolbar_model_);
    CreateActions();
  }
}

ContextualTasksExtensionsContainer::~ContextualTasksExtensionsContainer() {
  CloseExtensionsMenuIfOpen();
  HideActivePopup();
  popup_owner_ = nullptr;
  for (const auto& [_, model] : actions_) {
    model->UnregisterCommand();
  }
  actions_.clear();
}

void ContextualTasksExtensionsContainer::ShowExtensionsMenu() {
  if (extensions_menu_coordinator_ &&
      extensions_menu_coordinator_->IsShowing()) {
    extensions_menu_coordinator_->Hide();
    return;
  }
  if (ExtensionsMenuView::IsShowing()) {
    ExtensionsMenuView::Hide();
    return;
  }

  // The anchor is resolved at show time; its backing element may have gone
  // away since the menu was last shown.
  views::BubbleAnchor anchor = GetAnchor();
  if (anchor.IsNull()) {
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
  auto it = actions_.find(action_id);
  return it != actions_.end() ? it->second.get() : nullptr;
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
  ShowExtensionsMenu();
}

bool ContextualTasksExtensionsContainer::HasAnyExtensions() const {
  return !actions_.empty();
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
  // Extension keyboard shortcuts are registered by the browser window's main
  // extensions toolbar container. Returning nullptr prevents the side panel
  // container's action models from overwriting those accelerators or retaining
  // a keybinding after BrowserView teardown.
  return nullptr;
}

views::BubbleAnchor
ContextualTasksExtensionsContainer::GetReferenceButtonForPopup(
    const extensions::ExtensionId& action_id) {
  return GetAnchor();
}

views::BubbleAnchor
ContextualTasksExtensionsContainer::GetExtensionsButtonAnchor() {
  return GetAnchor();
}

views::BubbleBorder::Arrow ContextualTasksExtensionsContainer::GetPopupArrow()
    const {
  return views::BubbleBorder::TOP_LEFT;
}

void ContextualTasksExtensionsContainer::OnToolbarModelInitialized() {
  CreateActions();
}

void ContextualTasksExtensionsContainer::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {
  CreateActionForId(action_id);
}

void ContextualTasksExtensionsContainer::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {
  auto it = actions_.find(action_id);
  if (it == actions_.end()) {
    return;
  }
  if (popup_owner_ == it->second.get()) {
    popup_owner_ = nullptr;
  }
  it->second->HidePopup();
  it->second->UnregisterCommand();
  actions_.erase(it);
}

void ContextualTasksExtensionsContainer::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {}

void ContextualTasksExtensionsContainer::OnToolbarPinnedActionsChanged() {}

void ContextualTasksExtensionsContainer::OnToolbarActionsModelShutdown() {
  toolbar_model_observation_.Reset();
  toolbar_model_ = nullptr;
  CloseExtensionsMenuIfOpen();
  HideActivePopup();
  popup_owner_ = nullptr;
  for (const auto& [_, model] : actions_) {
    model->UnregisterCommand();
  }
  actions_.clear();
}

void ContextualTasksExtensionsContainer::CreateActions() {
  if (!toolbar_model_ || !toolbar_model_->actions_initialized()) {
    return;
  }

  for (const auto& action_id : toolbar_model_->action_ids()) {
    CreateActionForId(action_id);
  }
}

void ContextualTasksExtensionsContainer::CreateActionForId(
    const ToolbarActionsModel::ActionId& action_id) {
  actions_[action_id] = ExtensionActionViewModel::Create(
      action_id, browser_,
      std::make_unique<ExtensionActionDelegateDesktop>(browser_, this, this));
}

views::BubbleAnchor ContextualTasksExtensionsContainer::GetAnchor() const {
  return anchor_provider_ ? anchor_provider_.Run() : views::BubbleAnchor();
}

}  // namespace contextual_tasks
