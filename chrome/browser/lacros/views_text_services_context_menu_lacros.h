// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_LACROS_H_
#define CHROME_BROWSER_LACROS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_LACROS_H_

#include <memory>

#include "ui/views/controls/views_text_services_context_menu_base.h"

namespace chromeos::clipboard_history {
class ClipboardHistorySubmenuModel;
}  // namespace chromeos::clipboard_history

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace views {
class Textfield;
}  // namespace views

namespace crosapi {

// This class implements support for adding and handling text service items in
// lacros-chrome browser native textfields (i.e., the omnibox but not the WebUI
// embedded in the browser).
class ViewsTextServicesContextMenuLacros
    : public views::ViewsTextServicesContextMenuBase {
 public:
  ViewsTextServicesContextMenuLacros(ui::SimpleMenuModel* menu,
                                     views::Textfield* client);
  ViewsTextServicesContextMenuLacros(
      const ViewsTextServicesContextMenuLacros&) = delete;
  ViewsTextServicesContextMenuLacros& operator=(
      const ViewsTextServicesContextMenuLacros&) = delete;
  ~ViewsTextServicesContextMenuLacros() override;

  // ViewsTextServicesContextMenuBase:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool SupportsCommand(int command_id) const override;

 private:
  // Shows the standalone clipboard history menu in Ash. `event_flags` describes
  // the event that caused the menu to show.
  void ShowClipboardHistoryMenu(int event_flags);

  // A submenu model to contain clipboard history item descriptors. Used only if
  // the clipboard history refresh feature is enabled.
  std::unique_ptr<chromeos::clipboard_history::ClipboardHistorySubmenuModel>
      submenu_model_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_VIEWS_TEXT_SERVICES_CONTEXT_MENU_LACROS_H_
