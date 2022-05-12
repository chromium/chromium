// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/testing/dbus.h"

#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/fake_dlcservice_client.h"

namespace borealis {

BasicDBusHelper::BasicDBusHelper() {
  chromeos::DBusThreadManager::Initialize();
}

BasicDBusHelper::~BasicDBusHelper() {
  chromeos::DBusThreadManager::Shutdown();
}

FakeCiceroneHelper::FakeCiceroneHelper(BasicDBusHelper* basic_helper) {
  DCHECK(basic_helper);
  ash::CiceroneClient::InitializeFake();
}

FakeCiceroneHelper::~FakeCiceroneHelper() {
  ash::CiceroneClient::Shutdown();
}

ash::FakeCiceroneClient* FakeCiceroneHelper::FakeCiceroneClient() {
  return ash::FakeCiceroneClient::Get();
}

FakeSeneschalHelper::FakeSeneschalHelper(BasicDBusHelper* basic_helper) {
  DCHECK(basic_helper);
  ash::SeneschalClient::InitializeFake();
}

FakeSeneschalHelper::~FakeSeneschalHelper() {
  ash::SeneschalClient::Shutdown();
}

ash::FakeSeneschalClient* FakeSeneschalHelper::FakeSeneschalClient() {
  return ash::FakeSeneschalClient::Get();
}

FakeDlcserviceHelper::FakeDlcserviceHelper(BasicDBusHelper* basic_helper) {
  DCHECK(basic_helper);
  chromeos::DlcserviceClient::InitializeFake();
}

FakeDlcserviceHelper::~FakeDlcserviceHelper() {
  chromeos::DlcserviceClient::Shutdown();
}

chromeos::FakeDlcserviceClient* FakeDlcserviceHelper::FakeDlcserviceClient() {
  return static_cast<chromeos::FakeDlcserviceClient*>(
      chromeos::DlcserviceClient::Get());
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

FakeVmServicesHelper::FakeVmServicesHelper()
    : FakeCiceroneHelper(this),
      FakeSeneschalHelper(this),
      FakeDlcserviceHelper(this),
      FakeConciergeHelper(this) {}

}  // namespace borealis
