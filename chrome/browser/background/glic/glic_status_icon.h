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
#include "chrome/browser/ui/browser_list_observer.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#endif

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
#if BUILDFLAG(IS_WIN)
                       public ui::NativeThemeObserver,
#endif
                       public BrowserListObserver,
                       public GlicProfileManager::Observer,
                       public GlicWindowController::StateObserver {
 public:
  explicit GlicStatusIcon(GlicController* controller, StatusTray* status_tray);
  ~GlicStatusIcon() override;

  // StatusIconObserver:
  void OnStatusIconClicked() override;

  // StatusIconMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

#if BUILDFLAG(IS_WIN)
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;
#endif

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

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

 private:
  gfx::ImageSkia GetIcon() const;

  std::unique_ptr<StatusIconMenuModel> CreateStatusIconMenu();

#if BUILDFLAG(IS_WIN)
  void RegisterThemesRegkeyObserver();
  void UpdateForThemesRegkey();

  // System light/dark mode registry key.
  base::win::RegKey hkcu_themes_regkey_;

  // Theme change observer. Used only if registry key cannot be opened.
  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observer_{this};

  // Whether the system is in dark mode. The registry key takes precedence, if
  // available.
  bool in_dark_mode_ =
      ui::NativeTheme::GetInstanceForNativeUi()->preferred_color_scheme() ==
      ui::NativeTheme::PreferredColorScheme::kDark;
#endif

  raw_ptr<GlicController> controller_;

  base::ScopedObservation<GlicProfileManager, GlicProfileManager::Observer>
      profile_observer_{this};
  base::ScopedObservation<GlicWindowController,
                          GlicWindowController::StateObserver>
      panel_state_observer_{this};

  raw_ptr<StatusTray> status_tray_;
  raw_ptr<StatusIcon> status_icon_;
  raw_ptr<StatusIconMenuModel> context_menu_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_H_
