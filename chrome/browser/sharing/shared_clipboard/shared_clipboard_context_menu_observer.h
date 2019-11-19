// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_CONTEXT_MENU_OBSERVER_H_
#define CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_CONTEXT_MENU_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/base/models/simple_menu_model.h"

namespace syncer {
class DeviceInfo;
}  // namespace syncer

class RenderViewContextMenuProxy;
class SharedClipboardUiController;

class SharedClipboardContextMenuObserver
    : public RenderViewContextMenuObserver {
 public:
  class SubMenuDelegate : public ui::SimpleMenuModel::Delegate {
   public:
    explicit SubMenuDelegate(SharedClipboardContextMenuObserver* parent);
    ~SubMenuDelegate() override;

    bool IsCommandIdEnabled(int command_id) const override;
    void ExecuteCommand(int command_id, int event_flags) override;

   private:
    SharedClipboardContextMenuObserver* const parent_;

    DISALLOW_COPY_AND_ASSIGN(SubMenuDelegate);
  };

  explicit SharedClipboardContextMenuObserver(
      RenderViewContextMenuProxy* proxy);
  ~SharedClipboardContextMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SharedClipboardContextMenuObserverTest,
                           SingleDevice_ShowMenu);
  FRIEND_TEST_ALL_PREFIXES(SharedClipboardContextMenuObserverTest,
                           MultipleDevices_ShowMenu);
  FRIEND_TEST_ALL_PREFIXES(SharedClipboardContextMenuObserverTest,
                           MultipleDevices_MoreThanMax_ShowMenu);

  void BuildSubMenu();

  void SendSharedClipboardMessage(int chosen_device_index);

  RenderViewContextMenuProxy* proxy_ = nullptr;

  SharedClipboardUiController* controller_ = nullptr;

  std::vector<std::unique_ptr<syncer::DeviceInfo>> devices_;

  SubMenuDelegate sub_menu_delegate_{this};

  base::string16 text_;

  std::unique_ptr<ui::SimpleMenuModel> sub_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardContextMenuObserver);
};

#endif  // CHROME_BROWSER_SHARING_SHARED_CLIPBOARD_SHARED_CLIPBOARD_CONTEXT_MENU_OBSERVER_H_
