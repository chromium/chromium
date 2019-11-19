// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_DEVICE_STATE_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_DEVICE_STATE_MIXIN_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/test/scoped_policy_update.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"

namespace chromeos {

// A mixin for setting up device state:
// *   OOBE completion state
// *   enrollment state
// *   install attributes
// *   fake owner key
// *   cached device and local account policies
//
// Note that this uses in memory fake session manager client to store the
// policy blobs.
// It will initialized the fake in-memory client in
// SetUpInProcessBrowserTestFixture(), provided that a session manager was not
// initialized previously.
class DeviceStateMixin : public InProcessBrowserTestMixin {
 public:
  enum class State {
    BEFORE_OOBE,
    OOBE_COMPLETED_UNOWNED,
    OOBE_COMPLETED_CLOUD_ENROLLED,
    OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED,
    OOBE_COMPLETED_CONSUMER_OWNED,
    OOBE_COMPLETED_DEMO_MODE,
  };

  DeviceStateMixin(InProcessBrowserTestMixinHost* host, State initial_state);
  ~DeviceStateMixin() override;

  // InProcessBrowserTestMixin:
  bool SetUpUserDataDirectory() override;
  void SetUpInProcessBrowserTestFixture() override;
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override;

  // Returns a ScopedDevicePolicyUpdate instance that can be used to update
  // local device policy blob (kept in fake session manager client).
  // The cached policy value will be updated as the returned
  // ScopedDevicePolicyUpdate goes out of scope or when
  // FakeSessionManagerClient is initialized in
  // SetUpInProcessBrowserTestFixture(), whichever happens later.
  //
  // NOTE: If your test is serving policy using test policy server, use this to
  // set initially cached policy blob only. Prefre using policy server for
  // policy updates.
  std::unique_ptr<ScopedDevicePolicyUpdate> RequestDevicePolicyUpdate();

  // Returns ScopedUserPolicyUpdate instance that can be used to set, or update
  // local account policy blob for |account_id| (kept in fake session manager
  // client).
  //
  // The cached policy value will be updated as the returned
  // ScopedUsetPolicyUpdate goes out of scope or FakeSessionManagerClient is
  // initialized in SetUpInProcessBrowserTestFixtire(), whichever happens later.
  // DeviceStateMixin will fill out the required policy data values.
  //
  // NOTE: If your test is serving policy using test policy server, use this to
  // set initially cached policy blob only. Prefre using policy server for
  // policy updates.
  std::unique_ptr<ScopedUserPolicyUpdate> RequestDeviceLocalAccountPolicyUpdate(
      const std::string& account_id);

  void SetState(State state);
  void set_domain(const std::string& domain) { domain_ = domain; }
  void set_skip_initial_policy_setup(bool value) {
    skip_initial_policy_setup_ = value;
  }

 private:
  void SetDeviceState();
  void WriteInstallAttrFile();
  void WriteOwnerKey();

  // Whether |state_| value indicates enrolled state.
  bool IsEnrolledState() const;

  // Updates device policy blob stored by fake session manager client.
  void SetCachedDevicePolicy();

  // Updates device local account policy blob stored by fake session manager
  // client.
  void SetCachedDeviceLocalAccountPolicy(const std::string& account_id);

  State state_;
  std::string domain_;

  bool is_setup_ = false;

  // If set, the mixin will not set any device policy values during test setup,
  // unless requested using RequestDevicePolicyUpdate().
  bool skip_initial_policy_setup_ = false;

  // Whether session manager client has been initialized.
  bool session_manager_initialized_ = false;
  policy::DevicePolicyBuilder device_policy_;
  std::map<std::string, policy::UserPolicyBuilder>
      device_local_account_policies_;

  base::WeakPtrFactory<DeviceStateMixin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceStateMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_DEVICE_STATE_MIXIN_H_
