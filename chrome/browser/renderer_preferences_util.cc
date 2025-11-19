// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/convert_explicitly_allowed_network_ports_pref.h"
#include "content/public/browser/reduce_accept_language_utils.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#endif
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/peerconnection/webrtc_ip_handling_policy.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme.h"

#if defined(USE_AURA) && BUILDFLAG(IS_LINUX)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
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
  return content::ReduceAcceptLanguageUtils::GetLanguagesWithMaxCount(
      language_list);
}

}  // namespace

namespace renderer_preferences_util {

void UpdateFromSystemSettings(blink::RendererPreferences* prefs,
                              Profile* profile) {
  const PrefService* pref_service = profile->GetPrefs();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_WIN)
  content::UpdateFontRendererPreferencesFromSystemSettings(prefs);
#endif
  prefs->focus_ring_color = BUILDFLAG(IS_MAC) ? SkColorSetRGB(0x00, 0x5F, 0xCC)
                                              : SkColorSetRGB(0x10, 0x10, 0x10);
#if defined(USE_AURA)
#if BUILDFLAG(IS_CHROMEOS)
  // This color is 0x544d90fe modulated with 0xffffff.
  prefs->active_selection_bg_color = SkColorSetRGB(0xCB, 0xE4, 0xFA);
  prefs->active_selection_fg_color = SK_ColorBLACK;
  prefs->inactive_selection_bg_color = SkColorSetRGB(0xEA, 0xEA, 0xEA);
  prefs->inactive_selection_fg_color = SK_ColorBLACK;
#endif

#if BUILDFLAG(IS_LINUX)
  if (auto* linux_ui_theme = ui::LinuxUiTheme::GetForProfile(profile)) {
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
#endif  // BUILDFLAG(IS_LINUX)
#endif  // BUILDFLAG(USE_AURA)

#if BUILDFLAG(IS_LINUX)
  if (auto* linux_ui = ui::LinuxUi::instance()) {
    prefs->middle_click_paste_allowed = linux_ui->PrimaryPasteEnabled();
  }
#endif
  prefs->caret_blink_interval =
      ui::NativeTheme::GetInstanceForNativeUi()->caret_blink_interval();
  prefs->enable_referrers = pref_service->GetBoolean(prefs::kEnableReferrers);
  prefs->enable_do_not_track =
      pref_service->GetBoolean(prefs::kEnableDoNotTrack);
  prefs->enable_encrypted_media =
      pref_service->GetBoolean(prefs::kEnableEncryptedMedia);
  prefs->webrtc_ip_handling_policy = blink::ToWebRTCIPHandlingPolicy(
      pref_service->GetString(prefs::kWebRTCIPHandlingPolicy));

  for (const base::Value& entry :
       pref_service->GetList(prefs::kWebRTCIPHandlingUrl)) {
    const base::Value::Dict& dict = entry.GetDict();
    const std::string* url = dict.FindString("url");
    if (!url) {
      DVLOG(1) << "Malformed WebRtcIPHandlingUrl entry: Missing 'url' value.";
      continue;
    }
    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(*url);
    if (!pattern.IsValid()) {
      DVLOG(1)
          << "Malformed WebRtcIPHandlingUrl entry: Invalid pattern found: '"
          << *url << "'.";
      continue;
    }
    const std::string* handling = dict.FindString("handling");
    if (!handling) {
      DVLOG(1)
          << "Malformed WebRtcIPHandlingUrl entry: Missing 'handling' value.";
      continue;
    }
    prefs->webrtc_ip_handling_urls.push_back(
        {pattern, blink::ToWebRTCIPHandlingPolicy(*handling)});
  }
  if (pref_service->IsManagedPreference(
          prefs::kWebRTCPostQuantumKeyAgreement)) {
    prefs->webrtc_post_quantum_key_agreement =
        pref_service->GetBoolean(prefs::kWebRTCPostQuantumKeyAgreement);
  }

  ParsePortRange(pref_service->GetString(prefs::kWebRTCUDPPortRange),
                 &prefs->webrtc_udp_min_port, &prefs->webrtc_udp_max_port);
  prefs->webrtc_local_ips_allowed_urls = GetLocalIpsAllowedUrls(
      pref_service->GetList(prefs::kWebRtcLocalIpsAllowedUrls));
  prefs->accept_languages = GetLanguageListForProfile(
      profile, pref_service->GetString(language::prefs::kAcceptLanguages));
#if !BUILDFLAG(IS_MAC)
  prefs->plugin_fullscreen_allowed =
      pref_service->GetBoolean(prefs::kFullscreenAllowed);
#endif
#if BUILDFLAG(IS_ANDROID)
  prefs->uses_platform_autofill = pref_service->GetBoolean(
      autofill::prefs::kAutofillUsingVirtualViewStructure);
#endif
  prefs->caret_browsing_enabled =
      pref_service->GetBoolean(prefs::kCaretBrowsingEnabled);
  ui::AXPlatform::GetInstance().SetCaretBrowsingState(
      prefs->caret_browsing_enabled);
  if (PrefService* const local_state = g_browser_process->local_state()) {
    prefs->allow_cross_origin_auth_prompt =
        local_state->GetBoolean(prefs::kAllowCrossOriginAuthPrompt);

    prefs->explicitly_allowed_network_ports =
        ConvertExplicitlyAllowedNetworkPortsPref(local_state);
  }

  prefs->view_source_line_wrap_enabled =
      pref_service->GetBoolean(prefs::kViewSourceLineWrappingEnabled);
}

}  // namespace renderer_preferences_util
