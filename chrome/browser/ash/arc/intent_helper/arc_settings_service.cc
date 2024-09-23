// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/arc_settings_service.h"

#include <string>
#include <string_view>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/mojom/backup_settings.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/mojom/pip.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/gtest_prod_util.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/onc/onc_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/base/url_util.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "net/proxy_resolution/proxy_config.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/base_type_conversion.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace {

using ::ash::system::TimezoneSettings;

constexpr char kSetFontScaleAction[] =
    "org.chromium.arc.intent_helper.SET_FONT_SCALE";
constexpr char kSetPageZoomAction[] =
    "org.chromium.arc.intent_helper.SET_PAGE_ZOOM";
constexpr char kSetProxyAction[] = "org.chromium.arc.intent_helper.SET_PROXY";

constexpr char kArcProxyBypassListDelimiter[] = ",";

constexpr float kAndroidFontScaleNormal = 1;

// These values are based on
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/ash/settings/a11y_page/captions_subpage.ts;l=142;drc=0918c7f73782a9575396f0c6b80a722b5a3d255a
constexpr char kTextShadowRaised[] = "-2px -2px 4px rgba(0, 0, 0, 0.5)";
constexpr char kTextShadowDepressed[] = "2px 2px 4px rgba(0, 0, 0, 0.5)";
constexpr char kTextShadowUniform[] =
    "-1px 0px 0px black, 0px -1px 0px black, 1px 0px 0px black, 0px  1px 0px "
    "black";
constexpr char kTextShadowDropShadow[] =
    "0px 0px 2px rgba(0, 0, 0, 0.5), 2px 2px 2px black";

arc::mojom::CaptionColorPtr GetCaptionColorFromPrefs(
    const PrefService* prefs,
    const char* color_pref_name,
    const char* opacity_pref_name) {
  const std::string rgb = prefs->GetString(color_pref_name);
  if (rgb.empty()) {
    return nullptr;
  }
  const int opacity = prefs->GetInteger(opacity_pref_name);
  std::string color_str =
      base::StringPrintf("rgba(%s,%s)", rgb.c_str(),
                         base::NumberToString(opacity / 100.0).c_str());

  // Validate color value is correct by converting it to SkColor and retrieve
  // the values if it's valid. The caveat is due to the method being very
  // generic, it does some redundant stuffs (like utf16 conversion, removing rgb
  // prefix). But since this path is frequently used, the benefit of reusing
  // method outweighs the cons.
  std::optional<SkColor> sk_color =
      ui::metadata::SkColorConverter::FromString(base::UTF8ToUTF16(color_str));
  if (!sk_color) {
    return nullptr;
  }
  SkColor color = sk_color.value();
  return arc::mojom::CaptionColor::New(SkColorGetA(color), SkColorGetR(color),
                                       SkColorGetG(color), SkColorGetB(color));
}

float GetFontScaleFromPref(const PrefService* prefs) {
  std::string text_size =
      prefs->GetString(::prefs::kAccessibilityCaptionsTextSize);
  if (text_size.empty()) {
    return 1.0f;
  }
  CHECK(text_size[text_size.size() - 1] == '%');
  text_size = text_size.substr(0, text_size.size() - 1);
  int font_scale;
  CHECK(base::StringToInt(text_size, &font_scale));
  return font_scale / 100.0f;
}

arc::mojom::CaptionStylePtr GetCaptionStyleFromPrefs(const PrefService* prefs) {
  CHECK(prefs);

  arc::mojom::CaptionStylePtr style = arc::mojom::CaptionStyle::New();

  style->font_scale = GetFontScaleFromPref(prefs);
  style->text_color =
      GetCaptionColorFromPrefs(prefs, ::prefs::kAccessibilityCaptionsTextColor,
                               ::prefs::kAccessibilityCaptionsTextOpacity);
  style->background_color = GetCaptionColorFromPrefs(
      prefs, ::prefs::kAccessibilityCaptionsBackgroundColor,
      ::prefs::kAccessibilityCaptionsBackgroundOpacity);
  style->user_locale = prefs->GetString(::language::prefs::kApplicationLocale);

  const std::string text_shadow =
      prefs->GetString(::prefs::kAccessibilityCaptionsTextShadow);
  if (text_shadow == kTextShadowRaised) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::kRaised;
  } else if (text_shadow == kTextShadowDepressed) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::kDepressed;
  } else if (text_shadow == kTextShadowUniform) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::kUniform;
  } else if (text_shadow == kTextShadowDropShadow) {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::kDropShadow;
  } else {
    style->text_shadow_type = arc::mojom::CaptionTextShadowType::kNone;
  }

  return style;
}

