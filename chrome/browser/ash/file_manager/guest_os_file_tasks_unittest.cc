// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/guest_os_file_tasks.h"

#include "base/files/file_path.h"
#include "base/strings/escape.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/plugin_vm/fake_plugin_vm_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace file_tasks {
namespace {

static constexpr auto VM_TERMINA = guest_os::VmType::TERMINA;
static constexpr auto PLUGIN_VM = guest_os::VmType::PLUGIN_VM;

}  // namespace

class GuestOsFileTasksTest : public testing::Test {
 protected:
  GuestOsFileTasksTest() = default;

  GuestOsFileTasksTest(const GuestOsFileTasksTest&) = delete;
  GuestOsFileTasksTest& operator=(const GuestOsFileTasksTest&) = delete;

  void SetUp() override {
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        util::GetDownloadsMountPointName(&profile_),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        util::GetMyFilesFolderForProfile(&profile_));
    fake_crostini_features_.set_enabled(true);
    fake_plugin_vm_features_.set_enabled(true);
  }

  void TearDown() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        util::GetDownloadsMountPointName(&profile_));
  }

  void AddApp(const std::string& id,
              const std::string& name,
              const std::vector<std::string>& mimes,
              const std::vector<std::string>& extensions,
              guest_os::VmType vm_type) {
    // crostini.registry {<id>: {container_name: "penguin", name: {"": <name>},
    //                           mime_types: [<mime>,], vm_name: "termina"}}
    ScopedDictPrefUpdate update(profile_.GetPrefs(),
                                guest_os::prefs::kGuestOsRegistry);
    base::Value::Dict& registry = update.Get();
    base::Value app(base::Value::Type::DICTIONARY);
    app.SetKey("container_name", base::Value("penguin"));
    base::Value mime_list(base::Value::Type::LIST);
    for (const auto& mime : mimes)
      mime_list.Append(mime);
    app.SetKey("mime_types", std::move(mime_list));
    base::Value extension_list(base::Value::Type::LIST);
    for (const auto& extension : extensions)
      extension_list.Append(extension);
    app.SetKey("extensions", std::move(extension_list));
    base::Value name_dict(base::Value::Type::DICTIONARY);
    name_dict.SetKey("", base::Value(name));
    app.SetKey("name", std::move(name_dict));
    app.SetKey("vm_name", base::Value("termina"));
    app.SetIntKey("vm_type", static_cast<int>(vm_type));
    registry.Set(id, std::move(app));
  }

  void AddEntry(const std::string& path, const std::string& mime) {
    entries_.push_back(
        extensions::EntryInfo(base::FilePath(path), mime, false));
    std::string virtual_path = base::EscapeUrlEncodedData(
        util::GetDownloadsMountPointName(&profile_) + "/" + path,
        /*use_plus=*/false);
    urls_.push_back(
        GURL("filesystem:chrome-extension://id/external/" + virtual_path));
  }

  void AddMime(const std::string& file_ext, const std::string& mime) {
    // crostini.mime_types.termina.penguin.<file_ext>: <mime>
    ScopedDictPrefUpdate update(profile_.GetPrefs(),
                                guest_os::prefs::kGuestOsMimeTypes);
    base::Value::Dict& mimes = update.Get();
    mimes.SetByDottedPath("termina.penguin." + file_ext, mime);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::vector<extensions::EntryInfo> entries_;
  std::vector<GURL> urls_;
  std::vector<std::string> app_ids_;
  std::vector<std::string> app_names_;
  std::vector<guest_os::VmType> app_vm_types_;
  crostini::FakeCrostiniFeatures fake_crostini_features_;
  plugin_vm::FakePluginVmFeatures fake_plugin_vm_features_;
};

TEST_F(GuestOsFileTasksTest, CheckPathsCanBeShared) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.txt", "test/mime1");

  // Share ok.
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));

  // Share fails.
  urls_.clear();
  urls_.push_back(GURL("filesysytem://share/fail"));
  app_ids_.clear();
  app_names_.clear();
  app_vm_types_.clear();
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
  EXPECT_THAT(app_vm_types_, testing::IsEmpty());
}

TEST_F(GuestOsFileTasksTest, NoApps) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.txt", "test/mime2");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
  EXPECT_THAT(app_vm_types_, testing::IsEmpty());
}

