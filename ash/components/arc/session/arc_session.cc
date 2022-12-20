// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_session.h"

#include "ash/components/arc/session/arc_session_impl.h"

namespace arc {

ArcSession::ArcSession() = default;
ArcSession::~ArcSession() = default;

void ArcSession::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ArcSession::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

// static
std::unique_ptr<ArcSession> ArcSession::Create(
    ArcBridgeService* arc_bridge_service,
    version_info::Channel channel,
    ash::SchedulerConfigurationManagerBase* scheduler_configuration_manager,
    AdbSideloadingAvailabilityDelegate* adb_sideloading_availability_delegate) {
  return std::make_unique<ArcSessionImpl>(
      ArcSessionImpl::CreateDelegate(arc_bridge_service, channel),
      scheduler_configuration_manager, adb_sideloading_availability_delegate);
}

}  // namespace arc
