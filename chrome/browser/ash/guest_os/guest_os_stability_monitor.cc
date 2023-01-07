// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"

namespace guest_os {

GuestOsStabilityMonitor::GuestOsStabilityMonitor(const std::string& histogram)
    : histogram_(histogram),
      concierge_observer_(this),
      cicerone_observer_(this),
      seneschal_observer_(this),
      chunneld_observer_(this) {
  auto* concierge_client = ash::ConciergeClient::Get();
  DCHECK(concierge_client);
  concierge_client->WaitForServiceToBeAvailable(
      base::BindOnce(&GuestOsStabilityMonitor::ConciergeStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* cicerone_client = ash::CiceroneClient::Get();
  DCHECK(cicerone_client);
  cicerone_client->WaitForServiceToBeAvailable(
      base::BindOnce(&GuestOsStabilityMonitor::CiceroneStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* seneschal_client = ash::SeneschalClient::Get();
  DCHECK(seneschal_client);
  seneschal_client->WaitForServiceToBeAvailable(
      base::BindOnce(&GuestOsStabilityMonitor::SeneschalStarted,
                     weak_ptr_factory_.GetWeakPtr()));

  auto* chunneld_client = ash::ChunneldClient::Get();
  DCHECK(chunneld_client);
  chunneld_client->WaitForServiceToBeAvailable(
      base::BindOnce(&GuestOsStabilityMonitor::ChunneldStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

GuestOsStabilityMonitor::~GuestOsStabilityMonitor() {}

void GuestOsStabilityMonitor::ConciergeStarted(bool is_available) {
  DCHECK(is_available);

  auto* concierge_client = ash::ConciergeClient::Get();
  DCHECK(concierge_client);
  concierge_observer_.Observe(concierge_client);
}

void GuestOsStabilityMonitor::CiceroneStarted(bool is_available) {
  DCHECK(is_available);

  auto* cicerone_client = ash::CiceroneClient::Get();
  DCHECK(cicerone_client);
  cicerone_observer_.Observe(cicerone_client);
}

void GuestOsStabilityMonitor::SeneschalStarted(bool is_available) {
  DCHECK(is_available);

  auto* seneschal_client = ash::SeneschalClient::Get();
  DCHECK(seneschal_client);
  seneschal_observer_.Observe(seneschal_client);
}

void GuestOsStabilityMonitor::ChunneldStarted(bool is_available) {
  DCHECK(is_available);

  auto* chunneld_client = ash::ChunneldClient::Get();
  DCHECK(chunneld_client);
  chunneld_observer_.Observe(chunneld_client);
}

void GuestOsStabilityMonitor::ConciergeServiceStopped() {
  base::UmaHistogramEnumeration(histogram_, FailureClasses::ConciergeStopped);
}
void GuestOsStabilityMonitor::ConciergeServiceStarted() {}

void GuestOsStabilityMonitor::CiceroneServiceStopped() {
  base::UmaHistogramEnumeration(histogram_, FailureClasses::CiceroneStopped);
}
void GuestOsStabilityMonitor::CiceroneServiceStarted() {}

void GuestOsStabilityMonitor::SeneschalServiceStopped() {
  base::UmaHistogramEnumeration(histogram_, FailureClasses::SeneschalStopped);
}
void GuestOsStabilityMonitor::SeneschalServiceStarted() {}

void GuestOsStabilityMonitor::ChunneldServiceStopped() {
  base::UmaHistogramEnumeration(histogram_, FailureClasses::ChunneldStopped);
}
void GuestOsStabilityMonitor::ChunneldServiceStarted() {}

void GuestOsStabilityMonitor::LogUnexpectedVmShutdown() {
  base::UmaHistogramEnumeration(histogram_, FailureClasses::VmStopped);
}

}  // namespace guest_os
