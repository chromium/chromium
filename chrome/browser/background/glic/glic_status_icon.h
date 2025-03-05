// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/native_theme/native_theme_observer.h"

class StatusIcon;
class StatusIconMenuModel;
class StatusTray;
class Browser;

namespace glic {

class GlicController;

// This class abstracts away the details for creating a status tray icon and it
// context menu for the glic background mode manager. It is responsible for
// notifying the GlicController when the UI needs to be shown in response to the
// status icon being clicked or menu item being triggered.
class GlicStatusIcon : public StatusIconObserver,
                       public StatusIconMenuModel::Delegate,
                       public ui::NativeThemeObserver,
                       public BrowserListObserver {
 public:
  explicit GlicStatusIcon(GlicController* controller, StatusTray* status_tray);
  ~GlicStatusIcon() override;

  // StatusIconObserver
  void OnStatusIconClicked() override;

  // StatusIconMenuModel::Delegate
  void ExecuteCommand(int command_id, int event_flags) override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  void UpdateHotkey(const ui::Accelerator& hotkey);

  void UpdateShowExitInContextMenu();

 private:
  std::unique_ptr<StatusIconMenuModel> CreateStatusIconMenu();

  raw_ptr<GlicController> controller_;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observer_{this};

  raw_ptr<StatusTray> status_tray_;
  raw_ptr<StatusIcon> status_icon_;
  raw_ptr<StatusIconMenuModel> context_menu_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_H_