bool GetHttpProxyServer(const ProxyConfigDictionary* proxy_config_dict,
                        std::string* host,
                        int* port) {
  std::string proxy_rules_string;
  if (!proxy_config_dict->GetProxyServer(&proxy_rules_string))
    return false;

  net::ProxyConfig::ProxyRules proxy_rules;
  proxy_rules.ParseFromString(proxy_rules_string);

  const net::ProxyList* proxy_list = nullptr;
  if (proxy_rules.type == net::ProxyConfig::ProxyRules::Type::PROXY_LIST) {
    proxy_list = &proxy_rules.single_proxies;
  } else if (proxy_rules.type ==
             net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME) {
    proxy_list = proxy_rules.MapUrlSchemeToProxyList(url::kHttpScheme);
  }
  if (!proxy_list || proxy_list->IsEmpty())
    return false;

  const net::ProxyChain& chain = proxy_list->First();
  CHECK(chain.is_single_proxy());
  const net::ProxyServer& server = chain.First();
  *host = server.host_port_pair().host();
  *port = server.host_port_pair().port();
  return !host->empty() && *port;
}

bool IsProxyAutoDetectionConfigured(
    const base::Value::Dict& proxy_config_dict) {
  ProxyConfigDictionary dict(proxy_config_dict.Clone());
  ProxyPrefs::ProxyMode mode;
  dict.GetMode(&mode);
  return mode == ProxyPrefs::MODE_AUTO_DETECT;
}

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcSettingsService.
class ArcSettingsServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcSettingsService,
          ArcSettingsServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcSettingsServiceFactory";

  static ArcSettingsServiceFactory* GetInstance() {
    return base::Singleton<ArcSettingsServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcSettingsServiceFactory>;
  ArcSettingsServiceFactory() = default;
  ~ArcSettingsServiceFactory() override = default;
};

}  // namespace

// Listens to changes for select Chrome settings (prefs) that Android cares
// about and sends the new values to Android to keep the state in sync.
class ArcSettingsServiceImpl : public TimezoneSettings::Observer,
                               public ConnectionObserver<mojom::AppInstance>,
                               public ash::NetworkStateHandlerObserver {
 public:
  ArcSettingsServiceImpl(Profile* profile,
                         ArcBridgeService* arc_bridge_service);
  ArcSettingsServiceImpl(const ArcSettingsServiceImpl&) = delete;
  ArcSettingsServiceImpl& operator=(const ArcSettingsServiceImpl&) = delete;
  ~ArcSettingsServiceImpl() override;

  // Called when a Chrome pref we have registered an observer for has changed.
  // Obtains the new pref value and sends it to Android.
  void OnPrefChanged(const std::string& pref_name) const;

  // TimezoneSettings::Observer:
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const ash::NetworkState* network) override;

  // Retrieves Chrome's state for the settings that need to be synced on the
  // initial Android boot and send it to Android. Called by ArcSettingsService.
  void SyncInitialSettings() const;

 private:
  PrefService* GetPrefs() const { return profile_->GetPrefs(); }

  // Returns whether kProxy pref proxy config is applied.
  bool IsPrefProxyConfigApplied() const;

  // Registers to observe changes for Chrome settings we care about.
  void StartObservingSettingsChanges();

  // Stops listening for Chrome settings changes.
  void StopObservingSettingsChanges();

  // Retrieves Chrome's state for the settings that need to be synced on each
  // Android boot and send it to Android.
  void SyncBootTimeSettings() const;
  // Retrieves Chrome's state for the settings that need to be synced on each
  // Android boot after AppInstance is ready and send it to Android.
  void SyncAppTimeSettings();
  // Send particular settings to Android.
  // Keep these lines ordered lexicographically.
  void SyncAccessibilityLargeMouseCursorEnabled() const;
  void SyncAccessibilityFeatures() const;
  void SyncAccessibilityVirtualKeyboardEnabled() const;
  void SyncBackupEnabled() const;
  void SyncCaptionStyle() const;
  void SyncConsumerAutoUpdateToggle() const;
  void SyncLocale() const;
  void SyncLocationServiceEnabled() const;
  void SyncProxySettings() const;
  bool IsSystemProxyActive() const;
  void SyncProxySettingsForSystemProxy() const;
  void SyncReportingConsent(bool initial_sync) const;
  void SyncPictureInPictureEnabled() const;
  void SyncTimeZone() const;
  void SyncTimeZoneByGeolocation() const;
  void SyncUse24HourClock() const;
  void SyncUserGeolocation() const;
  void SyncUserGeolocationAccuracy() const;

  // Resets Android's font scale to the default value.
  void ResetFontScaleToDefault() const;

  // Resets Android's display density to the default value.
  void ResetPageZoomToDefault() const;

  // Registers to listen to a particular pref.
  // This should be used when dealing with pref per profile.
  void AddPrefToObserve(const std::string& pref_name);

  // Registers to listen to a particular perf in local state.
  // This should be used when dealing with pref per device.
  void AddLocalStatePrefToObserve(const std::string& pref_name);

  // Returns the integer value of the pref.  pref_name must exist.
  int GetIntegerPref(const std::string& pref_name) const;

  // Returns the boolean value of the pref. pref_name must exist.
  bool GetBooleanPref(const std::string& pref_name) const;

  // Gets whether this is a managed pref.
  bool IsBooleanPrefManaged(const std::string& pref_name) const;

  // Sends boolean pref broadcast to the delegate.
  void SendBoolPrefSettingsBroadcast(const std::string& pref_name,
                                     const std::string& action) const;

  // Sends boolean local state pref broadcast to the delegate.
  void SendBoolLocalStatePrefSettingsBroadcast(const std::string& pref_name,
                                               const std::string& action) const;

  // Same as above, except sends a specific boolean value.
  void SendBoolValueSettingsBroadcast(bool value,
                                      bool managed,
                                      const std::string& action) const;

  // Sends a broadcast to the delegate.
  void SendSettingsBroadcast(const std::string& action,
                             const base::Value::Dict& extras) const;

  // ConnectionObserver<mojom::AppInstance>:
  void OnConnectionReady() override;

  const raw_ptr<Profile> profile_;
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // Manages pref observation registration.
  PrefChangeRegistrar registrar_;
  PrefChangeRegistrar local_state_registrar_;

  base::CallbackListSubscription reporting_consent_subscription_;

  // Subscription for preference change of default zoom level. Subscription
  // automatically unregisters a callback when it's destructed.
  base::CallbackListSubscription default_zoom_level_subscription_;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  // Name of the default network. Used to keep track of whether the default
  // network has changed.
  std::string default_network_name_;

  // Proxy configuration of the default network.
  std::optional<base::Value::Dict> default_proxy_config_;

  // The PAC URL associated with `default_network_name_`, received via the DHCP
  // discovery method.
  GURL dhcp_wpad_url_;
};

