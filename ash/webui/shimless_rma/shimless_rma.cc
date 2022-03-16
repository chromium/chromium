// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/shimless_rma.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/grit/ash_shimless_rma_resources.h"
#include "ash/webui/grit/ash_shimless_rma_resources_map.h"
#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"
#include "ash/webui/shimless_rma/url_constants.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/strings/network_element_localized_strings_provider.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace ash {

namespace {

// TODO(crbug/1051793): Replace with webui::SetUpWebUIDataSource() once it no
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
}

void AddShimlessRmaStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      // Component names. Used by select components and calibration pages.
      {"componentAudio", IDS_SHIMLESS_RMA_COMPONENT_AUDIO},
      {"componentBattery", IDS_SHIMLESS_RMA_COMPONENT_BATTERY},
      {"componentStorage", IDS_SHIMLESS_RMA_COMPONENT_STORAGE},
      {"componentVpdCached", IDS_SHIMLESS_RMA_COMPONENT_VPD_CACHE},
      {"componentNetwork", IDS_SHIMLESS_RMA_COMPONENT_NETWORK},
      {"componentCamera", IDS_SHIMLESS_RMA_COMPONENT_CAMERA},
      {"componentStylus", IDS_SHIMLESS_RMA_COMPONENT_STYLUS},
      {"componentTouchpad", IDS_SHIMLESS_RMA_COMPONENT_TOUCHPAD},
      {"componentTouchscreen", IDS_SHIMLESS_RMA_COMPONENT_TOUCHSCREEN},
      {"componentDram", IDS_SHIMLESS_RMA_COMPONENT_MEMORY},
      {"componentDisplayPanel", IDS_SHIMLESS_RMA_COMPONENT_DISPLAY_PANEL},
      {"componentCellular", IDS_SHIMLESS_RMA_COMPONENT_CELLULAR},
      {"componentEthernet", IDS_SHIMLESS_RMA_COMPONENT_ETHERNET},
      {"componentWireless", IDS_SHIMLESS_RMA_COMPONENT_WIRELESS},
      {"componentBaseAccelerometer",
       IDS_SHIMLESS_RMA_COMPONENT_BASE_ACCELEROMETER},
      {"componentLidAccelerometer",
       IDS_SHIMLESS_RMA_COMPONENT_LID_ACCELEROMETER},
      {"componentBaseGyroscope", IDS_SHIMLESS_RMA_COMPONENT_BASE_GYROSCOPE},
      {"componentLidGyroscope", IDS_SHIMLESS_RMA_COMPONENT_LID_GYROSCOPE},
      {"componentScreen", IDS_SHIMLESS_RMA_COMPONENT_SCREEN},
      {"componentKeyboard", IDS_SHIMLESS_RMA_COMPONENT_KEYBOARD},
      {"componentPowerButton", IDS_SHIMLESS_RMA_COMPONENT_POWER_BUTTON},
      // Splash screen
      {"shimlessSplashRemembering", IDS_SHIMLESS_RMA_SPLASH_REMEMBERING},
      {"shimlessSplashLoading", IDS_SHIMLESS_RMA_SPLASH_LOADING},
      // Common buttons
      {"cancelButtonLabel", IDS_SHIMLESS_RMA_CANCEL_BUTTON},
      {"backButtonLabel", IDS_SHIMLESS_RMA_BACK_BUTTON},
      {"nextButtonLabel", IDS_SHIMLESS_RMA_NEXT_BUTTON},
      {"skipButtonLabel", IDS_SHIMLESS_RMA_SKIP_BUTTON},
      {"okButtonLabel", IDS_SHIMLESS_RMA_OK_BUTTON},
      {"retryButtonLabel", IDS_SHIMLESS_RMA_RETRY_BUTTON},
      // Landing page
      {"beginRmaWarningText", IDS_SHIMLESS_RMA_AUTHORIZED_TECH_ONLY_WARNING},
      {"validatingComponentsText", IDS_SHIMLESS_RMA_VALIDATING_COMPONENTS},
      {"validatedComponentsSuccessText",
       IDS_SHIMLESS_RMA_VALIDATED_COMPONENTS_SUCCESS},
      {"validatedComponentsFailText",
       IDS_SHIMLESS_RMA_VALIDATED_COMPONENTS_FAIL},
      {"getStartedButtonLabel", IDS_SHIMLESS_RMA_GET_STARTED_BUTTON_LABEL},
      {"unqualifiedComponentsTitle",
       IDS_SHIMLESS_RMA_UNQUALIFIED_COMPONENTS_TITLE},
      // Network connect page
      {"connectNetworkTitleText", IDS_SHIMLESS_RMA_CONNECT_PAGE_TITLE},
      {"connectNetworkDescriptionText",
       IDS_SHIMLESS_RMA_CONNECT_PAGE_DESCRIPTION},
      {"connectNetworkDialogConnectButtonText",
       IDS_SHIMLESS_RMA_CONNECT_DIALOG_CONNECT},
      {"connectNetworkDialogCancelButtonText",
       IDS_SHIMLESS_RMA_CONNECT_DIALOG_CANCEL},
      {"internetConfigName", IDS_SHIMLESS_RMA_CONNECT_DIALOG_CONFIG_NAME},
      {"internetJoinType", IDS_SHIMLESS_RMA_CONNECT_DIALOG_JOIN_TYPE},
      // Select components page
      {"selectComponentsTitleText",
       IDS_SHIMLESS_RMA_SELECT_COMPONENTS_PAGE_TITLE},
      {"reworkFlowLinkText", IDS_SHIMLESS_RMA_REWORK_FLOW_LINK},
      // Choose destination page
      {"chooseDestinationTitleText", IDS_SHIMLESS_RMA_CHOOSE_DESTINATION},
      {"sameOwnerText", IDS_SHIMLESS_RMA_SAME_OWNER},
      {"sameOwnerDescriptionText", IDS_SHIMLESS_RMA_SAME_OWNER_DESCRIPTION},
      {"newOwnerText", IDS_SHIMLESS_RMA_NEW_OWNER},
      {"newOwnerDescriptionText", IDS_SHIMLESS_RMA_NEW_OWNER_DESCRIPTION},
      // OS update page
      {"osUpdateTitleText", IDS_SHIMLESS_RMA_UPDATE_OS_PAGE_TITLE},
      {"osUpdateUnqualifiedComponentsTopText",
       IDS_SHIMLESS_RMA_UPDATE_OS_UNQUALIFIED_COMPONENTS_TOP},
      {"osUpdateUnqualifiedComponentsBottomText",
       IDS_SHIMLESS_RMA_UPDATE_OS_UNQUALIFIED_COMPONENTS_BOTTOM},
      {"osUpdateVeryOutOfDateDescriptionText",
       IDS_SHIMLESS_RMA_UPDATE_OS_VERY_OUT_OF_DATE},
      {"osUpdateOutOfDateDescriptionText",
       IDS_SHIMLESS_RMA_UPDATE_OS_OUT_OF_DATE},
      {"osUpdateNetworkUnavailableText",
       IDS_SHIMLESS_RMA_UPDATE_OS_NETWORK_UNAVAILABLE},
      {"osUpdateFailedToStartText", IDS_SHIMLESS_RMA_UPDATE_OS_FAILED_TO_START},
      {"currentVersionText", IDS_SHIMLESS_RMA_CURRENT_VERSION},
      {"currentVersionOutOfDateText",
       IDS_SHIMLESS_RMA_CURRENT_VERSION_OUT_OF_DATE},
      {"currentVersionUpToDateText",
       IDS_SHIMLESS_RMA_CURRENT_VERSION_UP_TO_DATE},
      {"updateVersionRestartLabel",
       IDS_SHIMLESS_RMA_UPDATE_VERSION_AND_RESTART},
      // Choose WP disable method page
      {"chooseWpDisableMethodPageTitleText",
       IDS_SHIMLESS_RMA_CHOOSE_WP_DISABLE_METHOD_PAGE_TITLE},
      {"manualWpDisableMethodOptionText",
       IDS_SHIMLESS_RMA_MANUAL_WP_DISABLE_METHOD_OPTION},
      {"rsuWpDisableMethodOptionText",
       IDS_SHIMLESS_RMA_RSU_WP_DISABLE_METHOD_OPTION},
      {"rsuWpDisableMethodDescriptionText",
       IDS_SHIMLESS_RMA_RSU_WP_DISABLE_METHOD_DESCRIPTION},
      // RSU code page
      {"rsuCodePageTitleText", IDS_SHIMLESS_RMA_RSU_CODE_PAGE_TITLE},
      {"rsuCodeInstructionsText", IDS_SHIMLESS_RMA_RSU_CODE_INSTRUCTIONS},
      {"rsuChallengeDialogTitleText",
       IDS_SHIMLESS_RMA_RSU_CHALLENGE_DIALOG_TITLE},
      {"rsuCodeLabelText", IDS_SHIMLESS_RMA_RSU_CODE_LABEL},
      {"rsuChallengeDialogDoneButtonLabel",
       IDS_SHIMLESS_RMA_RSU_CHALLENGE_DIALOG_DONE_BUTTON},
      // Manual WP disable complete
      {"wpDisableCompletePageTitleText",
       IDS_SHIMLESS_RMA_WP_DISABLE_COMPLETE_PAGE_TITLE},
      {"wpDisableSkippedText", IDS_SHIMLESS_RMA_WP_DISABLE_SKIPPED_MESSAGE},
      {"wpDisableReassembleNowText",
       IDS_SHIMLESS_RMA_WP_DISABLE_REASSEMBLE_NOW_MESSAGE},
      {"wpDisableLeaveDisassembledText",
       IDS_SHIMLESS_RMA_WP_DISABLE_LEAVE_DISASSEMBLED_MESSAGE},
      // Check calibration page
      {"calibrationFailedTitleText",
       IDS_SHIMLESS_RMA_CALIBRATION_FAILED_PAGE_TITLE},
      {"calibrationFailedInstructionsText",
       IDS_SHIMLESS_RMA_CALIBRATION_FAILED_INSTRUCTIONS},
      {"calibrationFailedRetryButtonLabel",
       IDS_SHIMLESS_RMA_CALIBRATION_FAILED_RETRY_BUTTON_LABEL},
      {"calibrationFailedDialogTitle",
       IDS_SHIMLESS_RMA_CALIBRATION_FAILED_DIALOG_TITLE},
      {"calibrationFailedDialogText",
       IDS_SHIMLESS_RMA_CALIBRATION_FAILED_DIALOG_TEXT},
      {"calibrationFailedSkipCalibrationButtonLabel",
       IDS_SHIMLESS_RMA_CALIBRATION_FAILED_SKIP_CALIBRATION_LABEL},
      // Setup calibration page
      {"setupCalibrationTitleText",
       IDS_SHIMLESS_RMA_SETUP_CALIBRATION_PAGE_TITLE},
      {"calibrateBaseInstructionsText",
       IDS_SHIMLESS_RMA_BASE_CALIBRATION_INSTRUCTIONS},
      {"calibrateLidInstructionsText",
       IDS_SHIMLESS_RMA_LID_CALIBRATION_INSTRUCTIONS},
      // Finalize device  page
      {"finalizePageTitleText", IDS_SHIMLESS_RMA_FINALIZE_PAGE_TITLE},
      {"finalizePageProgressText", IDS_SHIMLESS_RMA_FINALIZE_PROGRESS},
      {"finalizePageCompleteText", IDS_SHIMLESS_RMA_FINALIZE_COMPLETE},
      {"finalizePageFailedBlockingText",
       IDS_SHIMLESS_RMA_FINALIZE_FAILED_BLOCKING},
      {"finalizePageFailedNonBlockingText",
       IDS_SHIMLESS_RMA_FINALIZE_FAILED_NON_BLOCKING},
      {"finalizePageFailedRetryButtonLabel",
       IDS_SHIMLESS_RMA_FINALIZE_FAILED_RETRY_BUTTON_LABEL},
      // Run calibration page
      {"runCalibrationTitleText", IDS_SHIMLESS_RMA_RUN_CALIBRATION_PAGE_TITLE},
      {"runCalibrationCompleteText", IDS_SHIMLESS_RMA_RUN_CALIBRATION_COMPLETE},
      {"runCalibrationStartingText", IDS_SHIMLESS_RMA_RUN_CALIBRATION_STARTING},
      {"runCalibrationCalibratingComponent",
       IDS_SHIMLESS_RMA_RUN_CALIBRATING_COMPONENT},
      // Device provisioning page
      {"provisioningPageTitleText", IDS_SHIMLESS_RMA_PROVISIONING_TITLE},
      {"provisioningPageProgressText", IDS_SHIMLESS_RMA_PROVISIONING_PROGRESS},
      {"provisioningPageCompleteText", IDS_SHIMLESS_RMA_PROVISIONING_COMPLETE},
      {"provisioningPageFailedBlockingText",
       IDS_SHIMLESS_RMA_PROVISIONING_FAILED_BLOCKING},
      {"provisioningPageFailedNonBlockingText",
       IDS_SHIMLESS_RMA_PROVISIONING_FAILED_NON_BLOCKING},
      {"provisioningPageFailedRetryButtonLabel",
       IDS_SHIMLESS_RMA_PROVISIONING_FAILED_RETRY_BUTTON_LABEL},
      // Repair complete page
      {"repairCompletedTitleText", IDS_SHIMLESS_RMA_REPAIR_COMPLETED},
      {"repairCompletedDescriptionText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_DESCRIPTION},
      {"repairCompletedDiagnosticsButtonText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_DIAGNOSTICS_BUTTON},
      {"repairCompletedDiagnosticsDescriptionText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_DIAGNOSTICS_DESCRIPTION},
      {"repairCompleteShutDownButtonText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_SHUT_DOWN_BUTTON_LABEL},
      {"repairCompleteRebootButtonText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_REBOOT_BUTTON_LABEL},
      {"repairCompletedLogsButtonText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_LOGS_BUTTON},
      {"repairCompletedLogsDescriptionText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_LOGS_DESCRIPTION},
      {"repairCompletedShutoffButtonText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_BATTERY_CUTOFF_BUTTON},
      {"repairCompletedShutoffDescriptionText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_BATTERY_CUTOFF_DESCRIPTION},
      {"repairCompletedShutoffInstructionsText",
       IDS_SHIMLESS_RMA_REPAIR_COMPLETED_BATTERY_CUTOFF_INSTRUCTIONS},
      {"rmaLogsTitleText", IDS_SHIMLESS_RMA_LOGS_TITLE},
      {"rmaLogsCancelButtonText", IDS_SHIMLESS_RMA_LOGS_CANCEL_BUTTON},
      {"rmaLogsSaveToUsbButtonText", IDS_SHIMLESS_RMA_LOGS_SAVE_BUTTON},
      {"batteryShutoffTooltipText", IDS_SHIMLESS_BATTERY_SHUTOFF_TOOLTIP_TEXT},

      // Manual disable wp page
      {"manuallyDisableWpTitleText",
       IDS_SHIMLESS_RMA_MANUALLY_DISABLE_WP_TITLE},
      {"manuallyDisableWpInstructionsText",
       IDS_SHIMLESS_RMA_MANUALLY_DISABLE_WP_INSTRUCTIONS},
      // Restock mainboard page
      {"restockTitleText", IDS_SHIMLESS_RMA_RESTOCK_PAGE_TITLE},
      {"restockInstructionsText", IDS_SHIMLESS_RMA_RESTOCK_INSTRUCTIONS},
      {"restockShutdownButtonText", IDS_SHIMLESS_RMA_RESTOCK_SHUTDOWN_BUTTON},
      {"restockContinueButtonText", IDS_SHIMLESS_RMA_RESTOCK_CONTINUE_BUTTON},
      // Manual enable wp page
      {"manuallyEnableWpTitleText", IDS_SHIMLESS_RMA_MANUALLY_ENABLE_WP_TITLE},
      {"manuallyEnableWpInstructionsText",
       IDS_SHIMLESS_RMA_MANUALLY_ENABLE_WP_INSTRUCTIONS},
      {"manuallyEnabledWpMessageText",
       IDS_SHIMLESS_RMA_MANUALLY_ENABLED_WP_MESSAGE},
      // Confirm device information page
      {"confirmDeviceInfoTitle", IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_TITLE},
      {"confirmDeviceInfoInstructions",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_INSTRUCTIONS},
      {"confirmDeviceInfoSerialNumberLabel",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_SERIAL_NUMBER_LABEL},
      {"confirmDeviceInfoRegionLabel",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_REGION_LABEL},
      {"confirmDeviceInfoWhiteLabelLabel",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_WHITE_LABEL_LABEL},
      {"confirmDeviceInfoEmptyWhiteLabelLabel",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_EMPTY_WHITE_LABEL_LABEL},
      {"confirmDeviceInfoDramPartNumberLabel",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DRAM_PART_NUMBER_LABEL},
      {"confirmDeviceInfoDramPartNumberPlaceholderLabel",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DRAM_PART_NUMBER_PLACEHOLDER_LABEL},
      {"confirmDeviceInfoSkuLabel",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_SKU_LABEL},
      {"confirmDeviceInfoResetButtonLabel",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_RESET_BUTTON_LABEL},
      {"confirmDeviceInfoSkuWarning",
       IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_SKU_WARNING},
      // Firmware reimaging page
      {"firmwareUpdatePageTitleText", IDS_SHIMLESS_RMA_FIRMWARE_UPDATE_TITLE},
      {"firmwareUpdateWaitForUsbText", IDS_SHIMLESS_RMA_FIRMWARE_WAIT_FOR_USB},
      {"firmwareUpdateFileNotFoundText",
       IDS_SHIMLESS_RMA_FIRMWARE_FILE_NOT_FOUND},
      {"firmwareUpdatingText", IDS_SHIMLESS_RMA_FIRMWARE_UPDATING},
      {"firmwareUpdateRebootText", IDS_SHIMLESS_RMA_FIRMWARE_REBOOT},
      {"firmwareUpdateCompleteText", IDS_SHIMLESS_RMA_FIRMWARE_UPDATE_COMPLETE},
      // Onboarding update page
      {"onboardingUpdateConnectToInternet",
       IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_CONNECT},
      {"onboardingUpdateProgress", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_PROGRESS},
      {"onboardingUpdateIdle", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_IDLE},
      {"onboardingUpdateChecking", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_CHECKING},
      {"onboardingUpdateAvailable",
       IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_AVAILABLE},
      {"onboardingUpdateDownloading",
       IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_DOWNLOADING},
      {"onboardingUpdateVerifying",
       IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_VERIFYING},
      {"onboardingUpdateFinalizing",
       IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_FINALIZING},
      {"onboardingUpdateReboot", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_REBOOT},
      {"onboardingUpdateError", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_ERROR},
      {"onboardingUpdateRollback", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_ROLLBACK},
      {"onboardingUpdateDisabled", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_DISABLED},
      {"onboardingUpdatePermission",
       IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_PERMISSION},
      // Critical error
      {"criticalErrorMessageText", IDS_SHIMLESS_RMA_CRITICAL_ERROR_MESSAGE},
      {"criticalErrorExitText", IDS_SHIMLESS_RMA_CRITICAL_EXIT_BUTTON},
      {"criticalErrorRebootText", IDS_SHIMLESS_RMA_CRITICAL_REBOOT_BUTTON},
      // Wipe device page
      {"wipeDeviceTitleText", IDS_SHIMLESS_RMA_WIPE_DEVICE_TITLE},
      {"wipeDeviceRemoveDataLabel",
       IDS_SHIMLESS_RMA_WIPE_DEVICE_REMOVE_DATA_OPTION},
      {"wipeDeviceRemoveDataDescription",
       IDS_SHIMLESS_RMA_WIPE_DEVICE_REMOVE_DATA_OPTION_DESCRIPTION},
      {"wipeDevicePreserveDataLabel",
       IDS_SHIMLESS_RMA_WIPE_DEVICE_PRESERVE_DATA_OPTION},
      {"wipeDevicePreserveDataDescription",
       IDS_SHIMLESS_RMA_WIPE_DEVICE_PRESERVE_DATA_OPTION_DESCRIPTION},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->UseStringsJs();
}

void AddDevicePlaceholderStrings(content::WebUIDataSource* html_source) {
  html_source->AddString(
      "shimlessSplashTitle",
      ui::SubstituteChromeOSDeviceType(IDS_SHIMLESS_RMA_SPLASH_TITLE));
  html_source->AddString(
      "welcomeTitleText",
      ui::SubstituteChromeOSDeviceType(IDS_SHIMLESS_RMA_LANDING_PAGE_TITLE));
  html_source->AddString(
      "manualWpDisableMethodDescriptionText",
      ui::SubstituteChromeOSDeviceType(
          IDS_SHIMLESS_RMA_MANUAL_WP_DISABLE_METHOD_DESCRIPTION));
  html_source->AddString(
      "criticalErrorTitleText",
      ui::SubstituteChromeOSDeviceType(IDS_SHIMLESS_RMA_CRITICAL_ERROR_TITLE));
}

}  // namespace

namespace shimless_rma {

/* static */
bool IsShimlessRmaAllowed() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  // Do not attempt to launch RMA in safe mode as RMA will prevent login, and
  // any option to attempt repairs.
  return ash::features::IsShimlessRMAFlowEnabled() &&
         !command_line.HasSwitch(switches::kRmaNotAllowed) &&
         !command_line.HasSwitch(switches::kSafeMode);
}

/* static */
bool HasLaunchRmaSwitchAndIsAllowed() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Do not attempt to launch RMA in safe mode as RMA will prevent login, and
  // any option to attempt repairs.
  const bool launch_rma_switch_detected =
      command_line.HasSwitch(switches::kLaunchRma);

  // Call IsShimlessRmaAllowed() to safe guard from launching Shimless RMA in
  // in the wrong state.
  return launch_rma_switch_detected && IsShimlessRmaAllowed();
}

}  // namespace shimless_rma

ShimlessRMADialogUI::ShimlessRMADialogUI(
    content::WebUI* web_ui,
    std::unique_ptr<shimless_rma::ShimlessRmaDelegate> shimless_rma_delegate)
    : ui::MojoWebDialogUI(web_ui),
      shimless_rma_manager_(std::make_unique<shimless_rma::ShimlessRmaService>(
          std::move(shimless_rma_delegate))) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIShimlessRMAHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  html_source->DisableTrustedTypesCSP();

  const auto resources =
      base::make_span(kAshShimlessRmaResources, kAshShimlessRmaResourcesSize);
  SetUpWebUIDataSource(html_source, resources, IDR_ASH_SHIMLESS_RMA_INDEX_HTML);

  AddShimlessRmaStrings(html_source);
  AddDevicePlaceholderStrings(html_source);

  ui::network_element::AddLocalizedStrings(html_source);
  ui::network_element::AddOncLocalizedStrings(html_source);
  ui::network_element::AddDetailsLocalizedStrings(html_source);
  ui::network_element::AddConfigLocalizedStrings(html_source);
  ui::network_element::AddErrorLocalizedStrings(html_source);
  html_source->UseStringsJs();
}

ShimlessRMADialogUI::~ShimlessRMADialogUI() = default;

void ShimlessRMADialogUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

void ShimlessRMADialogUI::BindInterface(
    mojo::PendingReceiver<shimless_rma::mojom::ShimlessRmaService> receiver) {
  DCHECK(shimless_rma_manager_);
  shimless_rma_manager_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ShimlessRMADialogUI)

}  // namespace ash