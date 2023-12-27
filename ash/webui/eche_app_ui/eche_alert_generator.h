// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_ALERT_GENERATOR_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_ALERT_GENERATOR_H_

#include <string>

#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrefService;

namespace ash {
namespace eche_app {

class LaunchAppHelper;

extern const char kEcheAppScreenLockNotifierId[];
extern const char kEcheAppRetryConnectionNotifierId[];
extern const char kEcheAppInactivityNotifierId[];
extern const char kEcheAppFromWebWithoutButtonNotifierId[];
extern const char kEcheAppLearnMoreUrl[];
extern const char kEcheAppToastId[];
extern const char kEcheAppNetworkSettingNotifierId[];

// Implements the ShowNotification interface to allow WebUI show the native
// notification and toast.
class EcheAlertGenerator : public mojom::NotificationGenerator {
 public:
  explicit EcheAlertGenerator(LaunchAppHelper* launch_app_helper,
                              PrefService* pref_service);
  ~EcheAlertGenerator() override;

  EcheAlertGenerator(const EcheAlertGenerator&) = delete;
  EcheAlertGenerator& operator=(const EcheAlertGenerator&) = delete;

  // mojom::NotificationGenerator:
  void ShowNotification(const std::u16string& title,
                        const std::u16string& message,
                        mojom::WebNotificationType type) override;
  void ShowToast(const std::u16string& text) override;

  void Bind(mojo::PendingReceiver<mojom::NotificationGenerator> receiver);

 private:
  void OnEnableScreenLockChanged();

  mojo::Receiver<mojom::NotificationGenerator> notification_receiver_{this};
  raw_ptr<LaunchAppHelper, DanglingUntriaged> launch_app_helper_;
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_ALERT_GENERATOR_H_
