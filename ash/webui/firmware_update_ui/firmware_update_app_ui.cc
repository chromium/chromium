// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/firmware_update_ui/firmware_update_app_ui.h"

#include <memory>
#include <utility>

#include "ash/components/fwupd/firmware_update_manager.h"
#include "ash/grit/ash_firmware_update_app_resources.h"
#include "ash/grit/ash_firmware_update_app_resources_map.h"
#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

namespace {

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

void AddFirmwareUpdateAppStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appTitle", IDS_FIRMWARE_TITLE_TEXT},
      {"criticalUpdate", IDS_FIRMWARE_CRITICAL_UPDATE_TEXT},
      {"prepareDevice", IDS_FIRMWARE_PREPARE_DEVICE_TEXT},
      {"nextButton", IDS_FIRMWARE_NEXT_BUTTON_TEXT},
      {"cancelButton", IDS_FIRMWARE_CANCEL_BUTTON_TEXT},
      {"doneButton", IDS_FIRMWARE_DONE_BUTTON_TEXT},
      {"updateButton", IDS_FIRMWARE_UPDATE_BUTTON_TEXT},
      {"updating", IDS_FIRMWARE_UPDATING_TEXT},
      {"deviceUpToDate", IDS_FIRMWARE_DEVICE_UP_TO_DATE_TEXT},
      {"hasBeenUpdated", IDS_FIRMWARE_HAS_BEEN_UPDATED_TEXT},
      {"updatingInfo", IDS_FIRMWARE_UPDATING_INFO_TEXT},
      {"installing", IDS_FIRMWARE_INSTALLING_TEXT},
      {"upToDate", IDS_FIRMWARE_UP_TO_DATE_TEXT},
      {"versionText", IDS_FIRMWARE_VERSION_TEXT}};

  source->AddLocalizedStrings(kLocalizedStrings);
  source->UseStringsJs();
}

}  // namespace

FirmwareUpdateAppUI::FirmwareUpdateAppUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  auto source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIFirmwareUpdateAppHost));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  source->DisableTrustedTypesCSP();

  const auto resources = base::make_span(kAshFirmwareUpdateAppResources,
                                         kAshFirmwareUpdateAppResourcesSize);
  SetUpWebUIDataSource(source.get(), resources,
                       IDR_ASH_FIRMWARE_UPDATE_APP_INDEX_HTML);

  AddFirmwareUpdateAppStrings(source.get());

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

FirmwareUpdateAppUI::~FirmwareUpdateAppUI() = default;

void FirmwareUpdateAppUI::BindInterface(
    mojo::PendingReceiver<firmware_update::mojom::UpdateProvider> receiver) {
  FirmwareUpdateManager::Get()->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FirmwareUpdateAppUI)
}  // namespace ash