ArcSettingsServiceImpl::ArcSettingsServiceImpl(
    Profile* profile,
    ArcBridgeService* arc_bridge_service)
    : profile_(profile), arc_bridge_service_(arc_bridge_service) {
  StartObservingSettingsChanges();
  SyncBootTimeSettings();

  // Note: if App connection is already established, OnConnectionReady()
  // is synchronously called, so that initial sync is done in the method.
  arc_bridge_service_->app()->AddObserver(this);
}

ArcSettingsServiceImpl::~ArcSettingsServiceImpl() {
  StopObservingSettingsChanges();

  arc_bridge_service_->app()->RemoveObserver(this);
}

void ArcSettingsServiceImpl::OnPrefChanged(const std::string& pref_name) const {
  VLOG(1) << "OnPrefChanged: " << pref_name;
  // Keep these lines ordered lexicographically by pref_name.
  if (pref_name == onc::prefs::kDeviceOpenNetworkConfiguration ||
      pref_name == onc::prefs::kOpenNetworkConfiguration) {
    // Only update proxy settings if kProxy pref is not applied.
    if (IsPrefProxyConfigApplied()) {
      LOG(ERROR) << "Open Network Configuration proxy settings are not applied,"
                 << " because kProxy preference is configured.";
      return;
    }
    SyncProxySettings();
  } else if (pref_name == ::prefs::kAccessibilityCaptionsBackgroundColor ||
             pref_name == ::prefs::kAccessibilityCaptionsBackgroundOpacity ||
             pref_name == ::prefs::kAccessibilityCaptionsTextColor ||
             pref_name == ::prefs::kAccessibilityCaptionsTextFont ||
             pref_name == ::prefs::kAccessibilityCaptionsTextOpacity ||
             pref_name == ::prefs::kAccessibilityCaptionsTextShadow ||
             pref_name == ::prefs::kAccessibilityCaptionsTextSize) {
    SyncCaptionStyle();
  } else if (pref_name == ash::prefs::kAccessibilityFocusHighlightEnabled ||
             pref_name == ash::prefs::kAccessibilityScreenMagnifierEnabled ||
             pref_name == ash::prefs::kAccessibilitySelectToSpeakEnabled ||
             pref_name == ash::prefs::kAccessibilitySpokenFeedbackEnabled ||
             pref_name == ash::prefs::kAccessibilitySwitchAccessEnabled ||
             pref_name == ash::prefs::kDockedMagnifierEnabled) {
    SyncAccessibilityFeatures();
  } else if (pref_name == ash::prefs::kAccessibilityLargeCursorEnabled) {
    SyncAccessibilityLargeMouseCursorEnabled();
  } else if (pref_name == ash::prefs::kAccessibilityVirtualKeyboardEnabled) {
    SyncAccessibilityVirtualKeyboardEnabled();
  } else if (pref_name == ash::prefs::kUserGeolocationAccessLevel) {
    SyncUserGeolocation();
  } else if (pref_name == ash::prefs::kUserGeolocationAccuracyEnabled) {
    SyncUserGeolocationAccuracy();
  } else if (pref_name == ::language::prefs::kApplicationLocale ||
             pref_name == ::language::prefs::kPreferredLanguages) {
    SyncLocale();
    // Android separates locale settings for system language and caption
    // language, meanwhile ChromeOS settings treat it as one, hence we use
    // this same setting to update Android's caption locale.
    if (pref_name == ::language::prefs::kApplicationLocale) {
      SyncCaptionStyle();
    }
  } else if (pref_name == ::prefs::kConsumerAutoUpdateToggle) {
    SyncConsumerAutoUpdateToggle();
  } else if (pref_name == ::prefs::kUse24HourClock) {
    SyncUse24HourClock();
  } else if (pref_name == ::prefs::kResolveTimezoneByGeolocationMethod) {
    SyncTimeZoneByGeolocation();
  } else if (pref_name == proxy_config::prefs::kProxy ||
             pref_name == ::prefs::kSystemProxyUserTrafficHostAndPort) {
    SyncProxySettings();
  } else {
    LOG(ERROR) << "Unknown pref changed.";
  }
}

