// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/firmware_update_ui/firmware_update_app_ui.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "ash/webui/grit/ash_firmware_update_app_resources.h"
#include "ash/webui/grit/ash_firmware_update_app_resources_map.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom-forward.h"

namespace ash {

namespace {

void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddBoolean("isFirmwareUpdateUIV2Enabled",
                     ash::features::IsFirmwareUpdateUIV2Enabled());
  source->AddBoolean("isUpstreamTrustedReportsFirmwareEnabled",
                     ash::features::IsUpstreamTrustedReportsFirmwareEnabled());
}

void AddFirmwareUpdateAppStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appTitle", IDS_FIRMWARE_TITLE_TEXT},
      {"confirmationTitle", IDS_CONFIRMATION_TITLE_TEXT},
      {"criticalUpdate", IDS_FIRMWARE_CRITICAL_UPDATE_TEXT},
      {"prepareDevice", IDS_FIRMWARE_PREPARE_DEVICE_TEXT},
      {"nextButton", IDS_FIRMWARE_NEXT_BUTTON_TEXT},
      {"cancelButton", IDS_FIRMWARE_CANCEL_BUTTON_TEXT},
      {"doneButton", IDS_FIRMWARE_DONE_BUTTON_TEXT},
      {"numUpdatesText", IDS_FIRMWARE_NUM_AVAILABLE_UPDATES_TEXT},
      {"okButton", IDS_FIRMWARE_OK_BUTTON_TEXT},
      {"updateButton", IDS_FIRMWARE_UPDATE_BUTTON_TEXT},
      {"updateButtonA11yLabel", IDS_FIRMWARE_UPDATE_BUTTON_A11Y_LABEL},
      {"updateFailedBodyText", IDS_FIRMWARE_UPDATE_FAILED_BODY_TEXT},
      {"updateFailedTitleText", IDS_FIRMWARE_UPDATE_FAILED_TITLE_TEXT},
      {"updating", IDS_FIRMWARE_UPDATING_TEXT},
      {"deviceUpToDate", IDS_FIRMWARE_DEVICE_UP_TO_DATE_TEXT},
      {"deviceReadyToInstallUpdate",
       IDS_FIRMWARE_DEVICE_READY_TO_INSTALL_UPDATE_TEXT},
      {"deviceNeedsReboot", IDS_FIRMWARE_DEVICE_NEEDS_REBOOT_TEXT},
      {"hasBeenUpdated", IDS_FIRMWARE_HAS_BEEN_UPDATED_TEXT},
      {"updatingInfo", IDS_FIRMWARE_UPDATING_INFO_TEXT},
      {"installing", IDS_FIRMWARE_INSTALLING_TEXT},
      {"restartingBodyText", IDS_FIRMWARE_RESTARTING_BODY_TEXT},
      {"restartingFooterText", IDS_FIRMWARE_RESTARTING_FOOTER_TEXT},
      {"restartingTitleText", IDS_FIRMWARE_RESTARTING_TITLE_TEXT},
      {"waitingFooterText", IDS_FIRMWARE_WAITING_FOOTER_TEXT},
      {"upToDate", IDS_FIRMWARE_UP_TO_DATE_TEXT},
      {"versionText", IDS_FIRMWARE_VERSION_TEXT},
      {"proceedConfirmationText", IDS_FIRMWARE_PROCEED_UPDATE_CONFIRMATION},
      {"confirmationDisclaimer", IDS_FIRMWARE_CONFIRMATION_DISCLAIMER_TEXT},
      {"confirmationDisclaimerIconAriaLabel",
       IDS_FIRMWARE_CONFIRMATION_DISCLAIMER_ICON_ARIA_LABEL},
      {"requestIdRemoveReplug", IDS_FIRMWARE_REQUEST_ID_REMOVE_REPLUG},
      {"requestIdRemoveUsbCable", IDS_FIRMWARE_REQUEST_ID_REMOVE_USB_CABLE},
      {"requestIdInsertUsbCable", IDS_FIRMWARE_REQUEST_ID_INSERT_USB_CABLE},
      {"requestIdPressUnlock", IDS_FIRMWARE_REQUEST_ID_PRESS_UNLOCK},
      {"requestIdReplugInstall", IDS_FIRMWARE_REQUEST_ID_REPLUG_INSTALL},
      {"requestIdReplugPower", IDS_FIRMWARE_REQUEST_ID_REPLUG_POWER}};

  source->AddLocalizedStrings(kLocalizedStrings);
  source->AddString("requestIdDoNotPowerOff",
                    ui::SubstituteChromeOSDeviceType(
                        IDS_FIRMWARE_REQUEST_ID_DO_NOT_POWER_OFF));

  source->UseStringsJs();
}

}  // namespace

FirmwareUpdateAppUI::FirmwareUpdateAppUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIFirmwareUpdateAppHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  ash::EnableTrustedTypesCSP(source);

  const auto resources = base::make_span(kAshFirmwareUpdateAppResources,
                                         kAshFirmwareUpdateAppResourcesSize);
  SetUpWebUIDataSource(source, resources,
                       IDR_ASH_FIRMWARE_UPDATE_APP_INDEX_HTML);

  AddFirmwareUpdateAppStrings(source);
}

FirmwareUpdateAppUI::~FirmwareUpdateAppUI() = default;

void FirmwareUpdateAppUI::BindInterface(
    mojo::PendingReceiver<firmware_update::mojom::UpdateProvider> receiver) {
  if (FirmwareUpdateManager::IsInitialized()) {
    FirmwareUpdateManager::Get()->BindInterface(std::move(receiver));
  }
}

void FirmwareUpdateAppUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FirmwareUpdateAppUI)
}  // namespace ash
