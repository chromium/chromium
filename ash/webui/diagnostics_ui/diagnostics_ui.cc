// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/diagnostics_ui/diagnostics_ui.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/backend/plural_string_handler.h"
#include "ash/webui/common/keyboard_diagram_strings.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/diagnostics_ui/backend/common/histogram_util.h"
#include "ash/webui/diagnostics_ui/backend/connectivity/network_health_provider.h"
#include "ash/webui/diagnostics_ui/backend/diagnostics_manager.h"
#include "ash/webui/diagnostics_ui/backend/input/input_data_provider.h"
#include "ash/webui/diagnostics_ui/backend/session_log_handler.h"
#include "ash/webui/diagnostics_ui/backend/system/system_data_provider.h"
#include "ash/webui/diagnostics_ui/backend/system/system_routine_controller.h"
#include "ash/webui/diagnostics_ui/diagnostics_metrics.h"
#include "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "ash/webui/grit/ash_diagnostics_app_resources.h"
#include "ash/webui/grit/ash_diagnostics_app_resources_map.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash {

namespace {

void EmitInitialScreen(diagnostics::metrics::NavigationView initial_view) {
  base::UmaHistogramEnumeration("ChromeOS.DiagnosticsUi.InitialScreen",
                                initial_view);
}

diagnostics::metrics::NavigationView GetInitialView(const GURL url) {
  if (!url.has_query()) {
    return diagnostics::metrics::NavigationView::kSystem;
  }

  // Note: Valid query strings map to strings in the GetUrlForPage located in
  // chrome/browser/ui/webui/ash/diagnostics_dialog/diagnostics_dialog.cc.
  const std::string& original_query = url.query();  // must outlive |query|.
  std::string_view query =
      base::TrimString(original_query, " \t", base::TRIM_ALL);

  if (base::EqualsCaseInsensitiveASCII(query, "system")) {
    return diagnostics::metrics::NavigationView::kSystem;
  }

  if (base::EqualsCaseInsensitiveASCII(query, "connectivity")) {
    return diagnostics::metrics::NavigationView::kConnectivity;
  }

  if (base::EqualsCaseInsensitiveASCII(query, "input")) {
    return diagnostics::metrics::NavigationView::kInput;
  }

  // In production builds this is not expected to occur however it was observed
  // when running unit tests.
  LOG(WARNING) << "Unexpected screen requested with query: '" << query
               << "'. Defaulting value to system." << std::endl;

  return diagnostics::metrics::NavigationView::kSystem;
}

std::u16string GetLinkLabel(int string_id, const char* url) {
  std::vector<std::u16string> replacements;
  replacements.push_back(base::UTF8ToUTF16(url));
  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

base::Value::Dict GetDataSourceUpdate() {
  base::Value::Dict update;
  update.Set("settingsLinkText",
             GetLinkLabel(IDS_DIAGNOSTICS_SETTINGS_LINK_TEXT,
                          "chrome://os-settings/"));
  update.Set(
      "keyboardTesterHelpLink",
      GetLinkLabel(
          IDS_INPUT_DIAGNOSTICS_KEYBOARD_TESTER_HELP_LINK,
          "https://support.google.com/chromebook?p=keyboard_troubleshoot"));
  return update;
}

void AddDiagnosticsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"arcDnsResolutionFailedText",
       IDS_DIAGNOSTICS_ARC_DNS_RESOLUTION_FAILED_TEXT},
      {"arcDnsResolutionRoutineText",
       IDS_NETWORK_DIAGNOSTICS_ARC_DNS_RESOLUTION},
      {"arcHttpFailedText", IDS_DIAGNOSTICS_ARC_HTTP_FAILED_TEXT},
      {"arcHttpRoutineText", IDS_NETWORK_DIAGNOSTICS_ARC_HTTP},
      {"arcPingFailedText", IDS_DIAGNOSTICS_ARC_PING_FAILED_TEXT},
      {"arcPingRoutineText", IDS_NETWORK_DIAGNOSTICS_ARC_PING},
      {"batteryCalculatingText", IDS_DIAGNOSTICS_BATTERY_CALCULATING_TEXT},
      {"batteryChargeRoutineText", IDS_DIAGNOSTICS_BATTERY_CHARGE_ROUTINE_TEXT},
      {"batteryDischargeRoutineText",
       IDS_DIAGNOSTICS_BATTERY_DISCHARGE_ROUTINE_TEXT},
      {"batteryChargeTestFullMessage", IDS_DIAGNOSTICS_BATTERY_FULL_MESSAGE},
      {"batteryChargingStatusText", IDS_DIAGNOSTICS_BATTERY_CHARGING},
      {"batteryChipText", IDS_DIAGNOSTICS_BATTERY_CHIP_TEXT},
      {"batteryDischargingStatusText", IDS_DIAGNOSTICS_BATTERY_DISCHARGING},
      {"batteryFullText", IDS_DIAGNOSTICS_BATTERY_FULL_TEXT},
      {"batteryHealthLabel", IDS_DIAGNOSTICS_BATTERY_HEALTH_LABEL},
      {"batteryHealthText", IDS_DIAGNOSTICS_BATTERY_HEALTH_TEXT},
      {"batteryHealthTooltipText", IDS_DIAGNOSTICS_BATTERY_HEALTH_TOOLTIP_TEXT},
      {"batteryTitle", IDS_DIAGNOSTICS_BATTERY_TITLE},
      {"boardAndVersionInfo", IDS_DIAGNOSTICS_DEVICE_INFO_TEXT},
      {"captivePortalFailedText", IDS_DIAGNOSTICS_CAPTIVE_PORTAL_FAILED_TEXT},
      {"captivePortalRoutineText", IDS_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL},
      {"cellularLabel", IDS_DIAGNOSTICS_NETWORK_TYPE_CELLULAR},
      {"chargeTestResultText", IDS_CHARGE_TEST_RESULT},
      {"connectivityText", IDS_DIAGNOSTICS_CONNECTIVITY},
      {"cpuBannerMessage", IDS_DIAGNOSTICS_CPU_BANNER_MESSAGE},
      {"cpuCacheRoutineText", IDS_DIAGNOSTICS_CPU_CACHE_ROUTINE_TEXT},
      {"cpuChipText", IDS_DIAGNOSTICS_CPU_CHIP_TEXT},
      {"cpuFloatingPointAccuracyRoutineText",
       IDS_DIAGNOSTICS_CPU_FLOATING_POINT_ACCURACY_ROUTINE_TEXT},
      {"cpuPrimeSearchRoutineText",
       IDS_DIAGNOSTICS_CPU_PRIME_SEARCH_ROUTINE_TEXT},
      {"cpuSpeedLabel", IDS_DIAGNOSTICS_CPU_SPEED_LABEL},
      {"cpuStressRoutineText", IDS_DIAGNOSTICS_CPU_STRESS_ROUTINE_TEXT},
      {"cpuTempLabel", IDS_DIAGNOSTICS_CPU_TEMPERATURE_LABEL},
      {"cpuTempText", IDS_DIAGNOSTICS_CPU_TEMPERATURE_TEXT},
      {"cpuTitle", IDS_DIAGNOSTICS_CPU_TITLE},
      {"cpuUsageLabel", IDS_DIAGNOSTICS_CPU_USAGE_LABEL},
      {"cpuUsageText", IDS_DIAGNOSTICS_CPU_USAGE_TEXT},
      {"cpuUsageTooltipText", IDS_DIAGNOSTICS_CPU_USAGE_TOOLTIP_TEXT},
      {"cpuUsageSystem", IDS_DIAGNOSTICS_CPU_USAGE_SYSTEM_LABEL},
      {"cpuUsageUser", IDS_DIAGNOSTICS_CPU_USAGE_USER_LABEL},
      {"currentCpuSpeedText", IDS_DIAGNOSTICS_CPU_SPEED_TEXT},
      {"currentNowLabel", IDS_DIAGNOSTICS_CURRENT_NOW_LABEL},
      {"currentNowText", IDS_DIAGNOSTICS_CURRENT_NOW_TEXT},
      {"currentNowTooltipText", IDS_DIAGNOSTICS_CURRENT_NOW_TOOLTIP_TEXT},
      {"cycleCount", IDS_DIAGNOSTICS_CYCLE_COUNT_LABEL},
      {"cycleCountTooltipText", IDS_DIAGNOSTICS_CYCLE_COUNT_TOOLTIP_TEXT},
      {"deviceDisconnected", IDS_INPUT_DIAGNOSTICS_DEVICE_DISCONNECTED},
      {"diagnosticsTitle", IDS_DIAGNOSTICS_TITLE},
      {"disabledText", IDS_DIAGNOSTICS_DISABLED_TEXT},
      {"dischargeTestResultText", IDS_DISCHARGE_TEST_RESULT},
      {"dnsGroupText", IDS_NETWORK_DIAGNOSTICS_DNS_GROUP},
      {"dnsLatencyFailedText", IDS_DIAGNOSTICS_DNS_LATENCY_FAILED_TEXT},
      {"dnsLatencyRoutineText", IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY},
      {"dnsResolutionFailedText", IDS_DIAGNOSTICS_DNS_RESOLUTION_FAILED_TEXT},
      {"dnsResolutionRoutineText", IDS_NETWORK_DIAGNOSTICS_DNS_RESOLUTION},
      {"dnsResolverPresentFailedText",
       IDS_DIAGNOSTICS_DNS_RESOLVER_PRESENT_FAILED_TEXT},
      {"dnsResolverPresentRoutineText",
       IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT},
      {"ethernetLabel", IDS_NETWORK_TYPE_ETHERNET},
      {"firewallGroupText", IDS_NETWORK_DIAGNOSTICS_FIREWALL_GROUP},
      {"gatewayCanBePingedFailedText",
       IDS_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED_FAILED_TEXT},
      {"gatewayCanBePingedRoutineText",
       IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED},
      {"gatewayRoutineText", IDS_NETWORK_DIAGNOSTICS_GATEWAY_GROUP},
      {"hasSecureWiFiConnectionFailedText",
       IDS_DIAGNOSTICS_HAS_SECURE_WIFI_CONNECTION_FAILED_TEXT},
      {"hasSecureWiFiConnectionRoutineText",
       IDS_NETWORK_DIAGNOSTICS_HAS_SECURE_WIFI_CONNECTION},
      {"hideReportText", IDS_DIAGNOSTICS_HIDE_REPORT_TEXT},
      {"httpFirewallFailedText", IDS_DIAGNOSTICS_HTTP_FIREWALL_FAILED_TEXT},
      {"httpFirewallRoutineText", IDS_NETWORK_DIAGNOSTICS_HTTP_FIREWALL},
      {"httpsFirewallFailedText", IDS_DIAGNOSTICS_HTTPS_FIREWALL_FAILED_TEXT},
      {"httpsFirewallRoutineText", IDS_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL},
      {"httpsLatencyFailedText", IDS_DIAGNOSTICS_HTTPS_LATENCY_FAILED_TEXT},
      {"httpsLatencyRoutineText", IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY},
      {"inputCategoryKeyboard", IDS_INPUT_DIAGNOSTICS_CATEGORY_KEYBOARD},
      {"inputCategoryTouchpad", IDS_INPUT_DIAGNOSTICS_CATEGORY_TOUCHPAD},
      {"inputCategoryTouchscreen", IDS_INPUT_DIAGNOSTICS_CATEGORY_TOUCHSCREEN},
      {"inputDescriptionBluetoothKeyboard",
       IDS_INPUT_DIAGNOSTICS_BLUETOOTH_KEYBOARD},
      {"inputDescriptionBluetoothTouchpad",
       IDS_INPUT_DIAGNOSTICS_BLUETOOTH_TOUCHPAD},
      {"inputDescriptionBluetoothTouchscreen",
       IDS_INPUT_DIAGNOSTICS_BLUETOOTH_TOUCHSCREEN},
      {"inputDescriptionInternalKeyboard",
       IDS_INPUT_DIAGNOSTICS_INTERNAL_KEYBOARD},
      {"inputDescriptionInternalTouchpad",
       IDS_INPUT_DIAGNOSTICS_INTERNAL_TOUCHPAD},
      {"inputDescriptionInternalTouchscreen",
       IDS_INPUT_DIAGNOSTICS_INTERNAL_TOUCHSCREEN},
      {"inputDescriptionUsbKeyboard", IDS_INPUT_DIAGNOSTICS_USB_KEYBOARD},
      {"inputDescriptionUsbTouchpad", IDS_INPUT_DIAGNOSTICS_USB_TOUCHPAD},
      {"inputDescriptionUsbTouchscreen", IDS_INPUT_DIAGNOSTICS_USB_TOUCHSCREEN},
      {"inputDeviceTest", IDS_INPUT_DIAGNOSTICS_RUN_TEST},
      {"inputDeviceUntestableNote", IDS_INPUT_DIAGNOSTICS_UNTESTABLE_NOTE},
      {"inputKeyboardUntestableLidClosedNote",
       IDS_INPUT_DIAGNOSTICS_KEYBOARD_UNTESTABLE_LID_CLOSED_NOTE},
      {"inputKeyboardUntestableTabletModeNote",
       IDS_INPUT_DIAGNOSTICS_KEYBOARD_UNTESTABLE_TABLET_MODE_NOTE},
      {"inputKeyboardTesterClosedToastLidClosed",
       IDS_INPUT_DIAGNOSTICS_KEYBOARD_TESTER_CLOSED_LID_CLOSED_NOTE},
      {"inputKeyboardTesterClosedToastTabletMode",
       IDS_INPUT_DIAGNOSTICS_KEYBOARD_TESTER_CLOSED_TABLET_MODE_NOTE},
      {"inputTesterDone", IDS_INPUT_DIAGNOSTICS_TESTER_DONE},
      {"internetConnectivityGroupLabel",
       IDS_DIAGNOSTICS_INTERNET_CONNECTIVITY_GROUP_LABEL},
      {"ipConfigInfoDrawerGateway",
       IDS_NETWORK_DIAGNOSTICS_IP_CONFIG_INFO_DRAWER_GATEWAY},
      {"ipConfigInfoDrawerSubnetMask",
       IDS_NETWORK_DIAGNOSTICS_IP_CONFIG_INFO_DRAWER_SUBNET_MASK},
      {"ipConfigInfoDrawerTitle",
       IDS_NETWORK_DIAGNOSTICS_IP_CONFIG_INFO_DRAWER_TITLE},
      {"keyboardTesterFocusLossMessage",
       IDS_INPUT_DIAGNOSTICS_KEYBOARD_TESTER_FOCUS_LOSS_MESSAGE},
      {"keyboardTesterInstruction",
       IDS_INPUT_DIAGNOSTICS_KEYBOARD_TESTER_INSTRUCTION},
      {"keyboardTesterShortcutInstruction",
       IDS_INPUT_DIAGNOSTICS_KEYBOARD_TESTER_SHORTCUT_INSTRUCTION},
      {"keyboardTesterTitle", IDS_INPUT_DIAGNOSTICS_KEYBOARD_TESTER_TITLE},
      {"keyboardText", IDS_DIAGNOSTICS_KEYBOARD},
      {"joinNetworkLinkText", IDS_DIAGNOSTICS_JOIN_NETWORK_LINK_TEXT},
      {"lanConnectivityFailedText",
       IDS_DIAGNOSTICS_LAN_CONNECTIVITY_FAILED_TEXT},
      {"lanConnectivityGroupText", IDS_NETWORK_DIAGNOSTICS_CONNECTION_GROUP},
      {"lanConnectivityRoutineText", IDS_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY},
      {"learnMore", IDS_DIANOSTICS_LEARN_MORE_LABEL},
      {"learnMoreShort", IDS_DIAGNOSTICS_LEARN_MORE_LABEL_SHORT},
      {"localNetworkGroupLabel", IDS_DIAGNOSTICS_LOCAL_NETWORK_GROUP_LABEL},
      {"macAddressLabel", IDS_NETWORK_DIAGNOSTICS_MAC_ADDRESS_LABEL},
      {"memoryAvailable", IDS_DIAGNOSTICS_MEMORY_AVAILABLE_TEXT},
      {"memoryBannerMessage", IDS_DIAGNOSTICS_MEMORY_BANNER_MESSAGE},
      {"memoryRoutineText", IDS_DIAGNOSTICS_MEMORY_ROUTINE_TEXT},
      {"memoryTitle", IDS_DIAGNOSTICS_MEMORY_TITLE},
      {"missingNameServersText",
       IDS_NETWORK_DIAGNOSTICS_MISSING_NAME_SERVERS_TEXT},
      {"nameResolutionGroupLabel", IDS_DIAGNOSTICS_NAME_RESOLUTION_GROUP_LABEL},
      {"networkAuthenticationLabel", IDS_NETWORK_DIAGNOSTICS_AUTHENTICATION},
      {"networkBssidLabel", IDS_ONC_WIFI_BSSID},
      {"networkChannelLabel", IDS_NETWORK_DIAGNOSTICS_CHANNEL},
      {"networkDnsNotConfigured", IDS_NETWORK_DIAGNOSTICS_DNS_NOT_CONFIGURED},
      {"networkEidLabel", IDS_DIAGNOSTICS_EID_LABEL},
      {"networkEthernetAuthentication8021xLabel", IDS_ONC_WIFI_SECURITY_EAP},
      {"networkEthernetAuthenticationNoneLabel", IDS_ONC_WIFI_SECURITY_NONE},
      {"networkIccidLabel", IDS_ONC_CELLULAR_ICCID},
      {"networkIpAddressLabel", IDS_NETWORK_DIAGNOSTICS_IP_ADDRESS},
      {"networkRoamingOff", IDS_DIAGNOSTICS_ROAMING_OFF},
      {"networkRoamingStateHome", IDS_ONC_CELLULAR_ROAMING_STATE_HOME},
      {"networkRoamingStateLabel", IDS_ONC_CELLULAR_ROAMING_STATE},
      {"networkRoamingStateRoaming", IDS_ONC_CELLULAR_ROAMING_STATE_ROAMING},
      {"networkSignalStrengthLabel", IDS_ONC_WIFI_SIGNAL_STRENGTH},
      {"networkSimLockStatusLabel",
       IDS_DIAGNOSTICS_NETWORK_SIM_LOCK_STATUS_LABEL},
      {"networkSimLockedText", IDS_DIAGNOSTICS_NETWORK_SIM_LOCKED},
      {"networkSimUnlockedText", IDS_DIAGNOSTICS_NETWORK_SIM_UNLOCKED},
      {"networkSsidLabel", IDS_ONC_WIFI_SSID},
      {"networkStateConnectedText", IDS_NETWORK_HEALTH_STATE_CONNECTED},
      {"networkStateConnectingText", IDS_NETWORK_HEALTH_STATE_CONNECTING},
      {"networkStateDisabledText", IDS_NETWORK_HEALTH_STATE_DISABLED},
      {"networkStateNotConnectedText", IDS_NETWORK_HEALTH_STATE_NOT_CONNECTED},
      {"networkStateOnlineText", IDS_NETWORK_HEALTH_STATE_ONLINE},
      {"networkStatePortalText", IDS_NETWORK_HEALTH_STATE_PORTAL},
      {"networkSecurityLabel", IDS_NETWORK_DIAGNOSTICS_SECURITY},
      {"networkSecurityNoneLabel", IDS_ONC_WIFI_SECURITY_NONE},
      // 8021x uses EAP label in network element localization function.
      {"networkSecurityWep8021xLabel", IDS_ONC_WIFI_SECURITY_EAP},
      {"networkSecurityWepPskLabel", IDS_ONC_WIFI_SECURITY_WEP},
      {"networkSecurityWpaEapLabel", IDS_ONC_WIFI_SECURITY_EAP},
      {"networkSecurityWpaPskLabel", IDS_ONC_WIFI_SECURITY_PSK},
      {"networkTechnologyCdma1xrttLabel",
       IDS_NETWORK_DIAGNOSTICS_CELLULAR_CDMA1XRTT},
      {"networkTechnologyEdgeLabel", IDS_NETWORK_DIAGNOSTICS_CELLULAR_EDGE},
      {"networkTechnologyEvdoLabel", IDS_NETWORK_DIAGNOSTICS_CELLULAR_EVDO},
      {"networkTechnologyGprsLabel", IDS_NETWORK_DIAGNOSTICS_CELLULAR_GPRS},
      {"networkTechnologyGsmLabel", IDS_NETWORK_DIAGNOSTICS_CELLULAR_GSM},
      {"networkTechnologyHspaLabel", IDS_NETWORK_DIAGNOSTICS_CELLULAR_HSPA},
      {"networkTechnologyHspaPlusLabel",
       IDS_NETWORK_DIAGNOSTICS_CELLULAR_HSPA_PLUS},
      {"networkTechnologyLabel", IDS_ONC_CELLULAR_NETWORK_TECHNOLOGY},
      {"networkTechnologyLteLabel", IDS_NETWORK_DIAGNOSTICS_CELLULAR_LTE},
      {"networkTechnologyLteAdvancedLabel",
       IDS_NETWORK_DIAGNOSTICS_CELLULAR_LTE_ADVANCED},
      {"networkTechnologyUmtsLabel", IDS_NETWORK_DIAGNOSTICS_CELLULAR_UMTS},
      {"noIpAddressText", IDS_NETWORK_DIAGNOSTICS_NO_IP_ADDRESS_TEXT},
      {"notEnoughAvailableMemoryMessage",
       IDS_DIAGNOSTICS_NOT_ENOUGH_AVAILABLE_MEMORY},
      {"notEnoughAvailableMemoryCpuMessage",
       IDS_DIAGNOSTICS_NOT_ENOUGH_AVAILABLE_MEMORY_CPU},
      {"percentageLabel", IDS_DIAGNOSTICS_PERCENTAGE_LABEL},
      {"reconnectLinkText", IDS_DIAGNOSTICS_RECONNECT_LINK_TEXT},
      {"remainingCharge", IDS_DIAGNOSTICS_REMAINING_CHARGE_LABEL},
      {"routineEntryText", IDS_DIANOSTICS_ROUTINE_ENTRY_TEXT},
      {"routineNameText", IDS_DIANOSTICS_ROUTINE_NAME_TEXT},
      {"runAgainButtonText", IDS_DIAGNOSTICS_RUN_AGAIN_BUTTON_TEXT},
      {"routineRemainingMin", IDS_DIAGNOSTICS_ROUTINE_REMAINING_MIN},
      {"routineRemainingMinFinal", IDS_DIAGNOSTICS_ROUTINE_REMAINING_MIN_FINAL},
      {"routineRemainingMinFinalLarge",
       IDS_DIAGNOSTICS_ROUTINE_REMAINING_MIN_FINAL_LARGE},
      {"runBatteryChargeTestText",
       IDS_DIAGNOSTICS_CHARGE_RUN_TESTS_BUTTON_TEXT},
      {"runBatteryDischargeTestText",
       IDS_DIAGNOSTICS_DISCHARGE_RUN_TESTS_BUTTON_TEXT},
      {"runCpuTestText", IDS_DIAGNOSTICS_CPU_RUN_TESTS_BUTTON_TEXT},
      {"runMemoryTestText", IDS_DIAGNOSTICS_MEMORY_RUN_TESTS_BUTTON_TEXT},
      {"seeReportText", IDS_DIAGNOSTICS_SEE_REPORT_TEXT},
      {"sessionLog", IDS_DIAGNOSTICS_SESSION_LOG_LABEL},
      {"sessionLogToastTextFailure",
       IDS_DIAGNOSTICS_SESSION_LOG_TOAST_TEXT_FAILURE},
      {"sessionLogToastTextSuccess",
       IDS_DIAGNOSTICS_SESSION_LOG_TOAST_TEXT_SUCCESS},
      {"signalStrengthFailedText", IDS_DIAGNOSTICS_SIGNAL_STRENGTH_FAILED_TEXT},
      {"signalStrengthRoutineText", IDS_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH},
      {"signalStrength_Average",
       IDS_DIAGNOSTICS_NETWORK_SIGNAL_STRENGTH_AVERAGE},
      {"signalStrength_Excellent",
       IDS_DIAGNOSTICS_NETWORK_SIGNAL_STRENGTH_EXCELLENT},
      {"signalStrength_Good", IDS_DIAGNOSTICS_NETWORK_SIGNAL_STRENGTH_GOOD},
      {"signalStrength_Weak", IDS_DIAGNOSTICS_NETWORK_SIGNAL_STRENGTH_WEAK},
      {"stopTestButtonText", IDS_DIAGNOSTICS_STOP_TEST_BUTTON_TEXT},
      {"systemText", IDS_DIAGNOSTICS_SYSTEM},
      {"testCancelledText", IDS_DIAGNOSTICS_CANCELLED_TEST_TEXT},
      {"testFailure", IDS_DIAGNOSTICS_TEST_FAILURE_TEXT},
      {"testFailedBadgeText", IDS_DIAGNOSTICS_TEST_FAILURE_BADGE_TEXT},
      {"testOnRoutinesCompletedText", IDS_DIAGNOSTICS_TEST_ON_COMPLETED_TEXT},
      {"testQueuedBadgeText", IDS_DIAGNOSTICS_TEST_QUEUED_BADGE_TEXT},
      {"testRunning", IDS_DIAGNOSTICS_TEST_RUNNING_TEXT},
      {"testRunningBadgeText", IDS_DIAGNOSTICS_TEST_RUNNING_BADGE_TEXT},
      {"testSkippedBadgeText", IDS_DIAGNOSTICS_TEST_SKIPPED_BADGE_TEXT},
      {"testStoppedBadgeText", IDS_DIAGNOSTICS_TEST_STOPPED_BADGE_TEXT},
      {"testWarningBadgeText", IDS_DIAGNOSTICS_TEST_WARNING_BADGE_TEXT},
      {"testSuccess", IDS_DIAGNOSTICS_TEST_SUCCESS_TEXT},
      {"testSucceededBadgeText", IDS_DIAGNOSTICS_TEST_SUCCESS_BADGE_TEXT},
      {"touchpadTesterTitleText", IDS_INPUT_DIAGNOSTICS_TOUCHPAD_TESTER_TITLE},
      {"troubleConnecting", IDS_DIAGNOSTICS_TROUBLE_CONNECTING},
      {"troubleshootingText", IDS_DIAGNOSTICS_TROUBLESHOOTING_TEXT},
      {"versionInfo", IDS_DIAGNOSTICS_VERSION_INFO_TEXT},
      {"visitSettingsToConfigureLinkText",
       IDS_NETWORK_DIAGNOSTICS_VISIT_SETTINGS_TO_CONFIGURE_LINK_TEXT},
      {"wifiGroupLabel", IDS_NETWORK_DIAGNOSTICS_WIFI_GROUP},
      {"wifiLabel", IDS_NETWORK_TYPE_WIFI},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddLocalizedStrings(GetDataSourceUpdate());
  html_source->UseStringsJs();
}

