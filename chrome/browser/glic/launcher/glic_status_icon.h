// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_LAUNCHER_GLIC_STATUS_ICON_H_
#define CHROME_BROWSER_GLIC_LAUNCHER_GLIC_STATUS_ICON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"

class GlicController;
class StatusIcon;
class StatusIconMenuModel;
class StatusTray;

// This class abstracts away the details for creating a status tray icon and it
// context menu for the glic background mode manager. It is responsible for
// notifying the GlicController when the UI needs to be shown in response to the
// status icon being clicked or menu item being triggered.
class GlicStatusIcon : public StatusIconObserver,
                       public StatusIconMenuModel::Delegate {
 public:
  explicit GlicStatusIcon(GlicController* controller, StatusTray* status_tray);
  ~GlicStatusIcon() override;

  // StatusIconObserver
  void OnStatusIconClicked() override;

  // StatusIconMenuModel::Delegate
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  std::unique_ptr<StatusIconMenuModel> CreateStatusIconMenu();

  raw_ptr<GlicController> controller_;

  // TODO(https://crbug.com/378139555): Figure out how to not dangle these
  // pointers (and other instances of StatusTray/StatusIcon*).
  raw_ptr<StatusTray, DanglingUntriaged> status_tray_;
  raw_ptr<StatusIcon, DanglingUntriaged> status_icon_;
  raw_ptr<StatusIconMenuModel, DanglingUntriaged> context_menu_;
};

#endif  // CHROME_BROWSER_GLIC_LAUNCHER_GLIC_STATUS_ICON_H_
