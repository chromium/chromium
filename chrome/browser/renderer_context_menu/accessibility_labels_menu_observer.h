// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_ACCESSIBILITY_LABELS_MENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_ACCESSIBILITY_LABELS_MENU_OBSERVER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_member.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"

class RenderViewContextMenuProxy;
class Profile;

// An observer that listens to events from the RenderViewContextMenu class and
// shows the accessibility labels menu if a screen reader is enabled.
class AccessibilityLabelsMenuObserver : public RenderViewContextMenuObserver {
 public:
  explicit AccessibilityLabelsMenuObserver(RenderViewContextMenuProxy* proxy);

  AccessibilityLabelsMenuObserver(const AccessibilityLabelsMenuObserver&) =
      delete;
  AccessibilityLabelsMenuObserver& operator=(
      const AccessibilityLabelsMenuObserver&) = delete;

  ~AccessibilityLabelsMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

  // Whether the accessibility labels menu item should be shown in the menu.
  // This might depend on whether a screen reader is running.
  bool ShouldShowLabelsItem();

 private:
  void ShowConfirmBubble(Profile* profile, bool enable_always);

  // The interface to add a context-menu item and update it. This class uses
  // this interface to avoid accessing context-menu items directly.
  raw_ptr<RenderViewContextMenuProxy> proxy_;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_ACCESSIBILITY_LABELS_MENU_OBSERVER_H_
