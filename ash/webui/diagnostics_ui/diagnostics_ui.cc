// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_ui.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/grit/ash_diagnostics_app_resources.h"
#include "ash/grit/ash_diagnostics_app_resources_map.h"
#include "ash/webui/common/backend/plural_string_handler.h"
#include "ash/webui/diagnostics_ui/backend/diagnostics_manager.h"
#include "ash/webui/diagnostics_ui/backend/histogram_util.h"
#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"
#include "ash/webui/diagnostics_ui/backend/network_health_provider.h"
#include "ash/webui/diagnostics_ui/backend/session_log_handler.h"
#include "ash/webui/diagnostics_ui/backend/system_data_provider.h"
#include "ash/webui/diagnostics_ui/backend/system_routine_controller.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace ash {

namespace {

std::u16string GetSettingsLinkLabel() {
  int string_id = IDS_DIAGNOSTICS_SETTINGS_LINK_TEXT;
  std::vector<std::u16string> replacements;
  const char* kOsSettingsUrl = "chrome://os-settings/";
  replacements.push_back(base::UTF8ToUTF16(kOsSettingsUrl));

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}

std::unique_ptr<base::DictionaryValue> GetDataSourceUpdate() {
  auto update = std::make_unique<base::DictionaryValue>();
  update->SetKey("settingsLinkText", base::Value(GetSettingsLinkLabel()));
  return update;
}

void AddDiagnosticsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
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
      {"captivePortalRoutineText", IDS_NETWORK_DIAGNOSTICS_CAPTIVE_PORTAL},
      {"cellularLabel", IDS_NETWORK_TYPE_CELLULAR},
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
      {"diagnosticsTitle", IDS_DIAGNOSTICS_TITLE},
      {"dischargeTestResultText", IDS_DISCHARGE_TEST_RESULT},
      {"dnsLatencyRoutineText", IDS_NETWORK_DIAGNOSTICS_DNS_LATENCY},
      {"dnsResolutionRoutineText", IDS_NETWORK_DIAGNOSTICS_DNS_RESOLUTION},
      {"dnsResolverPresentRoutineText",
       IDS_NETWORK_DIAGNOSTICS_DNS_RESOLVER_PRESENT},
      {"ethernetLabel", IDS_NETWORK_TYPE_ETHERNET},
      {"gatewayCanBePingedRoutineText",
       IDS_NETWORK_DIAGNOSTICS_GATEWAY_CAN_BE_PINGED},
      {"hasSecureWiFiConnectionRoutineText",
       IDS_NETWORK_DIAGNOSTICS_HAS_SECURE_WIFI_CONNECTION},
      {"hideReportText", IDS_DIAGNOSTICS_HIDE_REPORT_TEXT},
      {"httpFirewallRoutineText", IDS_NETWORK_DIAGNOSTICS_HTTP_FIREWALL},
      {"httpsFirewallRoutineText", IDS_NETWORK_DIAGNOSTICS_HTTPS_FIREWALL},
      {"httpsLatencyRoutineText", IDS_NETWORK_DIAGNOSTICS_HTTPS_LATENCY},
      {"ipConfigInfoDrawerGateway",
       IDS_NETWORK_DIAGNOSTICS_IP_CONFIG_INFO_DRAWER_GATEWAY},
      {"ipConfigInfoDrawerMacAddress",
       IDS_NETWORK_DIAGNOSTICS_IP_CONFIG_INFO_DRAWER_MAC_ADDRESS},
      {"ipConfigInfoDrawerSubnetMask",
       IDS_NETWORK_DIAGNOSTICS_IP_CONFIG_INFO_DRAWER_SUBNET_MASK},
      {"ipConfigInfoDrawerTitle",
       IDS_NETWORK_DIAGNOSTICS_IP_CONFIG_INFO_DRAWER_TITLE},
      {"lanConnectivityRoutineText", IDS_NETWORK_DIAGNOSTICS_LAN_CONNECTIVITY},
      {"learnMore", IDS_DIANOSTICS_LEARN_MORE_LABEL},
      {"learnMoreShort", IDS_DIAGNOSTICS_LEARN_MORE_LABEL_SHORT},
      {"memoryAvailable", IDS_DIAGNOSTICS_MEMORY_AVAILABLE_TEXT},
      {"memoryBannerMessage", IDS_DIAGNOSTICS_MEMORY_BANNER_MESSAGE},
      {"memoryRoutineText", IDS_DIAGNOSTICS_MEMORY_ROUTINE_TEXT},
      {"memoryTitle", IDS_DIAGNOSTICS_MEMORY_TITLE},
      {"networkAuthenticationLabel", IDS_NETWORK_DIAGNOSTICS_AUTHENTICATION},
      {"networkBssidLabel", IDS_ONC_WIFI_BSSID},
      {"networkChannelLabel", IDS_NETWORK_DIAGNOSTICS_CHANNEL},
      {"networkDefaultConnectionLabel",
       IDS_NETWORK_DIAGNOSTICS_DEFAULT_CONNECTION},
      {"networkDnsNotConfigured", IDS_NETWORK_DIAGNOSTICS_DNS_NOT_CONFIGURED},
      {"networkIpAddressLabel", IDS_NETWORK_DIAGNOSTICS_IP_ADDRESS},
      {"networkLinkSpeedLabel", IDS_NETWORK_DIAGNOSTICS_LINK_SPEED},
      {"networkSignalStrengthLabel", IDS_ONC_WIFI_SIGNAL_STRENGTH},
      {"networkSsidLabel", IDS_ONC_WIFI_SSID},
      {"networkStateConnectedText", IDS_NETWORK_HEALTH_STATE_CONNECTED},
      {"networkStateConnectingText", IDS_NETWORK_HEALTH_STATE_CONNECTING},
      {"networkStateNotConnectedText", IDS_NETWORK_HEALTH_STATE_NOT_CONNECTED},
      {"networkStateOnlineText", IDS_NETWORK_HEALTH_STATE_ONLINE},
      {"networkStatePortalText", IDS_NETWORK_HEALTH_STATE_PORTAL},
      {"networkSecurityLabel", IDS_NETWORK_DIAGNOSTICS_SECURITY},
      {"notEnoughAvailableMemoryMessage",
       IDS_DIAGNOSTICS_NOT_ENOUGH_AVAILABLE_MEMORY},
      {"overviewText", IDS_DIAGNOSTICS_OVERVIEW},
      {"percentageLabel", IDS_DIAGNOSTICS_PERCENTAGE_LABEL},
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
      {"signalStrengthRoutineText", IDS_NETWORK_DIAGNOSTICS_SIGNAL_STRENGTH},
      {"stopTestButtonText", IDS_DIAGNOSTICS_STOP_TEST_BUTTON_TEXT},
      {"testCancelledText", IDS_DIAGNOSTICS_CANCELLED_TEST_TEXT},
      {"testFailure", IDS_DIAGNOSTICS_TEST_FAILURE_TEXT},
      {"testFailedBadgeText", IDS_DIAGNOSTICS_TEST_FAILURE_BADGE_TEXT},
      {"testQueuedBadgeText", IDS_DIAGNOSTICS_TEST_QUEUED_BADGE_TEXT},
      {"testRunning", IDS_DIAGNOSTICS_TEST_RUNNING_TEXT},
      {"testRunningBadgeText", IDS_DIAGNOSTICS_TEST_RUNNING_BADGE_TEXT},
      {"testStoppedBadgeText", IDS_DIAGNOSTICS_TEST_STOPPED_BADGE_TEXT},
      {"testSuccess", IDS_DIAGNOSTICS_TEST_SUCCESS_TEXT},
      {"testSucceededBadgeText", IDS_DIAGNOSTICS_TEST_SUCCESS_BADGE_TEXT},
      {"troubleConnecting", IDS_DIAGNOSTICS_TROUBLE_CONNECTING},
      {"troubleshootingText", IDS_DIAGNOSTICS_TROUBLESHOOTING_TEXT},
      {"versionInfo", IDS_DIAGNOSTICS_VERSION_INFO_TEXT},
      {"wifiLabel", IDS_NETWORK_TYPE_WIFI},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddLocalizedStrings(*GetDataSourceUpdate());
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
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddBoolean("isLoggedIn", LoginState::Get()->IsUserLoggedIn());
  source->AddBoolean("isInputEnabled",
                     features::IsInputInDiagnosticsAppEnabled());
  source->AddBoolean("isNetworkingEnabled",
                     features::IsNetworkingInDiagnosticsAppEnabled());
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
    HoldingSpaceClient* holding_space_client)
    : ui::MojoWebDialogUI(web_ui),
      session_log_handler_(std::make_unique<diagnostics::SessionLogHandler>(
          select_file_policy_creator,
          holding_space_client)) {
  diagnostics_manager_ = std::make_unique<diagnostics::DiagnosticsManager>(
      session_log_handler_.get());

  auto html_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIDiagnosticsAppHost));
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  html_source->DisableTrustedTypesCSP();

  const auto resources = base::make_span(kAshDiagnosticsAppResources,
                                         kAshDiagnosticsAppResourcesSize);
  SetUpWebUIDataSource(html_source.get(), resources,
                       IDR_DIAGNOSTICS_APP_INDEX_HTML);

  SetUpPluralStringHandler(web_ui);

  auto session_log_handler = std::make_unique<diagnostics::SessionLogHandler>(
      select_file_policy_creator, holding_space_client);
  diagnostics_manager_ = std::make_unique<diagnostics::DiagnosticsManager>(
      session_log_handler.get());
  web_ui->AddMessageHandler(std::move(session_log_handler));

  AddDiagnosticsStrings(html_source.get());
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source.release());

  open_timestamp_ = base::Time::Now();
}

DiagnosticsDialogUI::~DiagnosticsDialogUI() {
  const base::TimeDelta time_open = base::Time::Now() - open_timestamp_;
  diagnostics::metrics::EmitAppOpenDuration(time_open);
}

void DiagnosticsDialogUI::BindInterface(
    mojo::PendingReceiver<diagnostics::mojom::NetworkHealthProvider> receiver) {
  DCHECK(features::IsNetworkingInDiagnosticsAppEnabled());
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

WEB_UI_CONTROLLER_TYPE_IMPL(DiagnosticsDialogUI)

}  // namespace ash
