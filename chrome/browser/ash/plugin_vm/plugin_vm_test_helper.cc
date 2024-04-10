// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

namespace {

const char kDiskImageImportCommandUuid[] = "3922722bd7394acf85bf4d5a330d4a47";
const char kDomain[] = "example.com";
const char kDeviceId[] = "device_id";

// Arbitrary container value used for updating the GuestOsRegistryService.
const char kPluginVmContainer[] = "PluginVmContainer";

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
    ChromeShelfController::instance()->ReplaceWithAppShortcutOrRemove(
        ash::ShelfID(kPluginVmShelfAppId));
  }
};

}  // namespace

void SetupConciergeForSuccessfulDiskImageImport(
    ash::FakeConciergeClient* fake_concierge_client_) {
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

void SetupConciergeForFailedDiskImageImport(
    ash::FakeConciergeClient* fake_concierge_client_,
    vm_tools::concierge::DiskImageStatus status) {
  // Set immediate response for the ImportDiskImage call: will be that "image is
  // in progress":
  vm_tools::concierge::ImportDiskImageResponse import_disk_image_response;
  import_disk_image_response.set_status(
      vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  import_disk_image_response.set_command_uuid(kDiskImageImportCommandUuid);
  fake_concierge_client_->set_import_disk_image_response(
      import_disk_image_response);

  // Set a series of signals: one at 50% (in progress) and one at 75%
  // (failed):
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals;
  vm_tools::concierge::DiskImageStatusResponse signal1;
  signal1.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  signal1.set_progress(50);
  signal1.set_command_uuid(kDiskImageImportCommandUuid);
  vm_tools::concierge::DiskImageStatusResponse signal2;
  signal2.set_status(status);
  signal2.set_progress(75);
  signal2.set_command_uuid(kDiskImageImportCommandUuid);
  fake_concierge_client_->set_disk_image_status_signals({signal1, signal2});

  // Finally, set a failure response for any eventual final call to
  // DiskImageStatus:
  vm_tools::concierge::DiskImageStatusResponse disk_image_status_response;
  disk_image_status_response.set_status(status);
  disk_image_status_response.set_command_uuid(kDiskImageImportCommandUuid);
  fake_concierge_client_->set_disk_image_status_response(
      disk_image_status_response);
}

void SetupConciergeForCancelDiskImageOperation(
    ash::FakeConciergeClient* fake_concierge_client_,
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

  current_apps_.set_vm_name(kPluginVmName);
  current_apps_.set_container_name(kPluginVmContainer);
  current_apps_.set_vm_type(guest_os::VmType::PLUGIN_VM);
}

PluginVmTestHelper::~PluginVmTestHelper() = default;

void PluginVmTestHelper::SetPolicyRequirementsToAllowPluginVm() {
  testing_profile_->GetPrefs()->SetBoolean(plugin_vm::prefs::kPluginVmAllowed,
                                           true);
  testing_profile_->GetPrefs()->SetString(plugin_vm::prefs::kPluginVmUserId,
                                          "fake-id");
  testing_profile_->ScopedCrosSettingsTestHelper()->SetBoolean(
      ash::kPluginVmAllowed, true);
}

void PluginVmTestHelper::SetUserRequirementsToAllowPluginVm() {
  // User for the profile should be affiliated with the device.
  const AccountId account_id(AccountId::FromUserEmailGaiaId(
      testing_profile_->GetProfileUserName(), "id"));
  auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
  auto* user = user_manager->AddUserWithAffiliationAndTypeAndProfile(
      account_id, true, user_manager::UserType::kRegular, testing_profile_);
  user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::move(user_manager));
  running_on_chromeos_ =
      std::make_unique<base::test::ScopedRunningOnChromeOS>();
}

void PluginVmTestHelper::EnablePluginVmFeature() {
  scoped_feature_list_.InitAndEnableFeature(features::kPluginVm);
}

void PluginVmTestHelper::EnterpriseEnrollDevice() {
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->InstallAttributes()
      ->SetCloudManaged(kDomain, kDeviceId);
}

void PluginVmTestHelper::AllowPluginVm() {
  ASSERT_FALSE(PluginVmFeatures::Get()->IsAllowed(testing_profile_));
  SetUserRequirementsToAllowPluginVm();
  EnablePluginVmFeature();
  EnterpriseEnrollDevice();
  SetPolicyRequirementsToAllowPluginVm();
  ASSERT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_));
}

void PluginVmTestHelper::EnablePluginVm() {
  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);
}

void PluginVmTestHelper::OpenShelfItem() {
  ash::ShelfID shelf_id(kPluginVmShelfAppId);
  std::unique_ptr<ash::ShelfItemDelegate> delegate =
      std::make_unique<FakeShelfItemDelegate>(shelf_id);
  ChromeShelfController* shelf_controller = ChromeShelfController::instance();
  // Similar logic to AppServiceAppWindowShelfController, for handling pins
  // and spinners.
  if (shelf_controller->GetItem(shelf_id)) {
    shelf_controller->shelf_model()->ReplaceShelfItemDelegate(
        shelf_id, std::move(delegate));
    shelf_controller->SetItemStatus(shelf_id, ash::STATUS_RUNNING);
  } else {
    shelf_controller->CreateAppItem(std::move(delegate), ash::STATUS_RUNNING,
                                    /*pinned=*/false);
  }
}

void PluginVmTestHelper::CloseShelfItem() {
  ChromeShelfController::instance()->Close(ash::ShelfID(kPluginVmShelfAppId));
}

void PluginVmTestHelper::AddApp(const vm_tools::apps::App& app) {
  for (int i = 0; i < current_apps_.apps_size(); ++i) {
    if (current_apps_.apps(i).desktop_file_id() == app.desktop_file_id()) {
      *current_apps_.mutable_apps(i) = app;
      UpdateRegistry();
      return;
    }
  }
  *current_apps_.add_apps() = app;
  UpdateRegistry();
}

// static
std::string PluginVmTestHelper::GenerateAppId(const std::string& app_name) {
  return guest_os::GuestOsRegistryService::GenerateAppId(
      app_name, /*vm_name=*/kPluginVmName,
      /*container_name=*/kPluginVmContainer);
}

void PluginVmTestHelper::UpdateRegistry() {
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(testing_profile_)
      ->UpdateApplicationList(current_apps_);
}

}  // namespace plugin_vm
