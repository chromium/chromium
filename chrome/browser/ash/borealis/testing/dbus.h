// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_TESTING_DBUS_H_
#define CHROME_BROWSER_ASH_BOREALIS_TESTING_DBUS_H_

namespace chromeos {
class FakeCiceroneClient;
class FakeConciergeClient;
class FakeSeneschalClient;
class FakeDlcserviceClient;
}  // namespace chromeos

namespace borealis {

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
  chromeos::FakeCiceroneClient* FakeCiceroneClient();
};

class FakeSeneschalHelper {
 public:
  explicit FakeSeneschalHelper(BasicDBusHelper* basic_helper);
  ~FakeSeneschalHelper();

  // Returns a handle to the dbus fake for seneschal.
  chromeos::FakeSeneschalClient* FakeSeneschalClient();
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
  chromeos::FakeConciergeClient* FakeConciergeClient();
};

class FakeVmServicesHelper : public BasicDBusHelper,
                             public FakeCiceroneHelper,
                             public FakeSeneschalHelper,
                             public FakeDlcserviceHelper,
                             public FakeConciergeHelper {
 public:
  FakeVmServicesHelper();
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_TESTING_DBUS_H_
