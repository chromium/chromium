// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/recorder_app_ui/recorder_app_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/recorder_app_ui/resources/grit/recorder_app_resources.h"
#include "ash/webui/recorder_app_ui/resources/grit/recorder_app_resources_map.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "content/public/browser/on_device_model_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

bool RecorderAppUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  if (!base::FeatureList::IsEnabled(ash::features::kConch)) {
    return false;
  }

  return ash::switches::IsConchSecretKeyMatched();
}

RecorderAppUIConfig::RecorderAppUIConfig()
    : WebUIConfig(content::kChromeUIScheme, ash::kChromeUIRecorderAppHost) {}

RecorderAppUIConfig::~RecorderAppUIConfig() = default;

std::unique_ptr<content::WebUIController>
RecorderAppUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                           const GURL& url) {
  return std::make_unique<RecorderAppUI>(web_ui);
}

RecorderAppUI::RecorderAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // See go/cros-conch-key for the key
  // Add it to /etc/chrome_dev.conf:
  //  --conch-key="INSERT KEY HERE"
  //  --enable-features=Conch
  CHECK(ash::switches::IsConchSecretKeyMatched());

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUIRecorderAppURL));
  allowlist->RegisterAutoGrantedPermission(
      host_origin, ContentSettingsType::MEDIASTREAM_MIC);
  // TODO(pihsun): Auto grant other needed permission.

  // Setup the data source
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIRecorderAppHost);

  source->AddResourcePaths(
      base::make_span(kRecorderAppResources, kRecorderAppResourcesSize));

  // TODO(pihsun): See if there's a better way to handle client side
  // navigation.
  source->AddResourcePath("", IDR_STATIC_INDEX_HTML);
  source->AddResourcePath("playback", IDR_STATIC_INDEX_HTML);
  source->AddResourcePath("record", IDR_STATIC_INDEX_HTML);
  source->AddResourcePath("dev", IDR_STATIC_INDEX_HTML);

  ash::EnableTrustedTypesCSP(source);
  // TODO(pihsun): Add other needed CSP.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc,
      std::string("media-src 'self' blob:;"));
}

RecorderAppUI::~RecorderAppUI() = default;

void RecorderAppUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void RecorderAppUI::BindInterface(
    mojo::PendingReceiver<recorder_app::mojom::PageHandler> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  page_receivers_.Add(this, std::move(receiver));
}

on_device_model::mojom::OnDeviceModelService&
RecorderAppUI::GetOnDeviceModelService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& remote = content::GetRemoteOnDeviceModelService();
  CHECK(remote);
  return *remote;
}

void RecorderAppUI::LoadModel(
    const base::Uuid& uuid,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetOnDeviceModelService().LoadPlatformModel(uuid, std::move(model),
                                              std::move(callback));
}

WEB_UI_CONTROLLER_TYPE_IMPL(RecorderAppUI)

}  // namespace ash
