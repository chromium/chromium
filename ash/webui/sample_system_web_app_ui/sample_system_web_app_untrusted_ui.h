// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UNTRUSTED_UI_H_
#define ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UNTRUSTED_UI_H_

#include "ash/webui/sample_system_web_app_ui/mojom/sample_system_web_app_untrusted_ui.mojom.h"
#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

#if defined(OFFICIAL_BUILD)
#error Sample System Web App should only be included in unofficial builds.
#endif

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class SampleSystemWebAppUntrustedUI;

class SampleSystemWebAppUntrustedUIConfig
    : public SystemWebAppUntrustedUIConfig<SampleSystemWebAppUntrustedUI> {
 public:
  SampleSystemWebAppUntrustedUIConfig()
      : SystemWebAppUntrustedUIConfig(kChromeUISampleSystemWebAppUntrustedHost,
                                      SystemWebAppType::SAMPLE) {}
};

class SampleSystemWebAppUntrustedUI
    : public ui::UntrustedWebUIController,
      public mojom::sample_swa::UntrustedPageInterfacesFactory {
 public:
  explicit SampleSystemWebAppUntrustedUI(content::WebUI* web_ui);
  SampleSystemWebAppUntrustedUI(const SampleSystemWebAppUntrustedUI&) = delete;
  SampleSystemWebAppUntrustedUI& operator=(
      const SampleSystemWebAppUntrustedUI&) = delete;
  ~SampleSystemWebAppUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::sample_swa::UntrustedPageInterfacesFactory>
          factory);

 private:
  // mojom::sample_swa::UntrustedPageInterfacesFactory
  void CreateParentPage(
      mojo::PendingRemote<mojom::sample_swa::ChildUntrustedPage>
          child_untrusted_page,
      mojo::PendingReceiver<mojom::sample_swa::ParentTrustedPage>
          parent_trusted_page) override;

  mojo::Receiver<mojom::sample_swa::UntrustedPageInterfacesFactory>
      untrusted_page_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_SYSTEM_WEB_APP_UNTRUSTED_UI_H_
