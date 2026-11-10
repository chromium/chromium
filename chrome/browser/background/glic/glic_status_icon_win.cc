// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_status_icon_win.h"

#include "base/functional/bind.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/glic/resources/glic_resources.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace glic {

GlicStatusIconWin::GlicStatusIconWin(GlicBackgroundDelegate* delegate,
                                     StatusTray* status_tray)
    : GlicStatusIcon(delegate, status_tray),
      taskbar_theme_watcher_(
          base::BindRepeating(&GlicStatusIconWin::OnTaskbarThemeChanged,
                              base::Unretained(this))) {}

GlicStatusIconWin::~GlicStatusIconWin() = default;

gfx::ImageSkia GlicStatusIconWin::GetIcon() const {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      glic::GetResourceID(taskbar_theme_watcher_.in_dark_mode()
                              ? IDR_GLIC_STATUS_ICON_DARK
                              : IDR_GLIC_STATUS_ICON_LIGHT));
}

void GlicStatusIconWin::OnTaskbarThemeChanged() {
  SetIcon(GetIcon());
}

}  // namespace glic