void ArcSettingsServiceImpl::TimezoneChanged(const icu::TimeZone& timezone) {
  SyncTimeZone();
}

// This function is called when the default network changes or when any of its
// properties change. If the proxy configuration of the default network has
// changed, this method will call `SyncProxySettings` which syncs the proxy
// settings with ARC. Proxy changes on the default network are triggered by:
// - a user changing the proxy in the Network Settings UI;
// - ONC policy changes;
// - DHCP settings the WPAD URL via option 252.
void ArcSettingsServiceImpl::DefaultNetworkChanged(
    const ash::NetworkState* network) {
  if (!network)
    return;

  bool dhcp_wpad_url_changed =
      dhcp_wpad_url_ != network->GetWebProxyAutoDiscoveryUrl();
  dhcp_wpad_url_ = network->GetWebProxyAutoDiscoveryUrl();

  if (IsPrefProxyConfigApplied()) {
    //  Normally, we would ignore proxy changes coming from the default network
    //  because the kProxy pref has priority. If the proxy is configured to use
    //  the Web Proxy Auto-Discovery (WPAD) Protocol via the DHCP discovery
    //  method, the PAC URL will be propagated to Chrome via the default network
    //  properties.
    if (dhcp_wpad_url_changed) {
      const base::Value& proxy =
          GetPrefs()->GetValue(proxy_config::prefs::kProxy);
      if (proxy.is_dict() && IsProxyAutoDetectionConfigured(proxy.GetDict())) {
        SyncProxySettings();
      }
    }
    return;
  }

  bool sync_proxy = false;
  // Trigger a proxy settings sync to ARC if the default network changes.
  if (default_network_name_ != network->name()) {
    default_network_name_ = network->name();
    default_proxy_config_.reset();
    sync_proxy = true;
  }
  // Trigger a proxy settings sync to ARC if the proxy configuration of the
  // default network changes. Note: this code is only called if kProxy pref is
  // not set.
  if (default_proxy_config_ != network->proxy_config()) {
    if (network->proxy_config()) {
      default_proxy_config_ = network->proxy_config()->Clone();
    } else {
      default_proxy_config_.reset();
    }
    sync_proxy = true;
  }

  // Check if proxy auto detection is enabled. If yes, and the PAC URL set via
  // DHCP has changed, propagate the change to ARC.
  if (default_proxy_config_.has_value() && dhcp_wpad_url_changed &&
      IsProxyAutoDetectionConfigured(default_proxy_config_.value())) {
    sync_proxy = true;
  }

  if (!sync_proxy)
    return;

  SyncProxySettings();
}

bool ArcSettingsServiceImpl::IsPrefProxyConfigApplied() const {
  net::ProxyConfigWithAnnotation config;
  return PrefProxyConfigTrackerImpl::PrefPrecedes(
      PrefProxyConfigTrackerImpl::ReadPrefConfig(GetPrefs(), &config));
}

void ArcSettingsServiceImpl::StartObservingSettingsChanges() {
  registrar_.Init(GetPrefs());
  local_state_registrar_.Init(g_browser_process->local_state());

  // Keep these lines ordered lexicographically.
  AddPrefToObserve(::prefs::kAccessibilityCaptionsBackgroundColor);
  AddPrefToObserve(::prefs::kAccessibilityCaptionsBackgroundOpacity);
  AddPrefToObserve(::prefs::kAccessibilityCaptionsTextColor);
  AddPrefToObserve(::prefs::kAccessibilityCaptionsTextFont);
  AddPrefToObserve(::prefs::kAccessibilityCaptionsTextOpacity);
  AddPrefToObserve(::prefs::kAccessibilityCaptionsTextShadow);
  AddPrefToObserve(::prefs::kAccessibilityCaptionsTextSize);
  AddPrefToObserve(::prefs::kResolveTimezoneByGeolocationMethod);
  AddPrefToObserve(::prefs::kSystemProxyUserTrafficHostAndPort);
  AddPrefToObserve(::prefs::kUse24HourClock);
  AddPrefToObserve(ash::prefs::kAccessibilityFocusHighlightEnabled);
  AddPrefToObserve(ash::prefs::kAccessibilityLargeCursorEnabled);
  AddPrefToObserve(ash::prefs::kAccessibilityScreenMagnifierEnabled);
  AddPrefToObserve(ash::prefs::kAccessibilitySelectToSpeakEnabled);
  AddPrefToObserve(ash::prefs::kAccessibilitySpokenFeedbackEnabled);
  AddPrefToObserve(ash::prefs::kAccessibilitySwitchAccessEnabled);
  AddPrefToObserve(ash::prefs::kAccessibilityVirtualKeyboardEnabled);
  AddPrefToObserve(ash::prefs::kDockedMagnifierEnabled);
  AddPrefToObserve(ash::prefs::kUserGeolocationAccessLevel);
  AddPrefToObserve(ash::prefs::kUserGeolocationAccuracyEnabled);
  AddPrefToObserve(onc::prefs::kDeviceOpenNetworkConfiguration);
  AddPrefToObserve(onc::prefs::kOpenNetworkConfiguration);
  AddPrefToObserve(proxy_config::prefs::kProxy);

  // Keep these lines ordered lexicographically.
  AddLocalStatePrefToObserve(::prefs::kConsumerAutoUpdateToggle);

  // Note that some preferences, such as kArcBackupRestoreEnabled and
  // kArcLocationServiceEnabled, are not dynamically updated after initial
  // ARC setup and therefore are not observed here.

  reporting_consent_subscription_ =
      ash::StatsReportingController::Get()->AddObserver(
          base::BindRepeating(&ArcSettingsServiceImpl::SyncReportingConsent,
                              base::Unretained(this), /*initial_sync=*/false));

  TimezoneSettings::GetInstance()->AddObserver(this);

  network_state_handler_observer_.Observe(
      ash::NetworkHandler::Get()->network_state_handler());
}

