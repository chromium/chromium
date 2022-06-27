// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_DBUS_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_DBUS_TEST_HELPER_H_

namespace ash {
class FakeCiceroneClient;
class FakeConciergeClient;
class FakeSeneschalClient;
}  // namespace ash

namespace chromeos {
class FakeDlcserviceClient;
}  // namespace chromeos

namespace guest_os {

class BasicDBusHelper {
 public:
  BasicDBusHelper();
  ~BasicDBusHelper();
};

class FakeCiceroneHelper {
 public:
  explicit FakeCiceroneHelper(BasicDBusHelper* basic_helper);
  ~FakeCiceroneHelper();

  // Returns a handle to the dbus fake for cicerone.
  ash::FakeCiceroneClient* FakeCiceroneClient();
};

class FakeSeneschalHelper {
 public:
  explicit FakeSeneschalHelper(BasicDBusHelper* basic_helper);
  ~FakeSeneschalHelper();

  // Returns a handle to the dbus fake for seneschal.
  ash::FakeSeneschalClient* FakeSeneschalClient();
};

class FakeDlcserviceHelper {
 public:
  explicit FakeDlcserviceHelper(BasicDBusHelper* basic_helper);
  ~FakeDlcserviceHelper();

  chromeos::FakeDlcserviceClient* FakeDlcserviceClient();
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
  explicit FakeChunneldHelper(BasicDBusHelper* basic_helper);
  ~FakeChunneldHelper();
};

// A class for less boilerplate in VM tests. Have your fixture inherit from this
// class, and the dbus services common to most VMs get initialised with fakes
// during before your test and torn down correctly after.
// You can access the fakes with e.g. this->FakeConciergeClient.
class FakeVmServicesHelper : public BasicDBusHelper,
                             public FakeCiceroneHelper,
                             public FakeSeneschalHelper,
                             public FakeDlcserviceHelper,
                             public FakeConciergeHelper,
                             public FakeChunneldHelper {
 public:
  FakeVmServicesHelper();
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_DBUS_TEST_HELPER_H_
