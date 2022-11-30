// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_DBUS_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_DBUS_TEST_HELPER_H_

namespace ash {
class FakeCiceroneClient;
class FakeConciergeClient;
class FakeDlcserviceClient;
class FakeSeneschalClient;
}  // namespace ash

namespace guest_os {

class FakeCiceroneHelper {
 public:
  FakeCiceroneHelper();
  ~FakeCiceroneHelper();

  // Returns a handle to the dbus fake for cicerone.
  ash::FakeCiceroneClient* FakeCiceroneClient();
};

class FakeSeneschalHelper {
 public:
  FakeSeneschalHelper();
  ~FakeSeneschalHelper();

  // Returns a handle to the dbus fake for seneschal.
  ash::FakeSeneschalClient* FakeSeneschalClient();
};

class FakeDlcserviceHelper {
 public:
  FakeDlcserviceHelper();
  ~FakeDlcserviceHelper();

  ash::FakeDlcserviceClient* FakeDlcserviceClient();
};

class FakeConciergeHelper {
 public:
  explicit FakeConciergeHelper(FakeCiceroneHelper* cicerone_helper);
  ~FakeConciergeHelper();

  // Returns a handle to the dbus fake for concierge.
  ash::FakeConciergeClient* FakeConciergeClient();
};

class FakeChunneldHelper {
 public:
  FakeChunneldHelper();
  ~FakeChunneldHelper();
};

// A class for less boilerplate in VM tests. Have your fixture inherit from this
// class, and the dbus services common to most VMs get initialised with fakes
// during before your test and torn down correctly after.
// You can access the fakes with e.g. this->FakeConciergeClient.
class FakeVmServicesHelper : public FakeCiceroneHelper,
                             public FakeSeneschalHelper,
                             public FakeDlcserviceHelper,
                             public FakeConciergeHelper,
                             public FakeChunneldHelper {
 public:
  FakeVmServicesHelper();
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_DBUS_TEST_HELPER_H_