void ArcSettingsServiceImpl::StopObservingSettingsChanges() {
  registrar_.RemoveAll();
  local_state_registrar_.RemoveAll();
  reporting_consent_subscription_ = {};

  TimezoneSettings::GetInstance()->RemoveObserver(this);
  network_state_handler_observer_.Reset();
}

void ArcSettingsServiceImpl::SyncInitialSettings() const {
  // Keep these lines ordered lexicographically.
  SyncBackupEnabled();
  SyncLocationServiceEnabled();
  SyncReportingConsent(/*initial_sync=*/true);
}

void ArcSettingsServiceImpl::SyncBootTimeSettings() const {
  // Keep these lines ordered lexicographically.
  SyncAccessibilityLargeMouseCursorEnabled();
  SyncAccessibilityFeatures();
  SyncAccessibilityVirtualKeyboardEnabled();
  SyncCaptionStyle();
  SyncConsumerAutoUpdateToggle();
  SyncProxySettings();
  SyncReportingConsent(/*initial_sync=*/false);
  SyncPictureInPictureEnabled();
  SyncTimeZone();
  SyncTimeZoneByGeolocation();
  SyncUse24HourClock();

  // Reset the values to default in case the user had a custom value.
  // https://crbug.com/955071
  ResetFontScaleToDefault();
  ResetPageZoomToDefault();
}

void ArcSettingsServiceImpl::SyncAppTimeSettings() {
  // Applying system locales change on ARC will cause restarting other services
  // and applications on ARC and doing such change in early phase may lead to
  // ARC OptIn failure b/65385376. So that observing preferred languages change
  // should be deferred at least until |mojom::AppInstance| is ready.
  // Note that locale and preferred languages are passed to Android container on
  // boot time and current sync is redundant in most cases. However there is
  // race when preferred languages may be changed during the ARC booting.
  // Tracking and applying this information is complex task thar requires
  // syncronization ARC session start, ArcSettingsService and
  // ArcSettingsServiceImpl creation/destroying. Android framework has ability
  // to supress reduntant calls so having this little overhead simplifies common
  // implementation.
  SyncLocale();
  AddPrefToObserve(::language::prefs::kApplicationLocale);
  AddPrefToObserve(::language::prefs::kPreferredLanguages);
}

void ArcSettingsServiceImpl::SyncAccessibilityLargeMouseCursorEnabled() const {
  SendBoolPrefSettingsBroadcast(
      ash::prefs::kAccessibilityLargeCursorEnabled,
      "org.chromium.arc.intent_helper.ACCESSIBILITY_LARGE_POINTER_ICON");
}

void ArcSettingsServiceImpl::SyncAccessibilityVirtualKeyboardEnabled() const {
  SendBoolPrefSettingsBroadcast(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled,
      "org.chromium.arc.intent_helper.SET_SHOW_IME_WITH_HARD_KEYBOARD");
}

void ArcSettingsServiceImpl::SyncBackupEnabled() const {
  auto* backup_settings = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->backup_settings(), SetBackupEnabled);
  if (backup_settings) {
    const PrefService::Preference* pref =
        registrar_.prefs()->FindPreference(prefs::kArcBackupRestoreEnabled);
    DCHECK(pref);
    const base::Value* value = pref->GetValue();
    DCHECK(value->is_bool());
    backup_settings->SetBackupEnabled(value->GetBool(),
                                      !pref->IsUserModifiable());
  }
}

void ArcSettingsServiceImpl::SyncCaptionStyle() const {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->intent_helper(), SetCaptionStyle);
  if (!instance) {
    return;
  }

  const PrefService* pref_service = registrar_.prefs();
  CHECK(pref_service);
  arc::mojom::CaptionStylePtr caption_style =
      GetCaptionStyleFromPrefs(pref_service);
  CHECK(caption_style);

  instance->SetCaptionStyle(std::move(caption_style));
}

