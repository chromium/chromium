// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include <string>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "ui/gfx/font_render_params.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "ui/views/controls/textfield/textfield.h"
#endif

#if defined(OS_MACOSX)
#include "ui/base/cocoa/defaults_utils.h"
#endif

#if defined(USE_AURA) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "ui/views/linux_ui/linux_ui.h"
#endif

namespace {

// Parses a string |range| with a port range in the form "<min>-<max>".
// If |range| is not in the correct format or contains an invalid range, zero
// is written to |min_port| and |max_port|.
// TODO(guidou): Consider replacing with remoting/protocol/port_range.cc
void ParsePortRange(const std::string& range,
                    uint16_t* min_port,
                    uint16_t* max_port) {
  *min_port = 0;
  *max_port = 0;

  if (range.empty())
    return;

  size_t separator_index = range.find('-');
  if (separator_index == std::string::npos)
    return;

  std::string min_port_string, max_port_string;
  base::TrimWhitespaceASCII(range.substr(0, separator_index), base::TRIM_ALL,
                            &min_port_string);
  base::TrimWhitespaceASCII(range.substr(separator_index + 1), base::TRIM_ALL,
                            &max_port_string);
  unsigned min_port_uint, max_port_uint;
  if (!base::StringToUint(min_port_string, &min_port_uint) ||
      !base::StringToUint(max_port_string, &max_port_uint)) {
    return;
  }
  if (min_port_uint == 0 || min_port_uint > max_port_uint ||
      max_port_uint > UINT16_MAX) {
    return;
  }

  *min_port = static_cast<uint16_t>(min_port_uint);
  *max_port = static_cast<uint16_t>(max_port_uint);
}

}  // namespace

namespace renderer_preferences_util {

void UpdateFromSystemSettings(content::RendererPreferences* prefs,
                              Profile* profile) {
  const PrefService* pref_service = profile->GetPrefs();
  prefs->accept_languages = pref_service->GetString(prefs::kAcceptLanguages);
  prefs->enable_referrers = pref_service->GetBoolean(prefs::kEnableReferrers);
  prefs->enable_do_not_track =
      pref_service->GetBoolean(prefs::kEnableDoNotTrack);
  prefs->enable_encrypted_media =
      pref_service->GetBoolean(prefs::kEnableEncryptedMedia);
  prefs->webrtc_ip_handling_policy = std::string();
  // Handling the backward compatibility of previous boolean verions of policy
  // controls.
  if (!pref_service->HasPrefPath(prefs::kWebRTCIPHandlingPolicy)) {
    if (!pref_service->GetBoolean(prefs::kWebRTCNonProxiedUdpEnabled)) {
      prefs->webrtc_ip_handling_policy =
          content::kWebRTCIPHandlingDisableNonProxiedUdp;
    } else if (!pref_service->GetBoolean(prefs::kWebRTCMultipleRoutesEnabled)) {
      prefs->webrtc_ip_handling_policy =
          content::kWebRTCIPHandlingDefaultPublicInterfaceOnly;
    }
  }
  if (prefs->webrtc_ip_handling_policy.empty()) {
    prefs->webrtc_ip_handling_policy =
        pref_service->GetString(prefs::kWebRTCIPHandlingPolicy);
  }
  std::string webrtc_udp_port_range =
      pref_service->GetString(prefs::kWebRTCUDPPortRange);
  ParsePortRange(webrtc_udp_port_range, &prefs->webrtc_udp_min_port,
                 &prefs->webrtc_udp_max_port);

#if BUILDFLAG(USE_DEFAULT_RENDER_THEME)
  prefs->focus_ring_color = SkColorSetRGB(0x4D, 0x90, 0xFE);
#if defined(OS_CHROMEOS)
  // This color is 0x544d90fe modulated with 0xffffff.
  prefs->active_selection_bg_color = SkColorSetRGB(0xCB, 0xE4, 0xFA);
  prefs->active_selection_fg_color = SK_ColorBLACK;
  prefs->inactive_selection_bg_color = SkColorSetRGB(0xEA, 0xEA, 0xEA);
  prefs->inactive_selection_fg_color = SK_ColorBLACK;
#endif
#endif

#if defined(TOOLKIT_VIEWS)
  prefs->caret_blink_interval = views::Textfield::GetCaretBlinkInterval();
#endif

#if defined(OS_MACOSX)
  base::TimeDelta interval;
  if (ui::TextInsertionCaretBlinkPeriod(&interval))
    prefs->caret_blink_interval = interval;
#endif

#if defined(USE_AURA) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
  views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui) {
    if (ThemeServiceFactory::GetForProfile(profile)->UsingSystemTheme()) {
      prefs->focus_ring_color = linux_ui->GetFocusRingColor();
      prefs->active_selection_bg_color = linux_ui->GetActiveSelectionBgColor();
      prefs->active_selection_fg_color = linux_ui->GetActiveSelectionFgColor();
      prefs->inactive_selection_bg_color =
        linux_ui->GetInactiveSelectionBgColor();
      prefs->inactive_selection_fg_color =
        linux_ui->GetInactiveSelectionFgColor();
    }

    // If we have a linux_ui object, set the caret blink interval regardless of
    // whether we're in native theme mode.
    prefs->caret_blink_interval = linux_ui->GetCursorBlinkInterval();
  }
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_WIN)
  static const base::NoDestructor<gfx::FontRenderParams> params(
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr));
  prefs->should_antialias_text = params->antialiasing;
  prefs->use_subpixel_positioning = params->subpixel_positioning;
  prefs->hinting = params->hinting;
  prefs->use_autohinter = params->autohinter;
  prefs->use_bitmaps = params->use_bitmaps;
  prefs->subpixel_rendering = params->subpixel_rendering;
#endif

#if !defined(OS_MACOSX)
  prefs->plugin_fullscreen_allowed =
      pref_service->GetBoolean(prefs::kFullscreenAllowed);
#endif
}

}  // namespace renderer_preferences_util
