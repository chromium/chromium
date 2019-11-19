// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/json/json_reader.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

namespace {

const char kDiskImageImportCommandUuid[] = "3922722bd7394acf85bf4d5a330d4a47";
const char kPluginVmLicenseKey[] = "LICENSE_KEY";
const char kPluginVmImageHash[] =
    "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32";
const char kDomain[] = "example.com";
const char kDeviceId[] = "device_id";
const char kPluginVmImageUrl[] = "https://example.com/plugin_vm_image";

// For adding a fake shelf item without requiring opening an actual window.
class FakeShelfItemDelegate : public ash::ShelfItemDelegate {
 public:
  explicit FakeShelfItemDelegate(const ash::ShelfID& shelf_id)
      : ShelfItemDelegate(shelf_id) {}

  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override {}
  void Close() override {
    ChromeLauncherController::instance()->CloseLauncherItem(
        ash::ShelfID(kPluginVmAppId));
  }
};

}  // namespace

void SetupConciergeForSuccessfulDiskImageImport(
    chromeos::FakeConciergeClient* fake_concierge_client_) {
  // Set immediate response for the ImportDiskImage call: will be that "image is
  // in progress":
  vm_tools::concierge::ImportDiskImageResponse import_disk_image_response;
  import_disk_image_response.set_status(
      vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  import_disk_image_response.set_command_uuid(kDiskImageImportCommandUuid);
  fake_concierge_client_->set_import_disk_image_response(
      import_disk_image_response);

  // Set a series of signals: one at 50% (in progress) and one at 100%
  // (created):
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals;
  vm_tools::concierge::DiskImageStatusResponse signal1;
  signal1.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  signal1.set_progress(50);
  signal1.set_command_uuid(kDiskImageImportCommandUuid);
  vm_tools::concierge::DiskImageStatusResponse signal2;
  signal2.set_status(vm_tools::concierge::DISK_STATUS_CREATED);
  signal2.set_progress(100);
  signal2.set_command_uuid(kDiskImageImportCommandUuid);
  fake_concierge_client_->set_disk_image_status_signals({signal1, signal2});

  // Finally, set a success response for any eventual final call to
  // DiskImageStatus:
  vm_tools::concierge::DiskImageStatusResponse disk_image_status_response;
  disk_image_status_response.set_status(
      vm_tools::concierge::DISK_STATUS_CREATED);
  disk_image_status_response.set_command_uuid(kDiskImageImportCommandUuid);
  fake_concierge_client_->set_disk_image_status_response(
      disk_image_status_response);
}

void SetupConciergeForCancelDiskImageOperation(
    chromeos::FakeConciergeClient* fake_concierge_client_,
    bool success) {
  vm_tools::concierge::CancelDiskImageResponse cancel_disk_image_response;
  cancel_disk_image_response.set_success(success);
  fake_concierge_client_->set_cancel_disk_image_response(
      cancel_disk_image_response);
}

PluginVmTestHelper::PluginVmTestHelper(TestingProfile* testing_profile)
    : testing_profile_(testing_profile) {
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->ReplaceDeviceSettingsProviderWithStub();
}

PluginVmTestHelper::~PluginVmTestHelper() = default;

void PluginVmTestHelper::SetPolicyRequirementsToAllowPluginVm() {
  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      chromeos::kPluginVmAllowed, true);
  testing_profile_->ScopedCrosSettingsTestHelper()->SetString(
      chromeos::kPluginVmLicenseKey, kPluginVmLicenseKey);

  DictionaryPrefUpdate update(testing_profile_->GetPrefs(),
                              plugin_vm::prefs::kPluginVmImage);
  base::DictionaryValue* plugin_vm_image = update.Get();
  plugin_vm_image->SetKey("url", base::Value(kPluginVmImageUrl));
  plugin_vm_image->SetKey("hash", base::Value(kPluginVmImageHash));
}

void PluginVmTestHelper::SetUserRequirementsToAllowPluginVm() {
  // User for the profile should be affiliated with the device.
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      testing_profile_->GetProfileUserName(), "id"));
  auto mock_user_manager =
      std::make_unique<testing::NiceMock<chromeos::MockUserManager>>();
  mock_user_manager->AddUserWithAffiliationAndType(
      account_id, true, user_manager::USER_TYPE_REGULAR);
  chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(
      mock_user_manager->GetActiveUser());
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::move(mock_user_manager));
}

void PluginVmTestHelper::EnablePluginVmFeature() {
  scoped_feature_list_.InitAndEnableFeature(features::kPluginVm);
}

void PluginVmTestHelper::EnableDevMode() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kSystemDevMode);
}

void PluginVmTestHelper::EnterpriseEnrollDevice() {
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->InstallAttributes()
      ->SetCloudManaged(kDomain, kDeviceId);
}

void PluginVmTestHelper::AllowPluginVm() {
  ASSERT_FALSE(IsPluginVmAllowedForProfile(testing_profile_));
  SetUserRequirementsToAllowPluginVm();
  EnablePluginVmFeature();
  EnterpriseEnrollDevice();
  SetPolicyRequirementsToAllowPluginVm();
  ASSERT_TRUE(IsPluginVmAllowedForProfile(testing_profile_));
}

void PluginVmTestHelper::AllowPluginVmForManualTesting() {
  ASSERT_FALSE(IsPluginVmAllowedForProfile(testing_profile_));
  SetUserRequirementsToAllowPluginVm();
  EnablePluginVmFeature();
  EnableDevMode();
  ASSERT_TRUE(IsPluginVmAllowedForProfile(testing_profile_));
}

void PluginVmTestHelper::OpenShelfItem() {
  ash::ShelfID shelf_id(kPluginVmAppId);
  std::unique_ptr<ash::ShelfItemDelegate> delegate =
      std::make_unique<FakeShelfItemDelegate>(shelf_id);
  ChromeLauncherController* laucher_controller =
      ChromeLauncherController::instance();
  // Similar logic to InternalAppWindowShelfController, for handling pins and
  // spinners.
  if (laucher_controller->GetItem(shelf_id)) {
    laucher_controller->shelf_model()->SetShelfItemDelegate(
        shelf_id, std::move(delegate));
    laucher_controller->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  } else {
    laucher_controller->CreateAppLauncherItem(std::move(delegate),
                                              ash::STATUS_RUNNING);
  }
}

void PluginVmTestHelper::CloseShelfItem() {
  ChromeLauncherController::instance()->Close(ash::ShelfID(kPluginVmAppId));
}

}  // namespace plugin_vm
