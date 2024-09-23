// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/chrome_command_line_pref_store.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/language/core/browser/pref_names.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "components/sync/base/pref_names.h"
#include "content/public/common/content_switches.h"
#include "net/base/port_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_switches.h"
#endif

const CommandLinePrefStore::SwitchToPreferenceMapEntry
    ChromeCommandLinePrefStore::string_switch_map_[] = {
        {switches::kLang, language::prefs::kApplicationLocale},
        {switches::kAcceptLang, language::prefs::kSelectedLanguages},
        {switches::kAuthServerAllowlist, prefs::kAuthServerAllowlist},
        {switches::kSSLVersionMin, prefs::kSSLVersionMin},
        {switches::kSSLVersionMax, prefs::kSSLVersionMax},
        {switches::kWebRtcIPHandlingPolicy, prefs::kWebRTCIPHandlingPolicy},
#if BUILDFLAG(IS_ANDROID)
        {switches::kAuthAndroidNegotiateAccountType,
         prefs::kAuthAndroidNegotiateAccountType},
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
        {switches::kSchedulerConfiguration, prefs::kSchedulerConfiguration},
        {borealis::switches::kLaunchOptions,
         borealis::prefs::kExtraLaunchOptions},
#endif
};

const CommandLinePrefStore::SwitchToPreferenceMapEntry
    ChromeCommandLinePrefStore::path_switch_map_[] = {
      { switches::kDiskCacheDir, prefs::kDiskCacheDir },
      { switches::kLocalSyncBackendDir, syncer::prefs::kLocalSyncBackendDir },
};

const CommandLinePrefStore::BooleanSwitchToPreferenceMapEntry
    ChromeCommandLinePrefStore::boolean_switch_map_[] = {
        {switches::kDisable3DAPIs, prefs::kDisable3DAPIs, true},
        {switches::kEnableCloudPrintProxy, prefs::kCloudPrintProxyEnabled,
         true},
        {switches::kNoPings, prefs::kEnableHyperlinkAuditing, false},
        {switches::kAllowRunningInsecureContent,
         prefs::kWebKitAllowRunningInsecureContent, true},
        {switches::kAllowCrossOriginAuthPrompt,
         prefs::kAllowCrossOriginAuthPrompt, true},
        {switches::kDisablePrintPreview, prefs::kPrintPreviewDisabled, true},
        {safe_browsing::switches::kSbEnableEnhancedProtection,
         prefs::kSafeBrowsingEnhanced, true},
#if BUILDFLAG(IS_CHROMEOS_ASH)
        {ash::switches::kEnableTouchpadThreeFingerClick,
         ash::prefs::kEnableTouchpadThreeFingerClick, true},
        {switches::kEnableUnifiedDesktop,
         prefs::kUnifiedDesktopEnabledByDefault, true},
        {ash::switches::kEnableCastReceiver, prefs::kCastReceiverEnabled, true},
#endif
        {switches::kEnableLocalSyncBackend,
         syncer::prefs::kEnableLocalSyncBackend, true},
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
        {switches::kUseSystemDefaultPrinter,
         prefs::kPrintPreviewUseSystemDefaultPrinter, true},
#endif
        {switches::kSitePerProcess, prefs::kSitePerProcess, true},
};

const CommandLinePrefStore::SwitchToPreferenceMapEntry
    ChromeCommandLinePrefStore::integer_switch_map_[] = {
        {switches::kDiskCacheSize, prefs::kDiskCacheSize}};

ChromeCommandLinePrefStore::ChromeCommandLinePrefStore(
    const base::CommandLine* command_line)
    : CommandLinePrefStore(command_line) {
  ApplySimpleSwitches();
  ApplyProxyMode();
  ValidateProxySwitches();
  ApplySSLSwitches();
  ApplyBackgroundModeSwitches();
  ApplyExplicitlyAllowedPortSwitch();
}

ChromeCommandLinePrefStore::~ChromeCommandLinePrefStore() {}

bool ChromeCommandLinePrefStore::ValidateProxySwitches() {
  if (command_line()->HasSwitch(switches::kNoProxyServer) &&
      (command_line()->HasSwitch(switches::kProxyAutoDetect) ||
       command_line()->HasSwitch(switches::kProxyServer) ||
       command_line()->HasSwitch(switches::kProxyPacUrl) ||
       command_line()->HasSwitch(switches::kProxyBypassList))) {
    LOG(WARNING) << "Additional command-line proxy switches specified when --"
                 << switches::kNoProxyServer << " was also specified.";
    return false;
  }
  return true;
}

void ChromeCommandLinePrefStore::ApplySimpleSwitches() {
  // Look for each switch we know about and set its preference accordingly.
  ApplyStringSwitches(string_switch_map_);
  ApplyPathSwitches(path_switch_map_);
  ApplyIntegerSwitches(integer_switch_map_);
  ApplyBooleanSwitches(boolean_switch_map_);
}

void ChromeCommandLinePrefStore::ApplyProxyMode() {
  if (command_line()->HasSwitch(switches::kNoProxyServer)) {
    SetValue(proxy_config::prefs::kProxy,
             base::Value(ProxyConfigDictionary::CreateDirect()),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else if (command_line()->HasSwitch(switches::kProxyPacUrl)) {
    std::string pac_script_url =
        command_line()->GetSwitchValueASCII(switches::kProxyPacUrl);
    SetValue(proxy_config::prefs::kProxy,
             base::Value(
                 ProxyConfigDictionary::CreatePacScript(pac_script_url, false)),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else if (command_line()->HasSwitch(switches::kProxyAutoDetect)) {
    SetValue(proxy_config::prefs::kProxy,
             base::Value(ProxyConfigDictionary::CreateAutoDetect()),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  } else if (command_line()->HasSwitch(switches::kProxyServer)) {
    std::string proxy_server =
        command_line()->GetSwitchValueASCII(switches::kProxyServer);
    std::string bypass_list =
        command_line()->GetSwitchValueASCII(switches::kProxyBypassList);
    SetValue(proxy_config::prefs::kProxy,
             base::Value(ProxyConfigDictionary::CreateFixedServers(
                 proxy_server, bypass_list)),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

void ChromeCommandLinePrefStore::ApplySSLSwitches() {
  if (command_line()->HasSwitch(switches::kCipherSuiteBlacklist)) {
    base::Value::List list_value;
    const std::vector<std::string> str_list = base::SplitString(
        command_line()->GetSwitchValueASCII(switches::kCipherSuiteBlacklist),
        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const std::string& str : str_list) {
      list_value.Append(str);
    }
    SetValue(prefs::kCipherSuiteBlacklist, base::Value(std::move(list_value)),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

void ChromeCommandLinePrefStore::ApplyBackgroundModeSwitches() {
  if (command_line()->HasSwitch(switches::kDisableExtensions)) {
    SetValue(prefs::kBackgroundModeEnabled, base::Value(false),
             WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

void ChromeCommandLinePrefStore::ApplyExplicitlyAllowedPortSwitch() {
  if (!command_line()->HasSwitch(switches::kExplicitlyAllowedPorts)) {
    return;
  }

  base::Value::List integer_list;
  std::string switch_value =
      command_line()->GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
  const auto& split = base::SplitStringPiece(
      switch_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& piece : split) {
    int port;
    if (!base::StringToInt(piece, &port))
      continue;
    if (!net::IsPortValid(port))
      continue;
    integer_list.Append(base::Value(port));
  }
  SetValue(prefs::kExplicitlyAllowedNetworkPorts,
           base::Value(std::move(integer_list)),
           WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}
