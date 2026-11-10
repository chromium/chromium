// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_WIN_H_
#define CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_WIN_H_

#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/status_icons/status_tray_theme_watcher_win.h"

class StatusTray;

namespace glic {

class GlicBackgroundDelegate;

class GlicStatusIconWin : public GlicStatusIcon {
 public:
  GlicStatusIconWin(GlicBackgroundDelegate* delegate, StatusTray* status_tray);

  GlicStatusIconWin(const GlicStatusIconWin&) = delete;
  GlicStatusIconWin& operator=(const GlicStatusIconWin&) = delete;

  ~GlicStatusIconWin() override;

 private:
  // GlicStatusIcon:
  gfx::ImageSkia GetIcon() const override;

  void OnTaskbarThemeChanged();

  StatusTrayThemeWatcherWin taskbar_theme_watcher_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_BACKGROUND_GLIC_GLIC_STATUS_ICON_WIN_H_
