// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

ContextMenuNotificationObserver::ContextMenuNotificationObserver(
    int command_to_execute)
    : ContextMenuNotificationObserver(command_to_execute,
                                      /*event_flags=*/0,
                                      /*callback=*/base::NullCallback()) {}
ContextMenuNotificationObserver::ContextMenuNotificationObserver(
    int command_to_execute,
    int event_flags,
    MenuShownCallback callback)
    : command_to_execute_(command_to_execute),
      event_flags_(event_flags),
      callback_(std::move(callback)) {
  RenderViewContextMenu::RegisterMenuShownCallbackForTesting(base::BindOnce(
      &ContextMenuNotificationObserver::MenuShown, base::Unretained(this)));
}

ContextMenuNotificationObserver::~ContextMenuNotificationObserver() = default;

void ContextMenuNotificationObserver::MenuShown(
    RenderViewContextMenu* context_menu) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ContextMenuNotificationObserver::ExecuteCommand,
                     base::Unretained(this), context_menu));
}

void ContextMenuNotificationObserver::ExecuteCommand(
    RenderViewContextMenu* context_menu) {
  context_menu->ExecuteCommand(command_to_execute_, event_flags_);
  context_menu->Cancel();
  if (callback_) {
    std::move(callback_).Run(context_menu);
  }
}

ContextMenuWaiter::ContextMenuWaiter() {
  RenderViewContextMenu::RegisterMenuShownCallbackForTesting(
      base::BindOnce(&ContextMenuWaiter::MenuShown, base::Unretained(this)));
}

ContextMenuWaiter::ContextMenuWaiter(int command_to_execute)
    : ContextMenuWaiter(command_to_execute, base::NullCallback()) {}

ContextMenuWaiter::ContextMenuWaiter(int command_to_execute,
                                     base::OnceClosure before_execute)
    : maybe_command_to_execute_(command_to_execute),
      before_execute_(std::move(before_execute)) {
  RenderViewContextMenu::RegisterMenuShownCallbackForTesting(
      base::BindOnce(&ContextMenuWaiter::MenuShown, base::Unretained(this)));
}

ContextMenuWaiter::~ContextMenuWaiter() = default;

void ContextMenuWaiter::MenuShown(RenderViewContextMenu* context_menu) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ContextMenuWaiter::Cancel,
                                base::Unretained(this), context_menu));
}

void ContextMenuWaiter::WaitForMenuOpenAndClose() {
  run_loop_.Run();
}

content::ContextMenuParams& ContextMenuWaiter::params() {
  return params_;
}

const std::vector<int>& ContextMenuWaiter::GetCapturedCommandIds() const {
  return captured_command_ids_;
}

const std::vector<int>& ContextMenuWaiter::GetCapturedEnabledCommandIds()
    const {
  return captured_enabled_command_ids_;
}

void ContextMenuWaiter::Cancel(RenderViewContextMenu* context_menu) {
  params_ = context_menu->params();

  const ui::SimpleMenuModel& menu_model = context_menu->menu_model();
  captured_command_ids_.reserve(menu_model.GetItemCount());
  captured_enabled_command_ids_.reserve(menu_model.GetItemCount());
  for (size_t i = 0; i < menu_model.GetItemCount(); ++i) {
    captured_command_ids_.push_back(menu_model.GetCommandIdAt(i));
    if (menu_model.IsEnabledAt(i)) {
      captured_enabled_command_ids_.push_back(menu_model.GetCommandIdAt(i));
    }
  }

  if (maybe_command_to_execute_) {
    if (before_execute_) {
      std::move(before_execute_).Run();
    }
    context_menu->ExecuteCommand(*maybe_command_to_execute_, 0);
  }
  context_menu->Cancel();
  run_loop_.Quit();
}