void ArcSettingsServiceImpl::SyncAccessibilityFeatures() const {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->intent_helper(), EnableAccessibilityFeatures);
  if (!instance) {
    return;
  }

  arc::mojom::AccessibilityFeaturesPtr features =
      arc::mojom::AccessibilityFeatures::New();
  features->docked_magnifier_enabled =
      GetBooleanPref(ash::prefs::kDockedMagnifierEnabled);
  features->focus_highlight_enabled =
      GetBooleanPref(ash::prefs::kAccessibilityFocusHighlightEnabled);
  features->screen_magnifier_enabled =
      GetBooleanPref(ash::prefs::kAccessibilityScreenMagnifierEnabled);
  features->select_to_speak_enabled =
      GetBooleanPref(ash::prefs::kAccessibilitySelectToSpeakEnabled);
  features->spoken_feedback_enabled =
      GetBooleanPref(ash::prefs::kAccessibilitySpokenFeedbackEnabled);
  features->switch_access_enabled =
      GetBooleanPref(ash::prefs::kAccessibilitySwitchAccessEnabled);

  instance->EnableAccessibilityFeatures(std::move(features));
}

void ArcSettingsServiceImpl::SyncLocale() const {
  if (IsArcLocaleSyncDisabled()) {
    VLOG(1) << "Locale sync is disabled.";
    return;
  }

  std::string locale;
  std::string preferred_languages;
  base::Value::Dict extras;
  // Chrome OS locale may contain only the language part (e.g. fr) but country
  // code (e.g. fr_FR).  Since Android expects locale to contain country code,
  // ARC will derive a likely locale with country code from such
  GetLocaleAndPreferredLanguages(profile_, &locale, &preferred_languages);
  extras.Set("locale", locale);
  extras.Set("preferredLanguages", preferred_languages);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_LOCALE", extras);
}

void ArcSettingsServiceImpl::SyncLocationServiceEnabled() const {
  SendBoolPrefSettingsBroadcast(
      prefs::kArcLocationServiceEnabled,
      "org.chromium.arc.intent_helper.SET_LOCATION_SERVICE_ENABLED");
}

// TODO(b/159871128, hugobenichi, acostinas) The current implementation only
// syncs the global proxy from Chrome's default network settings. ARC has
// multi-network support so we should sync per-network proxy configuration.
void ArcSettingsServiceImpl::SyncProxySettings() const {
  std::unique_ptr<ProxyConfigDictionary> proxy_config_dict =
      ash::ProxyConfigServiceImpl::GetActiveProxyConfigDictionary(
          GetPrefs(), g_browser_process->local_state());

  ProxyPrefs::ProxyMode mode;
  if (!proxy_config_dict || !proxy_config_dict->GetMode(&mode))
    mode = ProxyPrefs::MODE_DIRECT;

  if (mode != ProxyPrefs::MODE_DIRECT && IsSystemProxyActive()) {
    SyncProxySettingsForSystemProxy();
    return;
  }

  base::Value::Dict extras;
  extras.Set("mode", ProxyPrefs::ProxyModeToString(mode));

  switch (mode) {
    case ProxyPrefs::MODE_DIRECT:
      break;
    case ProxyPrefs::MODE_SYSTEM:
      VLOG(1) << "The system mode is not translated.";
      return;
    case ProxyPrefs::MODE_AUTO_DETECT: {
      // WPAD with DHCP has a higher priority than DNS.
      if (dhcp_wpad_url_.is_valid()) {
        extras.Set("pacUrl", dhcp_wpad_url_.spec());
      } else {
        // Fallback to WPAD via DNS.
        extras.Set("pacUrl", "http://wpad/wpad.dat");
      }
      break;
    }
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string pac_url;
      if (!proxy_config_dict->GetPacUrl(&pac_url)) {
        LOG(ERROR) << "No pac URL for pac_script proxy mode.";
        return;
      }
      extras.Set("pacUrl", pac_url);
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string host;
      int port = 0;
      if (!GetHttpProxyServer(proxy_config_dict.get(), &host, &port)) {
        LOG(ERROR) << "No Http proxy server is sent.";
        return;
      }
      extras.Set("host", host);
      extras.Set("port", port);

      std::string bypass_list;
      if (proxy_config_dict->GetBypassList(&bypass_list) &&
          !bypass_list.empty()) {
        // Chrome uses semicolon [;] as delimiter for the proxy bypass list
        // while ARC expects comma [,] delimiter.  Using the wrong delimiter
        // causes loss of network connectivity for many apps in ARC.
        auto bypassed_hosts = base::SplitStringPiece(
            bypass_list, net::ProxyBypassRules::kBypassListDelimeter,
            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
        bypass_list =
            base::JoinString(bypassed_hosts, kArcProxyBypassListDelimiter);
        extras.Set("bypassList", bypass_list);
      }
      break;
    }
    default:
      LOG(ERROR) << "Incorrect proxy mode.";
      return;
  }

  SendSettingsBroadcast(kSetProxyAction, extras);
}

bool ArcSettingsServiceImpl::IsSystemProxyActive() const {
  if (!profile_->GetPrefs()->HasPrefPath(
          ::prefs::kSystemProxyUserTrafficHostAndPort)) {
    return false;
  }

  const std::string proxy_host_and_port = profile_->GetPrefs()->GetString(
      ::prefs::kSystemProxyUserTrafficHostAndPort);
  // System-proxy can be active, but the network namespace for the worker
  // process is not yet configured.
  return !proxy_host_and_port.empty();
}

