// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/guest_os/guest_os_stability_monitor.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace guest_os {

const char kCrostiniStabilityHistogram[] = "Crostini.Stability";

GuestOsStabilityMonitor::GuestOsStabilityMonitor(
    crostini::CrostiniManager* crostini_manager)
    : concierge_observer_(this),
      cicerone_observer_(this),
      seneschal_observer_(this),
      chunneld_observer_(this),
      vm_stopped_observer_(this),
      crostini_manager_(crostini_manager->GetWeakPtr()) {
  auto* concierge_client =
      chromeos::DBusThreadManager::Get()->GetConciergeClient();
  DCHECK(concierge_client);
  concierge_client->WaitForServiceToBeAvailable(
      base::BindOnce(&GuestOsStabilityMonitor::ConciergeStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* cicerone_client =
      chromeos::DBusThreadManager::Get()->GetCiceroneClient();
  DCHECK(cicerone_client);
  cicerone_client->WaitForServiceToBeAvailable(
      base::BindOnce(&GuestOsStabilityMonitor::CiceroneStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* seneschal_client =
      chromeos::DBusThreadManager::Get()->GetSeneschalClient();
  DCHECK(seneschal_client);
  seneschal_client->WaitForServiceToBeAvailable(
      base::BindOnce(&GuestOsStabilityMonitor::SeneschalStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* chunneld_client =
      chromeos::DBusThreadManager::Get()->GetChunneldClient();
  DCHECK(chunneld_client);
  chunneld_client->WaitForServiceToBeAvailable(
      base::BindOnce(&GuestOsStabilityMonitor::ChunneldStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  vm_stopped_observer_.Observe(crostini_manager);
}

GuestOsStabilityMonitor::~GuestOsStabilityMonitor() {}

void GuestOsStabilityMonitor::ConciergeStarted(bool is_available) {
  DCHECK(is_available);

  auto* concierge_client =
      chromeos::DBusThreadManager::Get()->GetConciergeClient();
  DCHECK(concierge_client);
  concierge_observer_.Observe(concierge_client);
}

void GuestOsStabilityMonitor::CiceroneStarted(bool is_available) {
  DCHECK(is_available);

  auto* cicerone_client =
      chromeos::DBusThreadManager::Get()->GetCiceroneClient();
  DCHECK(cicerone_client);
  cicerone_observer_.Observe(cicerone_client);
}

void GuestOsStabilityMonitor::SeneschalStarted(bool is_available) {
  DCHECK(is_available);

  auto* seneschal_client =
      chromeos::DBusThreadManager::Get()->GetSeneschalClient();
  DCHECK(seneschal_client);
  seneschal_observer_.Observe(seneschal_client);
}

void GuestOsStabilityMonitor::ChunneldStarted(bool is_available) {
  DCHECK(is_available);

  auto* chunneld_client =
      chromeos::DBusThreadManager::Get()->GetChunneldClient();
  DCHECK(chunneld_client);
  chunneld_observer_.Observe(chunneld_client);
}

void GuestOsStabilityMonitor::ConciergeServiceStopped() {
  base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                FailureClasses::ConciergeStopped);
}
void GuestOsStabilityMonitor::ConciergeServiceStarted() {}

void GuestOsStabilityMonitor::CiceroneServiceStopped() {
  base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                FailureClasses::CiceroneStopped);
}
void GuestOsStabilityMonitor::CiceroneServiceStarted() {}

void GuestOsStabilityMonitor::SeneschalServiceStopped() {
  base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                FailureClasses::SeneschalStopped);
}
void GuestOsStabilityMonitor::SeneschalServiceStarted() {}

void GuestOsStabilityMonitor::ChunneldServiceStopped() {
  base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                FailureClasses::ChunneldStopped);
}
void GuestOsStabilityMonitor::ChunneldServiceStarted() {}

void GuestOsStabilityMonitor::OnVmShutdown(const std::string& vm_name) {
  // CrostiniManager calls this observer method before removing the VM from its
  // tracking list, so this list will tell us what state the VM was believed to
  // be in before the stop signal was received.
  //
  // If it was STARTING then the error is tracked as a restart failure, not
  // here. If it was STOPPING then the stop was expected and not an error. If it
  // wasn't tracked by CrostiniManager, then we don't care what happens to it.
  //
  // So we can just ask if it was in the STARTED state with ::IsVmRunning.
  if (crostini_manager_->IsVmRunning(vm_name)) {
    base::UmaHistogramEnumeration(kCrostiniStabilityHistogram,
                                  FailureClasses::VmStopped);
  }
}

}  // namespace guest_os
