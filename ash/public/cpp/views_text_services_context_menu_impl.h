// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_VIEWS_TEXT_SERVICES_CONTEXT_MENU_IMPL_H_
#define ASH_PUBLIC_CPP_VIEWS_TEXT_SERVICES_CONTEXT_MENU_IMPL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ui/views/controls/views_text_services_context_menu_base.h"

namespace views {
class Textfield;
}

namespace ash {

// This class supports the text context menu with the exclusive functions under
// the CrOS environment.
class ASH_PUBLIC_EXPORT ViewsTextServicesContextMenuImpl
    : public views::ViewsTextServicesContextMenuBase {
 public:
  ViewsTextServicesContextMenuImpl(ui::SimpleMenuModel* menu,
                                   views::Textfield* client);
  ViewsTextServicesContextMenuImpl(const ViewsTextServicesContextMenuImpl&) =
      delete;
  ViewsTextServicesContextMenuImpl& operator=(
      const ViewsTextServicesContextMenuImpl&) = delete;
  ~ViewsTextServicesContextMenuImpl() override;

  // ViewsTextServicesContextMenuBase:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool SupportsCommand(int command_id) const override;

 private:
  // Adds the menu option which shows the clipboard history menu after
  // activation.
  void AddClipboardHistoryMenuOption(ui::SimpleMenuModel* menu);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_VIEWS_TEXT_SERVICES_CONTEXT_MENU_IMPL_H_
