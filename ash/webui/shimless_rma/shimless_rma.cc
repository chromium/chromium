// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/shimless_rma/shimless_rma.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_shimless_rma_resources.h"
#include "ash/webui/grit/ash_shimless_rma_resources_map.h"
#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"
#include "ash/webui/shimless_rma/url_constants.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "build/branding_buildflags.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"
#include "ui/resources/grit/webui_resources.h"

namespace ash {

namespace {

// TODO(crbug.com/40673941): Replace with webui::SetUpWebUIDataSource() once it
// no longer requires a dependency on //chrome/browser.
void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->AddResourcePath("", default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
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
    {"componentLidAccelerometer", IDS_SHIMLESS_RMA_COMPONENT_LID_ACCELEROMETER},
    {"componentBaseGyroscope", IDS_SHIMLESS_RMA_COMPONENT_BASE_GYROSCOPE},
    {"componentLidGyroscope", IDS_SHIMLESS_RMA_COMPONENT_LID_GYROSCOPE},
    {"componentScreen", IDS_SHIMLESS_RMA_COMPONENT_SCREEN},
    {"componentKeyboard", IDS_SHIMLESS_RMA_COMPONENT_KEYBOARD},
    {"componentPowerButton", IDS_SHIMLESS_RMA_COMPONENT_POWER_BUTTON},
    // Splash screen
    {"shimlessSplashRemembering", IDS_SHIMLESS_RMA_SPLASH_REMEMBERING},
    // Common buttons
    {"exitButtonLabel", IDS_SHIMLESS_RMA_EXIT_BUTTON},
    {"backButtonLabel", IDS_SHIMLESS_RMA_BACK_BUTTON},
    {"nextButtonLabel", IDS_SHIMLESS_RMA_NEXT_BUTTON},
    {"skipButtonLabel", IDS_SHIMLESS_RMA_SKIP_BUTTON},
    {"okButtonLabel", IDS_SHIMLESS_RMA_OK_BUTTON},
    {"cancelButtonLabel", IDS_SHIMLESS_RMA_CANCEL_BUTTON},
    {"retryButtonLabel", IDS_SHIMLESS_RMA_RETRY_BUTTON},
    {"tryAgainButtonLabel", IDS_SHIMLESS_RMA_TRY_AGAIN_BUTTON},
    {"doneButtonLabel", IDS_SHIMLESS_RMA_DONE_BUTTON},
    {"installButtonLabel", IDS_SHIMLESS_RMA_INSTALL_BUTTON},
    {"acceptButtonLabel", IDS_SHIMLESS_RMA_ACCEPT_BUTTON},
    // Exit dialog
    {"exitDialogTitleText", IDS_SHIMLESS_RMA_EXIT_DIALOG_TITLE},
    {"exitDialogCancelButtonLabel",
     IDS_SHIMLESS_RMA_EXIT_DIALOG_CANCEL_BUTTON_LABEL},
    // Landing page
    {"beginRmaWarningText", IDS_SHIMLESS_RMA_AUTHORIZED_TECH_ONLY_WARNING},
    {"validatingComponentsText", IDS_SHIMLESS_RMA_VALIDATING_COMPONENTS},
    {"validatedComponentsSuccessText",
     IDS_SHIMLESS_RMA_VALIDATED_COMPONENTS_SUCCESS},
    {"validatedComponentsFailText", IDS_SHIMLESS_RMA_VALIDATED_COMPONENTS_FAIL},
    {"getStartedButtonLabel", IDS_SHIMLESS_RMA_GET_STARTED_BUTTON_LABEL},
    {"unqualifiedComponentsTitle",
     IDS_SHIMLESS_RMA_UNQUALIFIED_COMPONENTS_TITLE},
    // Network connect page
    {"connectNetworkTitleText", IDS_SHIMLESS_RMA_CONNECT_PAGE_TITLE},
    {"connectNetworkDescriptionText",
     IDS_SHIMLESS_RMA_CONNECT_PAGE_DESCRIPTION},
    {"connectNetworkDialogConnectButtonText",
     IDS_SHIMLESS_RMA_CONNECT_DIALOG_CONNECT},
    {"connectNetworkDialogDisconnectButtonText",
     IDS_SHIMLESS_RMA_CONNECT_DIALOG_DISCONNECT},
    {"connectNetworkDialogCancelButtonText",
     IDS_SHIMLESS_RMA_CONNECT_DIALOG_CANCEL},
    {"internetConfigName", IDS_SHIMLESS_RMA_CONNECT_DIALOG_CONFIG_NAME},
    {"internetJoinType", IDS_SHIMLESS_RMA_CONNECT_DIALOG_JOIN_TYPE},
    // Select components page
    {"selectComponentsTitleText",
     IDS_SHIMLESS_RMA_SELECT_COMPONENTS_PAGE_TITLE},
    {"undetectedComponentText", IDS_SHIMLESS_RMA_UNDETECTED_COMPONENT_LABEL},
    {"reworkFlowLinkText", IDS_SHIMLESS_RMA_REWORK_FLOW_LINK},
    // Choose destination page
    {"chooseDestinationTitleText", IDS_SHIMLESS_RMA_CHOOSE_DESTINATION},
    {"sameOwnerText", IDS_SHIMLESS_RMA_SAME_OWNER},
    {"newOwnerText", IDS_SHIMLESS_RMA_NEW_OWNER},
    {"newOwnerDescriptionText", IDS_SHIMLESS_RMA_NEW_OWNER_DESCRIPTION},
    {"notSureOwnerText", IDS_SHIMLESS_RMA_NOT_SURE_OWNER},
    // OS update page
    {"osUpdateTitleText", IDS_SHIMLESS_RMA_UPDATE_OS_PAGE_TITLE},
    {"osUpdateUnqualifiedComponentsTopText",
     IDS_SHIMLESS_RMA_UPDATE_OS_UNQUALIFIED_COMPONENTS_TOP},
    {"osUpdateUnqualifiedComponentsBottomText",
     IDS_SHIMLESS_RMA_UPDATE_OS_UNQUALIFIED_COMPONENTS_BOTTOM},
    {"osUpdateOutOfDateDescriptionText",
     IDS_SHIMLESS_RMA_UPDATE_OS_OUT_OF_DATE},
    {"currentVersionOutOfDateText",
     IDS_SHIMLESS_RMA_CURRENT_VERSION_OUT_OF_DATE},
    {"updateVersionRestartLabel", IDS_SHIMLESS_RMA_UPDATE_VERSION_AND_RESTART},
    {"updatingOsVersionText", IDS_SHIMLESS_RMA_UPDATING_OS_VERSION},
    {"updatingOsErrorMessage", IDS_SHIMLESS_RMA_UPDATE_OS_ERROR_MESSAGE},
    // Choose WP disable method page
    {"chooseWpDisableMethodPageTitleText",
     IDS_SHIMLESS_RMA_CHOOSE_WP_DISABLE_METHOD_PAGE_TITLE},
    {"manualWpDisableMethodDescriptionText",
     IDS_SHIMLESS_RMA_MANUAL_WP_DISABLE_METHOD_DESCRIPTION},
    {"manualWpDisableMethodOptionText",
     IDS_SHIMLESS_RMA_MANUAL_WP_DISABLE_METHOD_OPTION},
    {"rsuWpDisableMethodOptionText",
     IDS_SHIMLESS_RMA_RSU_WP_DISABLE_METHOD_OPTION},
    {"rsuWpDisableMethodDescriptionText",
     IDS_SHIMLESS_RMA_RSU_WP_DISABLE_METHOD_DESCRIPTION},
    // RSU code page
    {"rsuCodePageTitleText", IDS_SHIMLESS_RMA_RSU_CODE_PAGE_TITLE},
    {"rsuCodeInstructionsText", IDS_SHIMLESS_RMA_RSU_CODE_INSTRUCTIONS},
    {"rsuCodeInstructionsAriaText",
     IDS_SHIMLESS_RMA_RSU_CODE_INSTRUCTIONS_ARIA},
    {"rsuChallengeDialogTitleText",
     IDS_SHIMLESS_RMA_RSU_CHALLENGE_DIALOG_TITLE},
    {"rsuCodeLabelText", IDS_SHIMLESS_RMA_RSU_CODE_LABEL},
    {"rsuCodeErrorLabelText", IDS_SHIMLESS_RMA_RSU_CODE_ERROR_LABEL},
    {"rsuChallengeDialogDoneButtonLabel",
     IDS_SHIMLESS_RMA_RSU_CHALLENGE_DIALOG_DONE_BUTTON},
    // Manual WP disable complete
    {"wpDisableCompletePageTitleText",
     IDS_SHIMLESS_RMA_WP_DISABLE_COMPLETE_PAGE_TITLE},
    {"wpDisableReassembleNowText",
     IDS_SHIMLESS_RMA_WP_DISABLE_REASSEMBLE_NOW_MESSAGE},
    {"wpDisableLeaveDisassembledText",
     IDS_SHIMLESS_RMA_WP_DISABLE_LEAVE_DISASSEMBLED_MESSAGE},
    // Check calibration page
    {"calibrationFailedTitleText",
     IDS_SHIMLESS_RMA_CALIBRATION_FAILED_PAGE_TITLE},
    {"calibrationFailedInstructionsText",
     IDS_SHIMLESS_RMA_CALIBRATION_FAILED_INSTRUCTIONS},
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
    // Run calibration page
    {"runCalibrationTitleText", IDS_SHIMLESS_RMA_RUN_CALIBRATION_PAGE_TITLE},
    {"runCalibrationCompleteTitleText",
     IDS_SHIMLESS_RMA_RUN_CALIBRATION_COMPLETE_TITLE},
    // Device provisioning page
    {"provisioningPageTitleText", IDS_SHIMLESS_RMA_PROVISIONING_TITLE},
    {"provisioningPageWpEnabledDialogTitle",
     IDS_SHIMLESS_RMA_PROVISIONING_WP_ENABLED_DIALOG_TITLE},
    {"provisioningPageWpEnabledDialogBody",
     IDS_SHIMLESS_RMA_PROVISIONING_WP_ENABLED_DIALOG_BODY},
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
    {"rmaLogsMissingUsbMessageText",
     IDS_SHIMLESS_RMA_LOGS_MISSING_USB_MESSAGE_TEXT},
    {"repairCompletedPowerwashTitle",
     IDS_SHIMLESS_RMA_REPAIR_COMPLETED_POWERWASH_TITLE},
    {"repairCompletedPowerwashShutdownDescription",
     IDS_SHIMLESS_RMA_REPAIR_COMPLETED_POWERWASH_SHUTDOWN_DESCRIPTION},
    {"repairCompletedPowerwashRebootDescription",
     IDS_SHIMLESS_RMA_REPAIR_COMPLETED_POWERWASH_REBOOT_DESCRIPTION},
    {"repairCompletedPowerwashButton",
     IDS_SHIMLESS_RMA_REPAIR_COMPLETED_POWERWASH_BUTTON},
    {"repairCompletedBatteryCutoffCountdownDescription",
     IDS_SHIMLESS_RMA_REPAIR_COMPLETED_CUTOFF_COUNTDOWN_DESCRIPTION},
    {"repairCompletedBatteryCutoffShutdownButton",
     IDS_SHIMLESS_RMA_REPAIR_COMPLETED_BATTERY_CUTOFF_SHUTDOWN_BUTTON},
    {"rmaLogsSaveSuccessText", IDS_SHIMLESS_RMA_LOGS_SAVE_SUCCESS},
    {"rmaLogsSaveFailText", IDS_SHIMLESS_RMA_LOGS_SAVE_FAIL},
    {"rmaLogsSaveUsbNotFound", IDS_SHIMLESS_RMA_LOGS_SAVE_USB_NOT_FOUND},
    // Powerwash dialog
    {"powerwashDialogTitle", IDS_SHIMLESS_RMA_POWERWASH_DIALOG_TITLE},
    {"powerwashDialogShutdownDescription",
     IDS_SHIMLESS_RMA_POWERWASH_DIALOG_SHUTDOWN_DESCRIPTION},
    {"powerwashDialogRebootDescription",
     IDS_SHIMLESS_RMA_POWERWASH_DIALOG_REBOOT_DESCRIPTION},
    {"powerwashDialogPowerwashButton",
     IDS_SHIMLESS_RMA_POWERWASH_DIALOG_POWERWASH_BUTTON},

    // Manual disable wp page
    {"manuallyDisableWpTitleText", IDS_SHIMLESS_RMA_MANUALLY_DISABLE_WP_TITLE},
    {"manuallyDisableWpInstructionsText",
     IDS_SHIMLESS_RMA_MANUALLY_DISABLE_WP_INSTRUCTIONS},
    {"manuallyDisableWpTitleTextReboot",
     IDS_SHIMLESS_RMA_MANUALLY_DISABLE_WP_TITLE_REBOOT},
    {"manuallyDisableWpInstructionsTextReboot",
     IDS_SHIMLESS_RMA_MANUALLY_DISABLE_WP_INSTRUCTIONS_REBOOT},
    // Restock mainboard page
    {"restockInstructionsText", IDS_SHIMLESS_RMA_RESTOCK_INSTRUCTIONS},
    {"restockShutdownButtonText", IDS_SHIMLESS_RMA_RESTOCK_SHUTDOWN_BUTTON},
    {"restockContinueButtonText", IDS_SHIMLESS_RMA_RESTOCK_CONTINUE_BUTTON},
    {"restockTitleText", IDS_SHIMLESS_RMA_RESTOCK_PAGE_TITLE},
    // Manual enable wp page
    {"manuallyEnableWpTitleText", IDS_SHIMLESS_RMA_MANUALLY_ENABLE_WP_TITLE},
    {"manuallyEnableWpInstructionsText",
     IDS_SHIMLESS_RMA_MANUALLY_ENABLE_WP_INSTRUCTIONS},
    // Confirm device information page
    {"confirmDeviceInfoTitle", IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_TITLE},
    {"confirmDeviceInfoInstructions",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_INSTRUCTIONS},
    {"confirmDeviceInfoSerialNumberLabel",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_SERIAL_NUMBER_LABEL},
    {"confirmDeviceInfoRegionLabel",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_REGION_LABEL},
    {"confirmDeviceInfoCustomLabelLabel",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_WHITE_LABEL_LABEL},
    {"confirmDeviceInfoEmptyCustomLabelLabel",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_EMPTY_WHITE_LABEL_LABEL},
    {"confirmDeviceInfoDramPartNumberLabel",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DRAM_PART_NUMBER_LABEL},
    {"confirmDeviceInfoSkuLabel",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_SKU_LABEL},
    {"confirmDeviceInfoResetButtonLabel",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_REVERT_BUTTON_LABEL},
    {"confirmDeviceInfoSkuWarning",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_SKU_WARNING},
// Project Simon strings should not be displayed until the feature has been
// launched, so we use a BUILDFLAG to enable the internal-only strings when
// in a chrome-branded build, and enable the public strings when we're in a
// public build.
// The launch bug for this feature is http://launch/4259546.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"confirmDeviceInfoDeviceNotCompliant",
     IDR_ASH_SHIMLESS_RMA_PROJECT_SIMON_STRINGS_DEVICE_NOT_COMPLIANT_TXT},
    {"confirmDeviceInfoDeviceCompliant",
     IDR_ASH_SHIMLESS_RMA_PROJECT_SIMON_STRINGS_DEVICE_COMPLIANT_TXT},
    {"confirmDeviceInfoDeviceComplianceWarning",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DEVICE_COMPLIANCE_WARNING},
    {"confirmDeviceInfoDeviceQuestionIsBranded",
     IDR_ASH_SHIMLESS_RMA_PROJECT_SIMON_STRINGS_QUESTION_IS_BRANDED_TXT},
    {"confirmDeviceInfoDeviceQuestionDoesMeetRequirements",
     IDR_ASH_SHIMLESS_RMA_PROJECT_SIMON_STRINGS_QUESTION_DOES_MEET_REQUIREMENTS_TXT},
#else
    {"confirmDeviceInfoDeviceNotCompliant",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DEVICE_NOT_COMPLIANT},
    {"confirmDeviceInfoDeviceCompliant",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DEVICE_COMPLIANT},
    {"confirmDeviceInfoDeviceComplianceWarning",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DEVICE_COMPLIANCE_WARNING},
    {"confirmDeviceInfoDeviceQuestionIsBranded",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DEVICE_QUESTION_IS_BRANDED},
    {"confirmDeviceInfoDeviceQuestionDoesMeetRequirements",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DEVICE_QUESTION_DOES_MEET_REQUIREMENTS},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING);
    {"confirmDeviceInfoDeviceQuestionDoesMeetRequirementsTooltip",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_DEVICE_QUESTION_DOES_MEET_REQUIREMENTS_TOOLTIP},
    {"confirmDeviceInfoDeviceAnswerDefault",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_ANSWER_DEFAULT},
    {"confirmDeviceInfoDeviceAnswerNo",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_ANSWER_NO},
    {"confirmDeviceInfoDeviceAnswerYes",
     IDS_SHIMLESS_RMA_CONFIRM_DEVICE_INFO_ANSWER_YES},
    // Firmware reimaging page
    {"firmwareUpdateInstallImageTitleText",
     IDS_SHIMLESS_RMA_FIRMWARE_UPDATE_INSTALL_IMAGE_TITLE},
    {"firmwareUpdateInstallCompleteTitleText",
     IDS_SHIMLESS_RMA_FIRMWARE_UPDATE_INSTALL_COMPLETE_TITLE},
    {"firmwareUpdateWaitForUsbText", IDS_SHIMLESS_RMA_FIRMWARE_WAIT_FOR_USB},
    {"firmwareUpdateFileNotFoundText",
     IDS_SHIMLESS_RMA_FIRMWARE_FILE_NOT_FOUND},
    {"firmwareUpdatingText", IDS_SHIMLESS_RMA_FIRMWARE_UPDATING},
    {"firmwareUpdateRebootText", IDS_SHIMLESS_RMA_FIRMWARE_REBOOT},
    {"firmwareUpdateCompleteText", IDS_SHIMLESS_RMA_FIRMWARE_UPDATE_COMPLETE},
    // Onboarding update page
    {"onboardingUpdateProgress", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_PROGRESS},
    {"onboardingUpdateIdle", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_IDLE},
    {"onboardingUpdateChecking", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_CHECKING},
    {"onboardingUpdateAvailable", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_AVAILABLE},
    {"onboardingUpdateDownloading",
     IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_DOWNLOADING},
    {"onboardingUpdateVerifying", IDS_SHIMLESS_RMA_ONBOARDING_UPDATE_VERIFYING},
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
    {"criticalErrorRebootText", IDS_SHIMLESS_RMA_CRITICAL_REBOOT_BUTTON},
    // Hardware error
    {"hardwareErrorTitle", IDS_SHIMLESS_RMA_HARDWARE_ERROR_TITLE},
    {"hardwareErrorMessage", IDS_SHIMLESS_RMA_HARDWARE_ERROR_MESSAGE},
    {"hardwareErrorShutDownButton", IDS_SHIMLESS_RMA_HARDWARE_SHUTDOWN_BUTTON},
    {"hardwareErrorCode", IDS_SHIMLESS_RMA_HARDWARE_ERROR_CODE_MESSAGE},
    // Reboot page
    {"rebootPageTitle", IDS_SHIMLESS_RMA_REBOOT_PAGE_TITLE},
    {"rebootPageMessage", IDS_SHIMLESS_RMA_REBOOT_PAGE_MESSAGE},
    {"shutdownPageTitle", IDS_SHIMLESS_RMA_REBOOT_PAGE_SHUTDOWN_TITLE},
    {"shutdownPageMessage", IDS_SHIMLESS_RMA_REBOOT_PAGE_SHUTDOWN_MESSAGE},
    // Wipe device page
    {"wipeDeviceTitleText", IDS_SHIMLESS_RMA_WIPE_DEVICE_TITLE},
    {"wipeDeviceRemoveDataLabel",
     IDS_SHIMLESS_RMA_WIPE_DEVICE_REMOVE_DATA_OPTION},
    {"wipeDeviceRemoveDataDescription",
     IDS_SHIMLESS_RMA_WIPE_DEVICE_REMOVE_DATA_OPTION_DESCRIPTION},
    {"wipeDevicePreserveDataLabel",
     IDS_SHIMLESS_RMA_WIPE_DEVICE_PRESERVE_DATA_OPTION},
    // Illustrations
    {"baseOnFlatSurfaceAltText",
     IDS_SHIMLESS_RMA_BASE_ON_FLAT_SURFACE_ALT_TEXT},
    {"downloadingAltText", IDS_SHIMLESS_RMA_DOWNLOADING_ALT_TEXT},
    {"errorAltText", IDS_SHIMLESS_RMA_ERROR_ALT_TEXT},
    {"insertUsbAltText", IDS_SHIMLESS_RMA_INSERT_USB_ALT_TEXT},
    {"lidOnFlatSurfaceAltText", IDS_SHIMLESS_RMA_LID_ON_FLAT_SURFACE_ALT_TEXT},
    {"repairStartAltText", IDS_SHIMLESS_RMA_REPAIR_START_ALT_TEXT},
    {"successAltText", IDS_SHIMLESS_RMA_SUCCESS_ALT_TEXT},
    {"updateOsAltText", IDS_SHIMLESS_RMA_UPDATE_OS_ALT_TEXT},
    // 3p diagnostics
    {"3pFindInstallalbeDialogTitle",
     IDS_SHIMLESS_RMA_3P_FIND_INSTALLABLE_DIALOG_TITLE},
    {"3pFindInstallalbeDialogMessage",
     IDS_SHIMLESS_RMA_3P_FIND_INSTALLABLE_DIALOG_MESSAGE},
    {"3pReviewPermissionDialogTitle",
     IDS_SHIMLESS_RMA_3P_REVIEW_PERMISSION_DIALOG_TITLE},
    {"3pReviewPermissionDialogMessagePrefix",
     IDS_SHIMLESS_RMA_3P_REVIEW_PERMISSION_DIALOG_MESSAGE_PREFIX},
    {"3pFailedToInstallDialogTitle",
     IDS_SHIMLESS_RMA_3P_FAILED_TO_INSTALL_DIALOG_TITLE},
    {"3pCheckWithOemDialogMessage",
     IDS_SHIMLESS_RMA_3P_CHECK_WITH_OEM_DIALOG_MESSAGE},
    {"3pNotInstalledDialogTitle",
     IDS_SHIMLESS_RMA_3P_NOT_INSTALLED_DIALOG_TITLE},
    {"3pFailedToLoadDialogTitle",
     IDS_SHIMLESS_RMA_3P_FAILED_TO_LOAD_DIALOG_TITLE},
    {"3pFailedToLoadDialogMessage",
     IDS_SHIMLESS_RMA_3P_FAILED_TO_LOAD_DIALOG_MESSAGE},
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
      "criticalErrorExitText",
      ui::SubstituteChromeOSDeviceType(IDS_SHIMLESS_RMA_CRITICAL_EXIT_BUTTON));
  html_source->AddString(
      "criticalErrorTitleText",
      ui::SubstituteChromeOSDeviceType(IDS_SHIMLESS_RMA_CRITICAL_ERROR_TITLE));
  html_source->AddString("exitDialogDescriptionText",
                         ui::SubstituteChromeOSDeviceType(
                             IDS_SHIMLESS_RMA_EXIT_DIALOG_DESCRIPTION));
}