void AddDiagnosticsAppPluralStrings(ash::PluralStringHandler* handler) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"nameServersText", IDS_DIAGNOSTICS_NAME_SERVERS}};

  for (const auto& str : kLocalizedStrings) {
    handler->AddStringToPluralMap(str.name, str.id);
  }
}

// TODO(jimmyxgong): Replace with webui::SetUpWebUIDataSource() once it no
// longer requires a dependency on //chrome/browser.
void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->AddResourcePath("", default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddBoolean("isLoggedIn", LoginState::Get()->IsUserLoggedIn());
  source->AddBoolean("isTouchpadEnabled",
                     features::IsTouchpadInDiagnosticsAppEnabled());
  source->AddBoolean("isTouchscreenEnabled",
                     features::IsTouchscreenInDiagnosticsAppEnabled());
}

void SetUpPluralStringHandler(content::WebUI* web_ui) {
  auto plural_string_handler = std::make_unique<ash::PluralStringHandler>();
  AddDiagnosticsAppPluralStrings(plural_string_handler.get());
  web_ui->AddMessageHandler(std::move(plural_string_handler));
}

}  // namespace

DiagnosticsDialogUI::DiagnosticsDialogUI(
    content::WebUI* web_ui,
    const diagnostics::SessionLogHandler::SelectFilePolicyCreator&
        select_file_policy_creator,
    HoldingSpaceClient* holding_space_client,
    const base::FilePath& log_directory_path)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIDiagnosticsAppHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  ash::EnableTrustedTypesCSP(html_source);

  const auto resources = base::make_span(kAshDiagnosticsAppResources,
                                         kAshDiagnosticsAppResourcesSize);
  SetUpWebUIDataSource(html_source, resources,
                       IDR_ASH_DIAGNOSTICS_APP_INDEX_HTML);

  SetUpPluralStringHandler(web_ui);

  auto session_log_handler = std::make_unique<diagnostics::SessionLogHandler>(
      select_file_policy_creator, holding_space_client, log_directory_path);
  diagnostics_manager_ = std::make_unique<diagnostics::DiagnosticsManager>(
      session_log_handler.get(), web_ui);
  web_ui->AddMessageHandler(std::move(session_log_handler));

  AddDiagnosticsStrings(html_source);
  ash::common::AddKeyboardDiagramStrings(html_source);
  // Add localized strings required for network-icon.
  ui::network_element::AddLocalizedStrings(html_source);
  ui::network_element::AddOncLocalizedStrings(html_source);

  // Configure SFUL metrics.
  diagnostics_metrics_ =
      std::make_unique<diagnostics::metrics::DiagnosticsMetrics>();
  diagnostics_metrics_->RecordUsage(true);

  // Setup application navigation metrics.
  diagnostics::metrics::NavigationView initial_view =
      GetInitialView(web_ui->GetWebContents()->GetURL());
  EmitInitialScreen(initial_view);
  web_ui->AddMessageHandler(
      std::make_unique<diagnostics::metrics::DiagnosticsMetricsMessageHandler>(
          initial_view));
  // TODO(ashleydp): Clean up timestamp when EmitAppOpenDuration is deprecated
  open_timestamp_ = base::Time::Now();
}

