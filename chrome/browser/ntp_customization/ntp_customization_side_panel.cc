// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "chrome/browser/ui/customize_chrome/side_panel_controller.h"
#endif

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ntp_customization/jni_headers/NtpCustomizationSidePanel_jni.h"

namespace ntp_customization {

static void JNI_NtpCustomizationSidePanel_Show(TabAndroid* tab) {
  CHECK(tab);
#if BUILDFLAG(ENABLE_WEBUI_NTP)
  auto* controller =
      customize_chrome::SidePanelController::Get(tab->GetUnownedUserDataHost());
  if (!controller) {
    return;
  }
  controller->OpenSidePanel(SidePanelOpenTrigger::kAppMenu, std::nullopt);
#endif
}

}  // namespace ntp_customization

DEFINE_JNI(NtpCustomizationSidePanel)
