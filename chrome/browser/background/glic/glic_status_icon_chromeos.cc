// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_status_icon_chromeos.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/common/chrome_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

namespace glic {
namespace {}

GlicStatusIconChromeOS::GlicStatusIconChromeOS(GlicController* controller,
                                               StatusTray* status_tray)
    : GlicStatusIcon(controller, status_tray) {
  if (!base::FeatureList::IsEnabled(features::kGlicShowStatusTrayIcon)) {
    return;
  }
  native_theme_observer_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
}

GlicStatusIconChromeOS::~GlicStatusIconChromeOS() = default;

void GlicStatusIconChromeOS::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  status_icon()->SetImage(GetIcon());
}

// GlicStatusIcon:
gfx::ImageSkia GlicStatusIconChromeOS::GetIcon() const {
  const auto& icon =
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_STATUS_ICON);

  const bool in_dark_mode =
      ui::NativeTheme::GetInstanceForNativeUi()->preferred_color_scheme() ==
      ui::NativeTheme::PreferredColorScheme::kDark;

  return gfx::CreateVectorIcon(icon,
                               in_dark_mode ? SK_ColorWHITE : SK_ColorBLACK);
}

}  // namespace glic
