// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/dbus_test_helper.h"

#include "chromeos/ash/components/dbus/chunneld/fake_chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"

namespace guest_os {

FakeCiceroneHelper::FakeCiceroneHelper() {
  ash::CiceroneClient::InitializeFake();
}

FakeCiceroneHelper::~FakeCiceroneHelper() {
  ash::CiceroneClient::Shutdown();
}

ash::FakeCiceroneClient* FakeCiceroneHelper::FakeCiceroneClient() {
  return ash::FakeCiceroneClient::Get();
}

FakeSeneschalHelper::FakeSeneschalHelper() {
  ash::SeneschalClient::InitializeFake();
}

FakeSeneschalHelper::~FakeSeneschalHelper() {
  ash::SeneschalClient::Shutdown();
}

ash::FakeSeneschalClient* FakeSeneschalHelper::FakeSeneschalClient() {
  return ash::FakeSeneschalClient::Get();
}

FakeDlcserviceHelper::FakeDlcserviceHelper() {
  ash::DlcserviceClient::InitializeFake();
}

FakeDlcserviceHelper::~FakeDlcserviceHelper() {
  ash::DlcserviceClient::Shutdown();
}

ash::FakeDlcserviceClient* FakeDlcserviceHelper::FakeDlcserviceClient() {
  return static_cast<ash::FakeDlcserviceClient*>(ash::DlcserviceClient::Get());
}

FakeConciergeHelper::FakeConciergeHelper(FakeCiceroneHelper* cicerone_helper) {
  DCHECK(cicerone_helper);
  ash::ConciergeClient::InitializeFake();
}

FakeConciergeHelper::~FakeConciergeHelper() {
  ash::ConciergeClient::Shutdown();
}

ash::FakeConciergeClient* FakeConciergeHelper::FakeConciergeClient() {
  return ash::FakeConciergeClient::Get();
}

FakeChunneldHelper::FakeChunneldHelper() {
  ash::ChunneldClient::InitializeFake();
}

FakeChunneldHelper::~FakeChunneldHelper() {
  ash::ChunneldClient::Shutdown();
}

FakeVmServicesHelper::FakeVmServicesHelper() : FakeConciergeHelper(this) {}

}  // namespace guest_os
