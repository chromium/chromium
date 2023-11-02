// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_STABILITY_MONITOR_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_STABILITY_MONITOR_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"

namespace guest_os {

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// GuestOsFailureClasses in src/tools/metrics/histograms/enums.xml and the copy
// in src/platform2/vm_tools/cicerone/crash_listener_impl.cc
enum class FailureClasses {
  ConciergeStopped = 0,
  CiceroneStopped = 1,
  SeneschalStopped = 2,
  ChunneldStopped = 3,
  VmStopped = 4,
  VmSyslogStopped = 5,
  VshdStopped = 6,
  LxcFsStopped = 7,
  TremplinStopped = 8,
  NdproxydStopped = 9,
  McastdStopped = 10,
  LxdStopped = 11,
  GarconStopped = 12,
  SommelierStopped = 13,
  SommelierXStopped = 14,
  CrosSftpStopped = 15,
  CrosNotificationdStopped = 16,
  kMaxValue = CrosNotificationdStopped,
};

// Logs host-side VM service failures, and unexpected VM shutdowns.
//
// Each implementing VM type (Crostini, Borealis, etc.) should create its own
// instance of this class, and keep it alive for as long as any VMs of that
// type are running. During the instance's lifetime, it will log any failures
// of concierge, cicerone, seneschal, or chunneld and log them under the
// provided histogram with a value from |FailureClasses|.
//
// Effectively, if any host service fails, *all* currently running VMs are
// blamed. Note this overattributes blame, so analyze results accordingly.
//
// Implementers should also listen for VmStopped events from concierge, and
// call |LogUnexpectedVmShutdown| if any are considered unexpected.
// Take care to ignore VMs owned by other implementers.
class GuestOsStabilityMonitor : ash::ConciergeClient::Observer,
                                ash::CiceroneClient::Observer,
                                ash::SeneschalClient::Observer,
                                ash::ChunneldClient::Observer {
 public:
  explicit GuestOsStabilityMonitor(const std::string& histogram);
  ~GuestOsStabilityMonitor() override;

  GuestOsStabilityMonitor(const GuestOsStabilityMonitor&) = delete;
  GuestOsStabilityMonitor& operator=(const GuestOsStabilityMonitor&) = delete;

  void LogUnexpectedVmShutdown();

  void ConciergeStarted(bool is_available);
  void CiceroneStarted(bool is_available);
  void SeneschalStarted(bool is_available);
  void ChunneldStarted(bool is_available);

  //  ash::ConciergeClient::Observer::
  void ConciergeServiceStopped() override;
  void ConciergeServiceStarted() override;

  //  ash::CiceroneClient::Observer::
  void CiceroneServiceStopped() override;
  void CiceroneServiceStarted() override;

  //  ash::SeneschalClient::Observer::
  void SeneschalServiceStopped() override;
  void SeneschalServiceStarted() override;

  //  ash::ChunneldClient::Observer::
  void ChunneldServiceStopped() override;
  void ChunneldServiceStarted() override;

 private:
  std::string histogram_;
  base::ScopedObservation<ash::ConciergeClient, ash::ConciergeClient::Observer>
      concierge_observer_;
  base::ScopedObservation<ash::CiceroneClient, ash::CiceroneClient::Observer>
      cicerone_observer_;
  base::ScopedObservation<ash::SeneschalClient, ash::SeneschalClient::Observer>
      seneschal_observer_;
  base::ScopedObservation<ash::ChunneldClient, ash::ChunneldClient::Observer>
      chunneld_observer_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GuestOsStabilityMonitor> weak_ptr_factory_{this};
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_STABILITY_MONITOR_H_
