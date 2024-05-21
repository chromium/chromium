// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_EXTERNAL_DISPLAY_BRIGHTNESS_EXTERNAL_DISPLAY_BRIGHTNESS_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_EXTERNAL_DISPLAY_BRIGHTNESS_EXTERNAL_DISPLAY_BRIGHTNESS_SERVICE_H_

#include <optional>

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_adaptor.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/external_display_brightness.mojom-shared.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/external_display_brightness.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cfm {

class ExternalDisplayBrightnessService
    : public CfmObserver,
      public chromeos::cfm::ServiceAdaptor::Delegate,
      public chromeos::cfm::mojom::ExternalDisplayBrightness {
 public:
  ExternalDisplayBrightnessService(const ExternalDisplayBrightnessService&) =
      delete;
  ExternalDisplayBrightnessService& operator=(
      const ExternalDisplayBrightnessService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static ExternalDisplayBrightnessService* Get();
  static bool IsInitialized();

 protected:
  ExternalDisplayBrightnessService();
  ~ExternalDisplayBrightnessService() override;

  // CfmObserver implementation
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // chromeos::cfm::ServiceAdaptor::Delegate implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;
  void OnAdaptorConnect(bool success) override;
  void OnAdaptorDisconnect() override;

  // mojom::ExternalDisplayBrightness implementation
  void SetExternalDisplayALSBrightness(bool enabled) override;
  void GetExternalDisplayALSBrightness(
      GetExternalDisplayALSBrightnessCallback callback) override;
  void SetExternalDisplayBrightnessPercent(double percent) override;
  void GetExternalDisplayBrightnessPercent(
      GetExternalDisplayBrightnessPercentCallback callback) override;

  // Disconnect handler for |mojom:ExternalDisplayBrightness|
  virtual void OnMojoDisconnect();

 private:
  static void OnGetExternalDisplayALSBrightness(
      GetExternalDisplayALSBrightnessCallback callback,
      std::optional<bool> enabled);
  static void OnGetExternalDisplayBrightnessPercent(
      GetExternalDisplayBrightnessPercentCallback callback,
      std::optional<double> percent);

  chromeos::cfm::ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<chromeos::cfm::mojom::ExternalDisplayBrightness> receivers_;
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_EXTERNAL_DISPLAY_BRIGHTNESS_EXTERNAL_DISPLAY_BRIGHTNESS_SERVICE_H_
