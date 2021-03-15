// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_TEST_HELPER_H_

#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/fake_concierge_client.h"

class TestingProfile;

namespace base {
namespace test {
class ScopedRunningOnChromeOS;
}  // namespace test
}  // namespace base

namespace user_manager {
class ScopedUserManager;
}  // namespace user_manager

namespace plugin_vm {

void SetupConciergeForSuccessfulDiskImageImport(
    chromeos::FakeConciergeClient* fake_concierge_client_);

void SetupConciergeForFailedDiskImageImport(
    chromeos::FakeConciergeClient* fake_concierge_client_,
    vm_tools::concierge::DiskImageStatus status);

void SetupConciergeForCancelDiskImageOperation(
    chromeos::FakeConciergeClient* fake_concierge_client_,
    bool success);

// A helper class for enabling Plugin VM in unit tests.
class PluginVmTestHelper {
 public:
  explicit PluginVmTestHelper(TestingProfile* testing_profile);
  ~PluginVmTestHelper();

  void SetPolicyRequirementsToAllowPluginVm();
  void SetUserRequirementsToAllowPluginVm();
  void EnablePluginVmFeature();
  void EnterpriseEnrollDevice();

  // Naming follows plugin_vm_util. Allow indicates Plugin VM can be used, while
  // enable indicates Plugin VM has been installed.
  void AllowPluginVm();
  void EnablePluginVm();

  // Fakes the Plugin VM window being opened or closed.
  void OpenShelfItem();
  void CloseShelfItem();

 private:
  TestingProfile* testing_profile_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::test::ScopedRunningOnChromeOS> running_on_chromeos_;

  DISALLOW_COPY_AND_ASSIGN(PluginVmTestHelper);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_TEST_HELPER_H_
