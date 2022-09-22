// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_

#include "base/run_loop.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "content/public/browser/context_menu_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class RenderViewContextMenu;

class ContextMenuNotificationObserver {
 public:
  // Wait for a context menu to be shown, and then execute |command_to_execute|
  // with specified |event_flags|. Also executes |callback| after executing the
  // command if provided.
  explicit ContextMenuNotificationObserver(
      int command_to_execute,
      int event_flags = 0,
      base::OnceCallback<void(RenderViewContextMenu*)> callback =
          base::NullCallbackAs<void(RenderViewContextMenu*)>());

  ContextMenuNotificationObserver(const ContextMenuNotificationObserver&) =
      delete;
  ContextMenuNotificationObserver& operator=(
      const ContextMenuNotificationObserver&) = delete;

  ~ContextMenuNotificationObserver();

 private:
  void MenuShown(RenderViewContextMenu* context_menu);

  void ExecuteCommand(RenderViewContextMenu* context_menu);

  int command_to_execute_;
  int event_flags_;
  base::OnceCallback<void(RenderViewContextMenu*)> callback_;
};

class ContextMenuWaiter {
 public:
  ContextMenuWaiter();
  explicit ContextMenuWaiter(int command_to_execute);

  ContextMenuWaiter(const ContextMenuWaiter&) = delete;
  ContextMenuWaiter& operator=(const ContextMenuWaiter&) = delete;

  ~ContextMenuWaiter();

  content::ContextMenuParams& params();
  const std::vector<int>& GetCapturedCommandIds() const;

  // Wait until the context menu is opened and closed.
  void WaitForMenuOpenAndClose();

 private:
  void MenuShown(RenderViewContextMenu* context_menu);

  void Cancel(RenderViewContextMenu* context_menu);

  content::ContextMenuParams params_;
  std::vector<int> captured_command_ids_;

  base::RunLoop run_loop_;
  absl::optional<int> maybe_command_to_execute_;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BROWSERTEST_UTIL_H_
