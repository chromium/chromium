// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/vm/vm_applications_service_provider.h"

#include <memory>

#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ash {

class VmApplicationsServiceProviderTest : public testing::Test {
 public:
  VmApplicationsServiceProviderTest() = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override { profile_.reset(); }

  void RegisterVm(const std::string& vm_name, guest_os::VmType vm_type) {
    // Add an entry to the containers list pref.
    ScopedListPrefUpdate update(profile_->GetPrefs(),
                                guest_os::prefs::kGuestOsContainers);
    base::DictValue container;
    container.Set(guest_os::prefs::kVmNameKey, vm_name);
    container.Set(guest_os::prefs::kVmTypeKey, static_cast<int>(vm_type));
    update->Append(std::move(container));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(VmApplicationsServiceProviderTest, ParseSelectFileDialogFileTypes) {
  VmApplicationsServiceProvider test;
  ui::SelectFileDialog::FileTypeInfo file_types;
  int file_type_index = 0;

  // Complex.
  test.ParseSelectFileDialogFileTypes("e1,e2:d1|,e3:d2|*", &file_types,
                                      &file_type_index);

  std::vector<std::vector<std::string>> exts{{"e1", "e2"}, {"e3"}};
  std::vector<std::u16string> descs{u"d1", u"d2"};
  EXPECT_EQ(file_types.extensions, exts);
  EXPECT_EQ(file_types.extension_description_overrides, descs);
  EXPECT_EQ(file_type_index, 2);
  EXPECT_TRUE(file_types.include_all_files);

  // Simple.
  test.ParseSelectFileDialogFileTypes("e1,e2", &file_types, &file_type_index);
  exts = {{"e1", "e2"}};
  descs = {u""};
  EXPECT_EQ(file_types.extensions, exts);
  EXPECT_EQ(file_types.extension_description_overrides, descs);
  EXPECT_EQ(file_type_index, 0);
  EXPECT_FALSE(file_types.include_all_files);
}

TEST_F(VmApplicationsServiceProviderTest, DetermineDlpComponent) {
  VmApplicationsServiceProvider test;

  // Test Crostini (Termina)
  RegisterVm("termina", guest_os::VmType::TERMINA);
  EXPECT_EQ(test.DetermineDlpComponent(profile_.get(), "termina"),
            data_controls::Component::kCrostini);

  // Test Baguette (New Crostini)
  RegisterVm("baguette", guest_os::VmType::BAGUETTE);
  EXPECT_EQ(test.DetermineDlpComponent(profile_.get(), "baguette"),
            data_controls::Component::kCrostini);

  // Test PluginVm
  RegisterVm("PvmDefault", guest_os::VmType::PLUGIN_VM);
  EXPECT_EQ(test.DetermineDlpComponent(profile_.get(), "PvmDefault"),
            data_controls::Component::kPluginVm);

  // Test Bruschetta (mapped to Crostini)
  RegisterVm("bru", guest_os::VmType::BRUSCHETTA);
  EXPECT_EQ(test.DetermineDlpComponent(profile_.get(), "bru"),
            data_controls::Component::kCrostini);

  // Test ArcVm
  RegisterVm("arcvm", guest_os::VmType::ARCVM);
  EXPECT_EQ(test.DetermineDlpComponent(profile_.get(), "arcvm"),
            data_controls::Component::kArc);

  // Test unknown VM (should fallback to Crostini)
  EXPECT_EQ(test.DetermineDlpComponent(profile_.get(), "unknown"),
            data_controls::Component::kCrostini);
}

}  // namespace ash
