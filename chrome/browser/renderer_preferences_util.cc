// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/convert_explicitly_allowed_network_ports_pref.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_settings_factory.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#endif
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/ui_base_features.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/controls/textfield/textfield.h"
#endif

#if defined(USE_AURA) && BUILDFLAG(IS_LINUX)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "ui/linux/linux_ui.h"
#endif

namespace {

constexpr char kPrefixedVideoFullscreenApiEnabled[] = "enabled";
constexpr char kPrefixedVideoFullscreenApiDisabled[] = "disabled";

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

// Extracts the string representation of URLs allowed for local IP exposure.
std::vector<std::string> GetLocalIpsAllowedUrls(
    const base::Value::List& allowed_urls) {
  std::vector<std::string> ret;
  for (const auto& url : allowed_urls)
    ret.push_back(url.GetString());
  return ret;
}

std::string GetLanguageListForProfile(Profile* profile,
                                      const std::string& language_list) {
  if (profile->IsOffTheRecord()) {
    // In incognito mode return only the first language.
    return language::GetFirstLanguage(language_list);
  }
  return language_list;
}

}  // namespace

namespace renderer_preferences_util {

void UpdateFromSystemSettings(blink::RendererPreferences* prefs,
                              Profile* profile) {
  const PrefService* pref_service = profile->GetPrefs();
  prefs->accept_languages = GetLanguageListForProfile(
      profile, pref_service->GetString(language::prefs::kAcceptLanguages));
  prefs->enable_referrers = pref_service->GetBoolean(prefs::kEnableReferrers);
  prefs->enable_do_not_track =
      TrackingProtectionSettingsFactory::GetForProfile(profile)
          ->IsDoNotTrackEnabled();
  prefs->enable_encrypted_media =
      pref_service->GetBoolean(prefs::kEnableEncryptedMedia);
  prefs->webrtc_ip_handling_policy = std::string();
#if !BUILDFLAG(IS_ANDROID)
  prefs->caret_browsing_enabled =
      pref_service->GetBoolean(prefs::kCaretBrowsingEnabled);
  ui::AXPlatform::GetInstance().SetCaretBrowsingState(
      prefs->caret_browsing_enabled);
#endif

  if (prefs->webrtc_ip_handling_policy.empty()) {
    prefs->webrtc_ip_handling_policy =
        pref_service->GetString(prefs::kWebRTCIPHandlingPolicy);
  }
  std::string webrtc_udp_port_range =
      pref_service->GetString(prefs::kWebRTCUDPPortRange);
  ParsePortRange(webrtc_udp_port_range, &prefs->webrtc_udp_min_port,
                 &prefs->webrtc_udp_max_port);

  const base::Value::List& allowed_urls =
      pref_service->GetList(prefs::kWebRtcLocalIpsAllowedUrls);
  prefs->webrtc_local_ips_allowed_urls = GetLocalIpsAllowedUrls(allowed_urls);
#if defined(USE_AURA)
  prefs->focus_ring_color = SkColorSetRGB(0x4D, 0x90, 0xFE);
#if BUILDFLAG(IS_CHROMEOS)
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

#if defined(USE_AURA) && BUILDFLAG(IS_LINUX)
  auto* linux_ui_theme = ui::LinuxUiTheme::GetForProfile(profile);
  if (linux_ui_theme) {
    if (ThemeServiceFactory::GetForProfile(profile)->UsingSystemTheme()) {
      linux_ui_theme->GetFocusRingColor(&prefs->focus_ring_color);
      linux_ui_theme->GetActiveSelectionBgColor(
          &prefs->active_selection_bg_color);
      linux_ui_theme->GetActiveSelectionFgColor(
          &prefs->active_selection_fg_color);
      linux_ui_theme->GetInactiveSelectionBgColor(
          &prefs->inactive_selection_bg_color);
      linux_ui_theme->GetInactiveSelectionFgColor(
          &prefs->inactive_selection_fg_color);
    }
  }

    // If we have a linux_ui object, set the caret blink interval regardless of
    // whether we're in native theme mode.
  if (auto* linux_ui = ui::LinuxUi::instance())
    prefs->caret_blink_interval = linux_ui->GetCursorBlinkInterval();
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN)
  content::UpdateFontRendererPreferencesFromSystemSettings(prefs);
#endif

#if !BUILDFLAG(IS_MAC)
  prefs->plugin_fullscreen_allowed =
      pref_service->GetBoolean(prefs::kFullscreenAllowed);
#endif

  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    prefs->allow_cross_origin_auth_prompt =
        local_state->GetBoolean(prefs::kAllowCrossOriginAuthPrompt);

    prefs->explicitly_allowed_network_ports =
        ConvertExplicitlyAllowedNetworkPortsPref(local_state);
  }

#if BUILDFLAG(IS_MAC)
  prefs->focus_ring_color = SkColorSetRGB(0x00, 0x5F, 0xCC);
#else
  prefs->focus_ring_color = SkColorSetRGB(0x10, 0x10, 0x10);
#endif

  std::string fullscreen_video_api_availability =
      pref_service->GetString(prefs::kPrefixedVideoFullscreenApiAvailability);

  if (fullscreen_video_api_availability == kPrefixedVideoFullscreenApiEnabled) {
    prefs->prefixed_fullscreen_video_api_availability = true;
  } else if (fullscreen_video_api_availability ==
             kPrefixedVideoFullscreenApiDisabled) {
    prefs->prefixed_fullscreen_video_api_availability = false;
  } else {
    prefs->prefixed_fullscreen_video_api_availability = std::nullopt;
  }
}

}  // namespace renderer_preferences_util
