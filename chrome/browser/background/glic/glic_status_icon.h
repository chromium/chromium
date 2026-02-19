// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_icon_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"

class StatusIcon;
class StatusIconMenuModel;
class StatusTray;
class BrowserWindowInterface;
class GlobalBrowserCollection;

namespace glic {

class GlicController;

// This class abstracts away the details for creating a status tray icon and it
// context menu for the glic background mode manager. It is responsible for
// notifying the GlicController when the UI needs to be shown in response to the
// status icon being clicked or menu item being triggered.
class GlicStatusIcon : public StatusIconObserver,
                       public StatusIconMenuModel::Delegate,
                       public BrowserCollectionObserver,
                       public GlicProfileManager::Observer,
                       public GlicWindowController::StateObserver {
 public:
  static std::unique_ptr<GlicStatusIcon> Create(GlicController* controller,
                                                StatusTray* status_tray);

  GlicStatusIcon(GlicController* controller, StatusTray* status_tray);

  GlicStatusIcon(const GlicStatusIcon&) = delete;
  GlicStatusIcon& operator=(const GlicStatusIcon&) = delete;

  ~GlicStatusIcon() override;

  virtual void Init();

  // StatusIconObserver:
  void OnStatusIconClicked() override;

  // StatusIconMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

  // GlicProfileManager::Observer
  void OnLastActiveGlicProfileChanged(Profile* profile) override;

  // GlicWindowController::StateObserver
  void PanelStateChanged(
      const mojom::PanelState& panel_state,
      const GlicWindowController::PanelStateContext& context) override;

  void UpdateHotkey(const ui::Accelerator& hotkey);

  void UpdateVisibilityOfExitInContextMenu();
  void UpdateVisibilityOfShowAndCloseInContextMenu();

  StatusIconMenuModel* GetContextMenuForTesting() { return context_menu_; }

 protected:
  StatusIcon* status_icon() { return status_icon_; }

 private:
  virtual gfx::ImageSkia GetIcon() const;

  std::unique_ptr<StatusIconMenuModel> CreateStatusIconMenu();

  raw_ptr<GlicController> controller_;

  base::ScopedObservation<GlicProfileManager, GlicProfileManager::Observer>
      profile_observer_{this};
  base::ScopedObservation<GlicWindowController,
                          GlicWindowController::StateObserver>
      panel_state_observer_{this};
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};

  raw_ptr<StatusTray> status_tray_;
  raw_ptr<StatusIcon> status_icon_;
  raw_ptr<StatusIconMenuModel> context_menu_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_H_
