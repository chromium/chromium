// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/user_manager/scoped_user_manager.h"

class TestingProfile;

namespace base {
namespace test {
class ScopedRunningOnChromeOS;
}  // namespace test
}  // namespace base

namespace plugin_vm {

void SetupConciergeForSuccessfulDiskImageImport(
    ash::FakeConciergeClient* fake_concierge_client_);

void SetupConciergeForFailedDiskImageImport(
    ash::FakeConciergeClient* fake_concierge_client_,
    vm_tools::concierge::DiskImageStatus status);

void SetupConciergeForCancelDiskImageOperation(
    ash::FakeConciergeClient* fake_concierge_client_,
    bool success);

// A helper class for enabling Plugin VM in unit tests.
class PluginVmTestHelper {
 public:
  explicit PluginVmTestHelper(TestingProfile* testing_profile);

  PluginVmTestHelper(const PluginVmTestHelper&) = delete;
  PluginVmTestHelper& operator=(const PluginVmTestHelper&) = delete;

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

  // Adds an app in the default container. Replaces an existing app with the
  // same app name if one exists.
  void AddApp(const vm_tools::apps::App& app);

  // Returns the app id that the registry would use for the given app name.
  static std::string GenerateAppId(const std::string& app_name);

 private:
  void UpdateRegistry();

  raw_ptr<TestingProfile> testing_profile_;
  vm_tools::apps::ApplicationList current_apps_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::test::ScopedRunningOnChromeOS> running_on_chromeos_;
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_TEST_HELPER_H_
