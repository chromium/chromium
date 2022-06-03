// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_DIAGNOSTICS_DIAGNOSTICS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_DIAGNOSTICS_DIAGNOSTICS_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/chromebox_for_meetings/service_adaptor.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_diagnostics.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace cfm {

// Implementation of the MeetDevicesDiagnostics Service.
class DiagnosticsService : public CfmObserver,
                           public ServiceAdaptor::Delegate,
                           public mojom::MeetDevicesDiagnostics {
 public:
  DiagnosticsService(const DiagnosticsService&) = delete;
  DiagnosticsService& operator=(const DiagnosticsService&) = delete;

  // Manage singleton instance.
  static void Initialize();
  static void Shutdown();
  static DiagnosticsService* Get();
  static bool IsInitialized();

  // mojom::MeetDevicesDiagnostics overrides:
  void GetCrosHealthdTelemetry(
      GetCrosHealthdTelemetryCallback callback) override;
  void GetCrosHealthdProcessInfo(
      uint32_t pid,
      GetCrosHealthdProcessInfoCallback callback) override;

 protected:
  // Forward |CfmObserver| implementation
  bool ServiceRequestReceived(const std::string& interface_name) override;

  // Disconnect handler for |mojom::CfmServiceAdaptor|
  void OnAdaptorDisconnect() override;

  // Forward |ServiceAdaptorDelegate| implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;

 private:
  DiagnosticsService();
  ~DiagnosticsService() override;

  ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<mojom::MeetDevicesDiagnostics> receivers_;
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_DIAGNOSTICS_DIAGNOSTICS_SERVICE_H_