void ArcSettingsServiceImpl::SyncProxySettingsForSystemProxy() const {
  const std::string proxy_host_and_port = profile_->GetPrefs()->GetString(
      ::prefs::kSystemProxyUserTrafficHostAndPort);
  std::string host;
  int port;
  if (!net::ParseHostAndPort(proxy_host_and_port, &host, &port))
    return;

  base::Value::Dict extras;
  extras.Set("mode",
             ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS));
  extras.Set("host", host);
  extras.Set("port", port);
  SendSettingsBroadcast(kSetProxyAction, extras);
}

void ArcSettingsServiceImpl::SyncReportingConsent(bool initial_sync) const {
  bool consent = IsArcStatsReportingEnabled();
  if (consent && !initial_sync && policy_util::IsAccountManaged(profile_)) {
    // Don't enable reporting for managed users who might not have seen the
    // reporting notice during ARC setup.
    return;
  }
  if (consent && initial_sync &&
      profile_->GetPrefs()->GetBoolean(prefs::kArcSkippedReportingNotice)) {
    // Explicitly leave reporting off for users who did not get a reporting
    // notice during setup, but otherwise would have reporting on due to the
    // result of |IsArcStatsReportingEnabled()| during setup. Typically this is
    // due to the fact that ArcSessionManager was able to skip the setup UI for
    // managed users.
    consent = false;
  }
  base::Value::Dict extras;
  extras.Set("reportingConsent", consent);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_REPORTING_CONSENT",
                        extras);
}

void ArcSettingsServiceImpl::SyncPictureInPictureEnabled() const {
  bool isPipEnabled =
      base::FeatureList::IsEnabled(arc::kPictureInPictureFeature);

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->pip(),
                                               SetPipSuppressionStatus);
  if (!instance)
    return;

  instance->SetPipSuppressionStatus(!isPipEnabled);
}

void ArcSettingsServiceImpl::SyncTimeZone() const {
  TimezoneSettings* timezone_settings = TimezoneSettings::GetInstance();
  std::u16string timezoneID = timezone_settings->GetCurrentTimezoneID();
  base::Value::Dict extras;
  extras.Set("olsonTimeZone", timezoneID);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_TIME_ZONE", extras);
}

void ArcSettingsServiceImpl::SyncTimeZoneByGeolocation() const {
  base::Value::Dict extras;
  extras.Set("autoTimeZone", ash::system::TimeZoneResolverManager::
                                     GetEffectiveUserTimeZoneResolveMethod(
                                         registrar_.prefs(), false) !=
                                 ash::system::TimeZoneResolverManager::
                                     TimeZoneResolveMethod::DISABLED);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_AUTO_TIME_ZONE",
                        extras);
}

void ArcSettingsServiceImpl::SyncUse24HourClock() const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(::prefs::kUse24HourClock);
  DCHECK(pref);
  DCHECK(pref->GetValue()->is_bool());
  bool use24HourClock = pref->GetValue()->GetBool();
  base::Value::Dict extras;
  extras.Set("use24HourClock", use24HourClock);
  SendSettingsBroadcast("org.chromium.arc.intent_helper.SET_USE_24_HOUR_CLOCK",
                        extras);
}

void ArcSettingsServiceImpl::SyncUserGeolocation() const {
  // We are purposefully not calling SyncUserGeolocation() at boot,
  // as we sync this property from Android. We might need to sync
  // in case of disable but not in case of enable (default).

  // We need to map tri-state of ChromeOS toggle to boolean ARC++ toggle.
  const PrefService::Preference* pref = registrar_.prefs()->FindPreference(
      ash::prefs::kUserGeolocationAccessLevel);
  DCHECK(pref);
  DCHECK(pref->GetValue()->is_int());

  bool enabled_for_arc =
      ash::PrivacyHubController::CrosToArcGeolocationPermissionMapping(
          static_cast<ash::GeolocationAccessLevel>(pref->GetValue()->GetInt()));

  SendBoolValueSettingsBroadcast(
      enabled_for_arc, !pref->IsUserModifiable(),
      "org.chromium.arc.intent_helper.SET_USER_GEOLOCATION");
}

void ArcSettingsServiceImpl::SyncUserGeolocationAccuracy() const {
  SendBoolPrefSettingsBroadcast(
      ash::prefs::kUserGeolocationAccuracyEnabled,
      "org.chromium.arc.intent_helper.SET_USER_GEOLOCATION_ACCURACY_ENABLED");
}

void ArcSettingsServiceImpl::SyncConsumerAutoUpdateToggle() const {
  SendBoolLocalStatePrefSettingsBroadcast(
      ::prefs::kConsumerAutoUpdateToggle,
      "org.chromium.arc.intent_helper.SET_CONSUMER_AUTO_UPDATE");
}

void ArcSettingsServiceImpl::ResetFontScaleToDefault() const {
  base::Value::Dict extras;
  extras.Set("scale", kAndroidFontScaleNormal);
  SendSettingsBroadcast(kSetFontScaleAction, extras);
}

void ArcSettingsServiceImpl::ResetPageZoomToDefault() const {
  base::Value::Dict extras;
  extras.Set("zoomFactor", 1.0);
  SendSettingsBroadcast(kSetPageZoomAction, extras);
}

void ArcSettingsServiceImpl::AddPrefToObserve(const std::string& pref_name) {
  registrar_.Add(pref_name,
                 base::BindRepeating(&ArcSettingsServiceImpl::OnPrefChanged,
                                     base::Unretained(this)));
}

