// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/os_icon_provider_mac.h"

#include "chrome/browser/background/glic/glic_status_icon.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/paint_vector_icon.h"

namespace glic {
namespace {

constexpr int kStatusIconSizePx = 20;

}  // namespace

OSIconProviderMac::OSIconProviderMac(PrefService& prefs,
                                     GlicStatusIcon& glic_status_icon) {
  prefs.ClearPref(prefs::kGlicUseAltOSIcon);
}

OSIconProviderMac::~OSIconProviderMac() = default;

gfx::ImageSkia OSIconProviderMac::GetIcon() const {
  return gfx::CreateVectorIcon(
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_OS_ICON_VARIANT_0),
      kStatusIconSizePx, SK_ColorWHITE);
}

}  // namespace glic
