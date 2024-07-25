// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_DIAGNOSTICS_DIAGNOSTICS_SERVICE_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_DIAGNOSTICS_DIAGNOSTICS_SERVICE_H_

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_adaptor.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/meet_devices_diagnostics.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::cfm {

// Implementation of the MeetDevicesDiagnostics Service.
class DiagnosticsService : public CfmObserver,
                           public chromeos::cfm::ServiceAdaptor::Delegate,
                           public chromeos::cfm::mojom::MeetDevicesDiagnostics {
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

  // Disconnect handler for |chromeos::cfm::mojom::CfmServiceAdaptor|
  void OnAdaptorDisconnect() override;

  // Forward |chromeos::cfm::ServiceAdaptor::Delegate| implementation
  void OnBindService(mojo::ScopedMessagePipeHandle receiver_pipe) override;

 private:
  DiagnosticsService();
  ~DiagnosticsService() override;

  chromeos::cfm::ServiceAdaptor service_adaptor_;
  mojo::ReceiverSet<chromeos::cfm::mojom::MeetDevicesDiagnostics> receivers_;
};

}  // namespace ash::cfm

#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_DIAGNOSTICS_DIAGNOSTICS_SERVICE_H_
