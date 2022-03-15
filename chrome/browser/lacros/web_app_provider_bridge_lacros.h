// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_WEB_APP_PROVIDER_BRIDGE_LACROS_H_
#define CHROME_BROWSER_LACROS_WEB_APP_PROVIDER_BRIDGE_LACROS_H_

#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {

// Created in lacros-chrome. Allows ash-chrome to modify web app state in
// lacros-chrome.
class WebAppProviderBridgeLacros : public mojom::WebAppProviderBridge {
 public:
  WebAppProviderBridgeLacros();
  WebAppProviderBridgeLacros(const WebAppProviderBridgeLacros&) = delete;
  WebAppProviderBridgeLacros& operator=(const WebAppProviderBridgeLacros&) =
      delete;
  ~WebAppProviderBridgeLacros() override;

  // mojom::WebAppProviderBridge overrides:
  void WebAppInstalledInArc(mojom::ArcWebAppInstallInfoPtr info,
                            WebAppInstalledInArcCallback callback) override;
  void WebAppUninstalledInArc(const std::string& app_id,
                              WebAppUninstalledInArcCallback callback) override;

 private:
  mojo::Receiver<mojom::WebAppProviderBridge> receiver_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_WEB_APP_PROVIDER_BRIDGE_LACROS_H_
