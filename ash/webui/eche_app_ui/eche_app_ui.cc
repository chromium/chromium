// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_app_ui.h"

#include <memory>

#include "ash/webui/eche_app_ui/eche_app_manager.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "ash/webui/eche_app_ui/url_constants.h"
#include "ash/webui/grit/ash_eche_app_resources.h"
#include "ash/webui/grit/ash_eche_bundle_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/webui/webui_allowlist.h"

namespace ash::eche_app {

EcheAppUI::EcheAppUI(content::WebUI* web_ui, EcheAppManager* manager)
    : ui::MojoWebUIController(web_ui), manager_(manager) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(browser_context,
                                             kChromeUIEcheAppHost);

  html_source->AddResourcePath("", IDR_ASH_ECHE_INDEX_HTML);
  html_source->AddResourcePath("system_assets/app_icon_32.png",
                               IDR_ASH_ECHE_APP_ICON_32_PNG);
  html_source->AddResourcePath("system_assets/app_icon_256.png",
                               IDR_ASH_ECHE_APP_ICON_256_PNG);
  html_source->AddResourcePath("js/app_bundle.js", IDR_ASH_ECHE_APP_BUNDLE_JS);
  html_source->AddResourcePath("assets/app_bundle.css",
                               IDR_ASH_ECHE_APP_BUNDLE_CSS);
  html_source->AddResourcePath("big_buffer.mojom-lite.js",
                               IDR_MOJO_BIG_BUFFER_MOJOM_LITE_JS);
  html_source->AddResourcePath("string16.mojom-lite.js",
                               IDR_MOJO_STRING16_MOJOM_LITE_JS);
  html_source->AddResourcePath(
      "eche_app.mojom-lite.js",
      IDR_ASH_ECHE_APP_ASH_WEBUI_ECHE_APP_UI_MOJOM_ECHE_APP_MOJOM_LITE_JS);
  html_source->AddResourcePath("message_pipe.js",
                               IDR_ASH_ECHE_APP_MESSAGE_PIPE_JS);
  html_source->AddResourcePath("message_types.js",
                               IDR_ASH_ECHE_APP_MESSAGE_TYPES_JS);
  html_source->AddResourcePath("browser_proxy.js",
                               IDR_ASH_ECHE_APP_BROWSER_PROXY_JS);
  html_source->AddResourcePath("mojo_bindings_lite.js",
                               IDR_MOJO_MOJO_BINDINGS_LITE_JS);

  // DisableTrustedTypesCSP to support TrustedTypePolicy named 'goog#html'.
  // It is the Closure templating system that renders our UI, as it does many
  // other web apps using it.
  html_source->DisableTrustedTypesCSP();
  // The guest is in an <iframe>. Add it to CSP.
  std::string csp = std::string("frame-src ") + kChromeUIEcheAppGuestURL + ";";
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  // Add ability to request chrome-untrusted: URLs.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // Register common permissions for chrome-untrusted:// pages.
  // TODO(crbug.com/40710326): Remove this after common permissions are
  // granted by default.
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin untrusted_eche_app_origin =
      url::Origin::Create(GURL(kChromeUIEcheAppGuestURL));
  for (const auto& permission : {
           ContentSettingsType::COOKIES,
           ContentSettingsType::JAVASCRIPT,
           ContentSettingsType::IMAGES,
           ContentSettingsType::SOUND,
       }) {
    webui_allowlist->RegisterAutoGrantedPermission(untrusted_eche_app_origin,
                                                   permission);
  }

  // Set untrusted URL of Eche app in WebApp scope for allowing AutoPlay.
  auto* web_contents = web_ui->GetWebContents();
  auto prefs = web_contents->GetOrCreateWebPreferences();
  prefs.web_app_scope = GURL(kChromeUIEcheAppGuestURL);
  web_contents->SetWebPreferences(prefs);
}

EcheAppUI::~EcheAppUI() = default;

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::SignalingMessageExchanger> receiver) {
  if (manager_) {
    manager_->BindSignalingMessageExchangerInterface(std::move(receiver));
  }
}

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::SystemInfoProvider> receiver) {
  if (manager_) {
    manager_->BindSystemInfoProviderInterface(std::move(receiver));
  }
}

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::AccessibilityProvider> receiver) {
  if (manager_) {
    manager_->BindAccessibilityProviderInterface(std::move(receiver));
  }
}

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::UidGenerator> receiver) {
  if (manager_) {
    manager_->BindUidGeneratorInterface(std::move(receiver));
  }
}

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::NotificationGenerator> receiver) {
  if (manager_) {
    manager_->BindNotificationGeneratorInterface(std::move(receiver));
  }
}

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::DisplayStreamHandler> receiver) {
  if (manager_) {
    manager_->BindDisplayStreamHandlerInterface(std::move(receiver));
  }
}

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::StreamOrientationObserver> receiver) {
  if (manager_) {
    manager_->BindStreamOrientationObserverInterface(std::move(receiver));
  }
}

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::ConnectionStatusObserver> receiver) {
  if (manager_) {
    manager_->BindConnectionStatusObserverInterface(std::move(receiver));
  }
}

void EcheAppUI::BindInterface(
    mojo::PendingReceiver<mojom::KeyboardLayoutHandler> receiver) {
  if (manager_) {
    manager_->BindKeyboardLayoutHandlerInterface(std::move(receiver));
  }
}

WEB_UI_CONTROLLER_TYPE_IMPL(EcheAppUI)

}  // namespace ash::eche_app