DiagnosticsDialogUI::~DiagnosticsDialogUI() {
  const base::TimeDelta time_open = base::Time::Now() - open_timestamp_;
  diagnostics_metrics_->StopSuccessfulUsage();
  // TODO(ashleydp): Clean up when EmitAppOpenDuration is deprecated.
  diagnostics::metrics::EmitAppOpenDuration(time_open);
}

void DiagnosticsDialogUI::BindInterface(
    mojo::PendingReceiver<diagnostics::mojom::NetworkHealthProvider> receiver) {
  diagnostics::NetworkHealthProvider* network_health_provider =
      diagnostics_manager_->GetNetworkHealthProvider();
  if (network_health_provider) {
    network_health_provider->BindInterface(std::move(receiver));
  }
}

void DiagnosticsDialogUI::BindInterface(
    mojo::PendingReceiver<diagnostics::mojom::SystemDataProvider> receiver) {
  diagnostics::SystemDataProvider* system_data_provider =
      diagnostics_manager_->GetSystemDataProvider();
  if (system_data_provider) {
    system_data_provider->BindInterface(std::move(receiver));
  }
}

void DiagnosticsDialogUI::BindInterface(
    mojo::PendingReceiver<diagnostics::mojom::SystemRoutineController>
        receiver) {
  diagnostics::SystemRoutineController* system_routine_controller =
      diagnostics_manager_->GetSystemRoutineController();
  if (system_routine_controller) {
    system_routine_controller->BindInterface(std::move(receiver));
  }
}

void DiagnosticsDialogUI::BindInterface(
    mojo::PendingReceiver<diagnostics::mojom::InputDataProvider> receiver) {
  diagnostics::InputDataProvider* input_data_provider =
      diagnostics_manager_->GetInputDataProvider();
  if (input_data_provider) {
    input_data_provider->BindInterface(std::move(receiver));
  }
}

void DiagnosticsDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(DiagnosticsDialogUI)

}  // namespace ash