void ArcSettingsServiceImpl::AddLocalStatePrefToObserve(
    const std::string& pref_name) {
  local_state_registrar_.Add(
      pref_name, base::BindRepeating(&ArcSettingsServiceImpl::OnPrefChanged,
                                     base::Unretained(this)));
}

int ArcSettingsServiceImpl::GetIntegerPref(const std::string& pref_name) const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(pref_name);
  DCHECK(pref);
  DCHECK(pref->GetValue()->is_int());
  return pref->GetValue()->GetIfInt().value_or(-1);
}

bool ArcSettingsServiceImpl::GetBooleanPref(
    const std::string& pref_name) const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(pref_name);
  DCHECK(pref);
  DCHECK(pref->GetValue()->is_bool());
  return pref->GetValue()->GetBool();
}

bool ArcSettingsServiceImpl::IsBooleanPrefManaged(
    const std::string& pref_name) const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(pref_name);
  DCHECK(pref);
  bool value_exists = pref->GetValue()->is_bool();
  DCHECK(value_exists);
  return !pref->IsUserModifiable();
}

void ArcSettingsServiceImpl::SendBoolLocalStatePrefSettingsBroadcast(
    const std::string& pref_name,
    const std::string& action) const {
  DCHECK(g_browser_process);
  const PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const PrefService::Preference* local_state_pref =
      local_state->FindPreference(pref_name);
  DCHECK(local_state_pref);
  bool enabled = local_state->GetBoolean(pref_name);
  SendBoolValueSettingsBroadcast(enabled, !local_state_pref->IsUserModifiable(),
                                 action);
}

void ArcSettingsServiceImpl::SendBoolPrefSettingsBroadcast(
    const std::string& pref_name,
    const std::string& action) const {
  const PrefService::Preference* pref =
      registrar_.prefs()->FindPreference(pref_name);
  DCHECK(pref);
  DCHECK(pref->GetValue()->is_bool());
  bool enabled = pref->GetValue()->GetBool();
  SendBoolValueSettingsBroadcast(enabled, !pref->IsUserModifiable(), action);
}

void ArcSettingsServiceImpl::SendBoolValueSettingsBroadcast(
    bool enabled,
    bool managed,
    const std::string& action) const {
  base::Value::Dict extras;
  extras.Set("enabled", enabled);
  extras.Set("managed", managed);
  SendSettingsBroadcast(action, extras);
}

void ArcSettingsServiceImpl::SendSettingsBroadcast(
    const std::string& action,
    const base::Value::Dict& extras) const {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->intent_helper(), SendBroadcast);
  if (!instance)
    return;
  std::string extras_json;
  bool write_success = base::JSONWriter::Write(extras, &extras_json);
  DCHECK(write_success);

  instance->SendBroadcast(
      action, kArcIntentHelperPackageName,
      ArcIntentHelperBridge::AppendStringToIntentHelperPackageName(
          "SettingsReceiver"),
      extras_json);
}

// ConnectionObserver<mojom::AppInstance>:
void ArcSettingsServiceImpl::OnConnectionReady() {
  arc_bridge_service_->app()->RemoveObserver(this);
  SyncAppTimeSettings();
}

// static
ArcSettingsService* ArcSettingsService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcSettingsServiceFactory::GetForBrowserContext(context);
}

ArcSettingsService::ArcSettingsService(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : profile_(Profile::FromBrowserContext(context)),
      arc_bridge_service_(bridge_service) {
  arc_bridge_service_->intent_helper()->AddObserver(this);
  ArcSessionManager::Get()->AddObserver(this);

  if (!IsArcPlayStoreEnabledForProfile(profile_))
    SetInitialSettingsPending(false);
}

ArcSettingsService::~ArcSettingsService() {
  ArcSessionManager::Get()->RemoveObserver(this);
  arc_bridge_service_->intent_helper()->RemoveObserver(this);
}

void ArcSettingsService::OnConnectionReady() {
  impl_ =
      std::make_unique<ArcSettingsServiceImpl>(profile_, arc_bridge_service_);
  if (!IsInitialSettingsPending())
    return;
  impl_->SyncInitialSettings();
  SetInitialSettingsPending(false);
}

void ArcSettingsService::OnConnectionClosed() {
  impl_.reset();
}

void ArcSettingsService::OnArcPlayStoreEnabledChanged(bool enabled) {
  if (!enabled)
    SetInitialSettingsPending(false);
}

void ArcSettingsService::OnArcInitialStart() {
  DCHECK(!IsInitialSettingsPending());

  if (!impl_) {
    SetInitialSettingsPending(true);
    return;
  }

  impl_->SyncInitialSettings();
}

void ArcSettingsService::SetInitialSettingsPending(bool pending) {
  profile_->GetPrefs()->SetBoolean(prefs::kArcInitialSettingsPending, pending);
}

bool ArcSettingsService::IsInitialSettingsPending() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kArcInitialSettingsPending);
}

// static
void ArcSettingsService::EnsureFactoryBuilt() {
  ArcSettingsServiceFactory::GetInstance();
}

}  // namespace arc
