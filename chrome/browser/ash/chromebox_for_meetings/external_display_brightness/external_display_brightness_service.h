// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_EXTERNAL_DISPLAY_BRIGHTNESS_EXTERNAL_DISPLAY_BRIGHTNESS_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_EXTERNAL_DISPLAY_BRIGHTNESS_EXTERNAL_DISPLAY_BRIGHTNESS_SERVICE_H_

#include "chrome/browser/ash/chromebox_for_meetings/service_adaptor.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/external_display_brightness.mojom-shared.h"
#include "chromeos/ash/services/chromebox_for_meetings/public/mojom/external_display_brightness.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::cfm {

class ExternalDisplayBrightnessService
    : public CfmObserver,
      public ServiceAdaptor::Delegate,
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

  // ServiceAdaptorDelegate implementation
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
      absl::optional<bool> enabled);
  static void OnGetExternalDisplayBrightnessPercent(
      GetExternalDisplayBrightnessPercentCallback callback,
      absl::optional<double> percent);

  ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<chromeos::cfm::mojom::ExternalDisplayBrightness> receivers_;
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_EXTERNAL_DISPLAY_BRIGHTNESS_EXTERNAL_DISPLAY_BRIGHTNESS_SERVICE_H_
