// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_STANDALONE_BROWSER_TEST_CONTROLLER_H_
#define CHROME_BROWSER_LACROS_STANDALONE_BROWSER_TEST_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/crosapi/mojom/tts.mojom-forward.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/install_result_code.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// Created in lacros-chrome and registered with ash-chrome's test controller
// over crosapi to let the Ash browser tests that require Lacros to send
// commands to this lacros-chrome instance.
class StandaloneBrowserTestController
    : public crosapi::mojom::StandaloneBrowserTestController {
 public:
  explicit StandaloneBrowserTestController(
      mojo::Remote<crosapi::mojom::TestController>& test_controller);
  ~StandaloneBrowserTestController() override;

  void InstallWebApp(const std::string& start_url,
                     apps::WindowMode window_mode,
                     InstallWebAppCallback callback) override;

  void LoadVpnExtension(const std::string& extension_name,
                        LoadVpnExtensionCallback callback) override;

  void GetTtsVoices(GetTtsVoicesCallback callback) override;

  void GetExtensionKeeplist(GetExtensionKeeplistCallback callback) override;

  void TtsSpeak(crosapi::mojom::TtsUtterancePtr mojo_utterance,
                mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient>
                    utterance_client) override;

 private:
  class LacrosUtteranceEventDelegate;

  void OnUtteranceFinished(int utterance_id);
  void WebAppInstallationDone(InstallWebAppCallback callback,
                              const web_app::AppId& installed_app_id,
                              webapps::InstallResultCode code);

  base::Value::Dict CreateVpnExtensionManifest(
      const std::string& extension_name);

  mojo::Receiver<crosapi::mojom::StandaloneBrowserTestController>
      controller_receiver_{this};

  // Lacros utterance event delegates by utterance id.
  std::map<int, std::unique_ptr<LacrosUtteranceEventDelegate>>
      lacros_utterance_event_delegates_;

  base::WeakPtrFactory<StandaloneBrowserTestController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_STANDALONE_BROWSER_TEST_CONTROLLER_H_
