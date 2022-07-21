// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_UNTRUSTED_SAMPLE_SYSTEM_WEB_APP_UI_H_
#define ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_UNTRUSTED_SAMPLE_SYSTEM_WEB_APP_UI_H_

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

class UntrustedSampleSystemWebAppUI;

class SampleSystemWebAppUntrustedUIConfig
    : public SystemWebAppUntrustedUIConfig<UntrustedSampleSystemWebAppUI> {
 public:
  SampleSystemWebAppUntrustedUIConfig()
      : SystemWebAppUntrustedUIConfig(kChromeUIUntrustedSampleSystemWebAppHost,
                                      SystemWebAppType::SAMPLE) {}
};

// TODO(crbug.com/1345846): Rename to SampleSystemWebAppUntrustedUI.
class UntrustedSampleSystemWebAppUI
    : public ui::UntrustedWebUIController,
      public mojom::sample_swa::UntrustedPageInterfacesFactory {
 public:
  explicit UntrustedSampleSystemWebAppUI(content::WebUI* web_ui);
  UntrustedSampleSystemWebAppUI(const UntrustedSampleSystemWebAppUI&) = delete;
  UntrustedSampleSystemWebAppUI& operator=(
      const UntrustedSampleSystemWebAppUI&) = delete;
  ~UntrustedSampleSystemWebAppUI() override;

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

#endif  // ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_UNTRUSTED_SAMPLE_SYSTEM_WEB_APP_UI_H_