void AddFeatureFlags(content::WebUIDataSource* html_source) {
  html_source->AddBoolean(
      "osUpdateEnabled",
      base::FeatureList::IsEnabled(features::kShimlessRMAOsUpdate));
  html_source->AddBoolean("3pDiagnosticsEnabled",
                          features::IsShimlessRMA3pDiagnosticsEnabled());
}

}  // namespace

namespace shimless_rma {

/* static */
bool IsShimlessRmaAllowed() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  // Do not attempt to launch RMA in safe mode as RMA will prevent login, and
  // any option to attempt repairs.
  return !command_line.HasSwitch(switches::kRmaNotAllowed) &&
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

ShimlessRMADialogUIConfig::ShimlessRMADialogUIConfig(
    CreateWebUIControllerFunc create_controller_func)
    : ChromeOSWebUIConfig(content::kChromeUIScheme,
                          ash::kChromeUIShimlessRMAHost,
                          create_controller_func) {}

bool ShimlessRMADialogUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return shimless_rma::HasLaunchRmaSwitchAndIsAllowed();
}

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
      "script-src chrome://resources chrome://webui-test 'self';");
  ash::EnableTrustedTypesCSP(html_source);

  const auto resources =
      base::make_span(kAshShimlessRmaResources, kAshShimlessRmaResourcesSize);
  SetUpWebUIDataSource(html_source, resources, IDR_ASH_SHIMLESS_RMA_INDEX_HTML);

  AddShimlessRmaStrings(html_source);
  AddDevicePlaceholderStrings(html_source);
  AddFeatureFlags(html_source);

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
