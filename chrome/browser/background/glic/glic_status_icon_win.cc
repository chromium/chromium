// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_status_icon_win.h"

#include <windows.h>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/registry.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/background/glic/glic_controller.h"
#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/glic/resources/glic_resources.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace glic {

GlicStatusIconWin::GlicStatusIconWin(GlicController* controller,
                                     StatusTray* status_tray)
    : GlicStatusIcon(controller, status_tray) {}

GlicStatusIconWin::~GlicStatusIconWin() = default;

void GlicStatusIconWin::Init() {
  GlicStatusIcon::Init();

  if (hkcu_themes_regkey_.Open(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
          KEY_READ | KEY_NOTIFY) == ERROR_SUCCESS) {
    UpdateForThemesRegkey();
    // If there's no sequenced task runner handle, we can't be called back for
    // registry changes. This generally happens in tests.
    if (base::SequencedTaskRunner::HasCurrentDefault()) {
      RegisterThemesRegkeyObserver();
    }
  } else {
    // Fall back to the native theme's preferred color scheme.
    native_theme_observer_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  }
}

void GlicStatusIconWin::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  CHECK(!hkcu_themes_regkey_.Valid());
  in_dark_mode_ = observed_theme->preferred_color_scheme() ==
                  ui::NativeTheme::PreferredColorScheme::kDark;
  status_icon()->SetImage(GetIcon());
}

gfx::ImageSkia GlicStatusIconWin::GetIcon() const {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      glic::GetResourceID(in_dark_mode_ ? IDR_GLIC_STATUS_ICON_DARK
                                        : IDR_GLIC_STATUS_ICON_LIGHT));
}

void GlicStatusIconWin::RegisterThemesRegkeyObserver() {
  CHECK(hkcu_themes_regkey_.Valid());
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  hkcu_themes_regkey_.StartWatching(base::BindOnce(
      [](GlicStatusIconWin* icon) {
        icon->UpdateForThemesRegkey();
        // `StartWatching()`'s callback is one-shot and must be re-registered
        // for future notifications.
        icon->RegisterThemesRegkeyObserver();
      },
      base::Unretained(this)));
}

void GlicStatusIconWin::UpdateForThemesRegkey() {
  CHECK(hkcu_themes_regkey_.Valid());
  DWORD system_uses_light_theme = 1;
  hkcu_themes_regkey_.ReadValueDW(L"SystemUsesLightTheme",
                                  &system_uses_light_theme);
  in_dark_mode_ = !system_uses_light_theme;
  status_icon()->SetImage(GetIcon());
}

}  // namespace glic
