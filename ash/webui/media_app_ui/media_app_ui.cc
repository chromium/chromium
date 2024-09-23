// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/media_app_ui.h"

#include <utility>

#include "ash/webui/grit/ash_media_app_resources.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/media_app_ui/media_app_page_handler.h"
#include "ash/webui/web_applications/webui_test_prod_util.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/extra_mojo_js_features.mojom.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {
namespace {

constexpr webui::LocalizedString kLocalizedStrings[] = {
    {"appTitle", IDS_MEDIA_APP_APP_NAME},
    {"fileFilterAudio", IDS_MEDIA_APP_FILE_FILTER_AUDIO},
    {"fileFilterImage", IDS_MEDIA_APP_FILE_FILTER_IMAGE},
    {"fileFilterVideo", IDS_MEDIA_APP_FILE_FILTER_VIDEO},
};

content::WebUIDataSource* CreateAndAddHostDataSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, kChromeUIMediaAppHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  // Add resources from ash_media_app_resources.pak.
  source->SetDefaultResource(IDR_MEDIA_APP_INDEX_DARK_LIGHT_HTML);
  source->AddResourcePath("launch.js", IDR_MEDIA_APP_LAUNCH_JS);
  source->AddResourcePath("viewpdfhost.html", IDR_MEDIA_APP_VIEWPDFHOST_HTML);
  source->AddResourcePath("viewpdfhost.js", IDR_MEDIA_APP_VIEWPDFHOST_JS);
  source->AddResourcePath("first_message_received.js",
                          IDR_MEDIA_APP_FIRST_MESSAGE_RECEIVED_JS);

  source->AddLocalizedStrings(kLocalizedStrings);
  source->UseStringsJs();

  // Redirects "system_assets/*" (from manifest.json) to the icons for the
  // gallery app.
  source->AddResourcePath("system_assets/app_icon_16.png",
                          IDR_MEDIA_APP_APP_ICON_16_PNG);
  source->AddResourcePath("system_assets/app_icon_32.png",
                          IDR_MEDIA_APP_APP_ICON_32_PNG);
  source->AddResourcePath("system_assets/app_icon_48.png",
                          IDR_MEDIA_APP_APP_ICON_48_PNG);
  source->AddResourcePath("system_assets/app_icon_64.png",
                          IDR_MEDIA_APP_APP_ICON_64_PNG);
  source->AddResourcePath("system_assets/app_icon_96.png",
                          IDR_MEDIA_APP_APP_ICON_96_PNG);
  source->AddResourcePath("system_assets/app_icon_128.png",
                          IDR_MEDIA_APP_APP_ICON_128_PNG);
  source->AddResourcePath("system_assets/app_icon_192.png",
                          IDR_MEDIA_APP_APP_ICON_192_PNG);
  source->AddResourcePath("system_assets/app_icon_256.png",
                          IDR_MEDIA_APP_APP_ICON_256_PNG);
  source->AddResourcePath("system_assets/app_icon.svg",
                          IDR_MEDIA_APP_APP_ICON_192_SVG);  // App favicon.

  // File-type favicons.
  source->AddResourcePath("system_assets/pdf_icon.svg",
                          IDR_MEDIA_APP_PDF_ICON_SVG);
  source->AddResourcePath("system_assets/pdf_icon_dark.svg",
                          IDR_MEDIA_APP_PDF_ICON_DARK_SVG);
  source->AddResourcePath("system_assets/video_icon.svg",
                          IDR_MEDIA_APP_VIDEO_ICON_SVG);
  source->AddResourcePath("system_assets/video_icon_dark.svg",
                          IDR_MEDIA_APP_VIDEO_ICON_DARK_SVG);
  source->AddResourcePath("system_assets/image_icon.svg",
                          IDR_MEDIA_APP_IMAGE_ICON_SVG);
  source->AddResourcePath("system_assets/image_icon_dark.svg",
                          IDR_MEDIA_APP_IMAGE_ICON_DARK_SVG);
  source->AddResourcePath("system_assets/audio_icon.svg",
                          IDR_MEDIA_APP_AUDIO_ICON_SVG);
  source->AddResourcePath("system_assets/audio_icon_dark.svg",
                          IDR_MEDIA_APP_AUDIO_ICON_DARK_SVG);
  source->AddResourcePath("system_assets/file_icon.svg",
                          IDR_MEDIA_APP_FILE_ICON_SVG);
  source->AddResourcePath("system_assets/file_icon_dark.svg",
                          IDR_MEDIA_APP_FILE_ICON_DARK_SVG);
  return source;
}

}  // namespace

MediaAppUI::MediaAppUI(content::WebUI* web_ui,
                       std::unique_ptr<MediaAppUIDelegate> delegate)
    : MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source =
      CreateAndAddHostDataSource(browser_context);

  // The guest is in an <iframe>. Add it to CSP.
  std::string csp = std::string("frame-src ") + kChromeUIMediaAppGuestURL + ";";
  host_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);
  // Allow use of SharedArrayBuffer (required by wasm code in the iframe guest).
  host_source->OverrideCrossOriginOpenerPolicy("same-origin");
  host_source->OverrideCrossOriginEmbedderPolicy("require-corp");

  if (MaybeConfigureTestableDataSource(host_source)) {
    host_source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::TrustedTypes,
        std::string("trusted-types test-harness;"));
  }

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin host_origin =
      url::Origin::Create(GURL(kChromeUIMediaAppURL));
  allowlist->RegisterAutoGrantedPermissions(
      host_origin, {
                       ContentSettingsType::COOKIES,
                       ContentSettingsType::FILE_SYSTEM_READ_GUARD,
                       ContentSettingsType::FILE_SYSTEM_WRITE_GUARD,
                       ContentSettingsType::IMAGES,
                       ContentSettingsType::JAVASCRIPT,
                       ContentSettingsType::SOUND,
                   });
  const url::Origin guest_origin =
      url::Origin::Create(GURL(kChromeUIMediaAppGuestURL));
  allowlist->RegisterAutoGrantedPermissions(guest_origin,
                                            {
                                                ContentSettingsType::COOKIES,
                                            });
  // Add ability to request chrome-untrusted: URLs.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

MediaAppUI::~MediaAppUI() = default;

void MediaAppUI::BindInterface(
    mojo::PendingReceiver<media_app_ui::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void MediaAppUI::WebUIRenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // Allow the render frame to get a MojoHandle from a FileSystemFileHandle.
  auto features = content::mojom::ExtraMojoJsFeatures::New();
  features->file_system_access = true;
  render_frame_host->EnableMojoJsBindings(std::move(features));
}

bool MediaAppUI::IsJavascriptErrorReportingEnabled() {
  // JavaScript errors are reported via CrashReportPrivate.reportError. Don't
  // send duplicate reports via WebUI.
  return false;
}

void MediaAppUI::CreatePageHandler(
    mojo::PendingReceiver<media_app_ui::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<MediaAppPageHandler>(this, std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(MediaAppUI)

}  // namespace ash