TEST_F(GuestOsFileTasksTest, Termina_AppRegistered) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));
}

TEST_F(GuestOsFileTasksTest, Termina_IgnoreCase) {
  AddApp("app1", "name1", {"Test/Mime1"}, {}, VM_TERMINA);
  AddEntry("entry.txt", "tesT/mimE1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));
}

TEST_F(GuestOsFileTasksTest, Termina_NotEnabled) {
  fake_crostini_features_.set_enabled(false);
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
  EXPECT_THAT(app_vm_types_, testing::IsEmpty());
}

TEST_F(GuestOsFileTasksTest, PluginVm_AppRegistered) {
  AddApp("app1", "name1", {}, {"txt"}, PLUGIN_VM);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1 (Windows)"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(PLUGIN_VM));
}

TEST_F(GuestOsFileTasksTest, PluginVm_IgnoreCase) {
  AddApp("app1", "name1", {}, {"Txt"}, PLUGIN_VM);
  AddEntry("entry.txT", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1 (Windows)"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(PLUGIN_VM));
}

TEST_F(GuestOsFileTasksTest, PluginVm_NotEnabled) {
  fake_plugin_vm_features_.set_enabled(false);
  AddApp("app1", "name1", {}, {"txt"}, PLUGIN_VM);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
  EXPECT_THAT(app_vm_types_, testing::IsEmpty());
}

TEST_F(GuestOsFileTasksTest, Termina_NotAllEntries) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddApp("app2", "name2", {"test/mime2"}, {}, VM_TERMINA);
  AddEntry("entry1.txt", "test/mime1");
  AddEntry("entry2.txt", "test/mime2");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
  EXPECT_THAT(app_vm_types_, testing::IsEmpty());
}

TEST_F(GuestOsFileTasksTest, PluginVm_NotAllEntries) {
  AddApp("app1", "name1", {}, {"txt"}, PLUGIN_VM);
  AddApp("app2", "name2", {}, {"jpg"}, PLUGIN_VM);
  AddEntry("entry1.txt", "test/mime1");
  AddEntry("entry2.jpg", "test/mime2");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::IsEmpty());
  EXPECT_THAT(app_names_, testing::IsEmpty());
  EXPECT_THAT(app_vm_types_, testing::IsEmpty());
}

TEST_F(GuestOsFileTasksTest, Termina_MultipleAppsRegistered) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddApp("app2", "name2", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1", "app2"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1", "name2"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA, VM_TERMINA));
}

TEST_F(GuestOsFileTasksTest, PluginVm_MultipleAppsRegistered) {
  AddApp("app1", "name1", {}, {"txt"}, PLUGIN_VM);
  AddApp("app2", "name2", {}, {"txt"}, PLUGIN_VM);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1", "app2"));
  EXPECT_THAT(app_names_,
              testing::ElementsAre("name1 (Windows)", "name2 (Windows)"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(PLUGIN_VM, PLUGIN_VM));
}

TEST_F(GuestOsFileTasksTest, MultipleAppsFromMultipleVmsRegistered) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddApp("app2", "name2", {}, {"txt"}, PLUGIN_VM);
  AddEntry("entry.txt", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1", "app2"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1", "name2 (Windows)"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA, PLUGIN_VM));
}

TEST_F(GuestOsFileTasksTest, AppRegisteredForTextPlain) {
  AddApp("app1", "name1", {"text/plain"}, {}, VM_TERMINA);
  AddEntry("entry.js", "text/javascript");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));
}

TEST_F(GuestOsFileTasksTest, MimeServiceForTextPlain) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.unknown", "text/plain");
  AddMime("unknown", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));
}

TEST_F(GuestOsFileTasksTest, MimeServiceForApplicationOctetStream) {
  AddApp("app1", "name1", {"test/mime1"}, {}, VM_TERMINA);
  AddEntry("entry.unknown", "application/octet-stream");
  AddMime("unknown", "test/mime1");
  FindGuestOsApps(&profile_, entries_, urls_, &app_ids_, &app_names_,
                  &app_vm_types_);
  EXPECT_THAT(app_ids_, testing::ElementsAre("app1"));
  EXPECT_THAT(app_names_, testing::ElementsAre("name1"));
  EXPECT_THAT(app_vm_types_, testing::ElementsAre(VM_TERMINA));
}

}  // namespace file_tasks
}  // namespace file_manager
