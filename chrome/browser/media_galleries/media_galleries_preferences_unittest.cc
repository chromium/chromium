// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaGalleriesPreferences unit tests.

#include "chrome/browser/media_galleries/media_galleries_preferences.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/common/apps/platform_apps/media_galleries_permission.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/storage_monitor/media_storage_util.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#endif

using base::ASCIIToUTF16;
using storage_monitor::MediaStorageUtil;
using storage_monitor::StorageInfo;
using storage_monitor::TestStorageMonitor;

namespace {

class MockGalleryChangeObserver
    : public MediaGalleriesPreferences::GalleryChangeObserver {
 public:
  explicit MockGalleryChangeObserver(MediaGalleriesPreferences* pref)
      : pref_(pref),
        notifications_(0) {}

  MockGalleryChangeObserver(const MockGalleryChangeObserver&) = delete;
  MockGalleryChangeObserver& operator=(const MockGalleryChangeObserver&) =
      delete;

  ~MockGalleryChangeObserver() override {}

  int notifications() const { return notifications_;}

 private:
  // MediaGalleriesPreferences::GalleryChangeObserver implementation.
  void OnPermissionAdded(MediaGalleriesPreferences* pref,
                         const std::string& extension_id,
                         MediaGalleryPrefId pref_id) override {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  void OnPermissionRemoved(MediaGalleriesPreferences* pref,
                           const std::string& extension_id,
                           MediaGalleryPrefId pref_id) override {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  void OnGalleryAdded(MediaGalleriesPreferences* pref,
                      MediaGalleryPrefId pref_id) override {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  void OnGalleryRemoved(MediaGalleriesPreferences* pref,
                        MediaGalleryPrefId pref_id) override {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  void OnGalleryInfoUpdated(MediaGalleriesPreferences* pref,
                            MediaGalleryPrefId pref_id) override {
    EXPECT_EQ(pref_, pref);
    ++notifications_;
  }

  raw_ptr<MediaGalleriesPreferences> pref_;
  int notifications_;
};

}  // namespace

class MediaGalleriesPreferencesTest : public testing::Test {
 public:
  typedef std::map<std::string /*device id*/, MediaGalleryPrefIdSet>
      DeviceIdPrefIdsMap;

  MediaGalleriesPreferencesTest()
      : profile_(new TestingProfile()), default_galleries_count_(0) {}

  MediaGalleriesPreferencesTest(const MediaGalleriesPreferencesTest&) = delete;
  MediaGalleriesPreferencesTest& operator=(
      const MediaGalleriesPreferencesTest&) = delete;

  ~MediaGalleriesPreferencesTest() override {}

  void SetUp() override {
    ASSERT_TRUE(TestStorageMonitor::CreateAndInstall());

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_.get())));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

    ReinitPrefsAndExpectations();

    const MediaGalleriesPrefInfoMap& known_galleries =
        gallery_prefs_->known_galleries();
    if (!known_galleries.empty()) {
      ASSERT_EQ(3U, known_galleries.size());
    }

    std::vector<std::string> all_permissions;
    all_permissions.push_back(
        chrome_apps::MediaGalleriesPermission::kReadPermission);
    all_permissions.push_back(
        chrome_apps::MediaGalleriesPermission::kAllAutoDetectedPermission);
    std::vector<std::string> read_permissions;
    read_permissions.push_back(
        chrome_apps::MediaGalleriesPermission::kReadPermission);

    all_permission_extension =
        AddMediaGalleriesApp("all", all_permissions, profile_.get());
    regular_permission_extension =
        AddMediaGalleriesApp("regular", read_permissions, profile_.get());
    no_permissions_extension =
        AddMediaGalleriesApp("no", read_permissions, profile_.get());
  }

  void TearDown() override {
    Verify();
    TestStorageMonitor::Destroy();
  }

  void ChangeMediaPathOverrides() {
    mock_gallery_locations_.ChangeMediaPathOverrides();
  }

  void ReinitPrefsAndExpectations() {
    gallery_prefs_ =
        std::make_unique<MediaGalleriesPreferences>(profile_.get());
    base::RunLoop loop;
    gallery_prefs_->EnsureInitialized(loop.QuitClosure());
    loop.Run();

    // Load the default galleries into the expectations.
    const MediaGalleriesPrefInfoMap& known_galleries =
        gallery_prefs_->known_galleries();
    if (!known_galleries.empty()) {
      default_galleries_count_ = 3;
      MediaGalleriesPrefInfoMap::const_iterator it;
      for (it = known_galleries.begin(); it != known_galleries.end(); ++it) {
        expected_galleries_[it->first] = it->second;
        if (it->second.type == MediaGalleryPrefInfo::kAutoDetected)
          expected_galleries_for_all.insert(it->first);
      }
    }
  }

  void RemovePersistedDefaultGalleryValues() {
    PrefService* prefs = profile_->GetPrefs();
    std::unique_ptr<ScopedListPrefUpdate> update =
        std::make_unique<ScopedListPrefUpdate>(
            prefs, prefs::kMediaGalleriesRememberedGalleries);
    base::Value::List& list = update->Get();

    for (auto& entry : list) {
      if (entry.is_dict()) {
        base::Value::Dict& dict = entry.GetDict();
        // Setting the prefs version to 2 which is the version before
        // default_gallery_type was added.
        dict.Set(kMediaGalleriesPrefsVersionKey, 2);
        dict.Remove(kMediaGalleriesDefaultGalleryTypeKey);
      }
    }
    update.reset();
  }

  void Verify() {
    const MediaGalleriesPrefInfoMap& known_galleries =
        gallery_prefs_->known_galleries();
    EXPECT_EQ(expected_galleries_.size(), known_galleries.size());
    for (auto it = known_galleries.begin(); it != known_galleries.end(); ++it) {
      VerifyGalleryInfo(it->second, it->first);
      if (it->second.type != MediaGalleryPrefInfo::kAutoDetected &&
          it->second.type != MediaGalleryPrefInfo::kBlockListed) {
        if (!base::Contains(expected_galleries_for_all, it->first) &&
            !base::Contains(expected_galleries_for_regular, it->first)) {
          EXPECT_FALSE(gallery_prefs_->NonAutoGalleryHasPermission(it->first));
        } else {
          EXPECT_TRUE(gallery_prefs_->NonAutoGalleryHasPermission(it->first));
        }
      }
    }

    for (DeviceIdPrefIdsMap::const_iterator it = expected_device_map.begin();
         it != expected_device_map.end();
         ++it) {
      MediaGalleryPrefIdSet actual_id_set =
          gallery_prefs_->LookUpGalleriesByDeviceId(it->first);
      EXPECT_EQ(it->second, actual_id_set);
    }

    std::set<MediaGalleryPrefId> galleries_for_all =
        gallery_prefs_->GalleriesForExtension(*all_permission_extension.get());
    EXPECT_EQ(expected_galleries_for_all, galleries_for_all);

    std::set<MediaGalleryPrefId> galleries_for_regular =
        gallery_prefs_->GalleriesForExtension(
            *regular_permission_extension.get());
    EXPECT_EQ(expected_galleries_for_regular, galleries_for_regular);

    std::set<MediaGalleryPrefId> galleries_for_no =
        gallery_prefs_->GalleriesForExtension(*no_permissions_extension.get());
    EXPECT_EQ(0U, galleries_for_no.size());
  }

  void VerifyGalleryInfo(const MediaGalleryPrefInfo& actual,
                         MediaGalleryPrefId expected_id) const {
    auto in_expectation = expected_galleries_.find(expected_id);
    ASSERT_FALSE(in_expectation == expected_galleries_.end())  << expected_id;
    EXPECT_EQ(in_expectation->second.pref_id, actual.pref_id);
    EXPECT_EQ(in_expectation->second.display_name, actual.display_name);
    EXPECT_EQ(in_expectation->second.device_id, actual.device_id);
    EXPECT_EQ(in_expectation->second.path.value(), actual.path.value());
    EXPECT_EQ(in_expectation->second.type, actual.type);
    EXPECT_EQ(in_expectation->second.audio_count, actual.audio_count);
    EXPECT_EQ(in_expectation->second.image_count, actual.image_count);
    EXPECT_EQ(in_expectation->second.video_count, actual.video_count);
    EXPECT_EQ(
        in_expectation->second.default_gallery_type,
        actual.default_gallery_type);
  }

  MediaGalleriesPreferences* gallery_prefs() {
    return gallery_prefs_.get();
  }

  uint64_t default_galleries_count() { return default_galleries_count_; }

  void AddGalleryExpectation(MediaGalleryPrefId id,
                             std::u16string display_name,
                             std::string device_id,
                             base::FilePath relative_path,
                             MediaGalleryPrefInfo::Type type) {
    expected_galleries_[id].pref_id = id;
    expected_galleries_[id].display_name = display_name;
    expected_galleries_[id].device_id = device_id;
    expected_galleries_[id].path = relative_path.NormalizePathSeparators();
    expected_galleries_[id].type = type;

    if (type == MediaGalleryPrefInfo::kAutoDetected)
      expected_galleries_for_all.insert(id);

    expected_device_map[device_id].insert(id);
  }

  void AddScanResultExpectation(MediaGalleryPrefId id,
                                std::u16string display_name,
                                std::string device_id,
                                base::FilePath relative_path,
                                int audio_count,
                                int image_count,
                                int video_count) {
    AddGalleryExpectation(id, display_name, device_id, relative_path,
                          MediaGalleryPrefInfo::kScanResult);
    expected_galleries_[id].audio_count = audio_count;
    expected_galleries_[id].image_count = image_count;
    expected_galleries_[id].video_count = video_count;
  }

  MediaGalleryPrefId AddGalleryWithNameV0(const std::string& device_id,
                                          const std::u16string& display_name,
                                          const base::FilePath& relative_path,
                                          bool user_added) {
    MediaGalleryPrefInfo::Type type =
        user_added ? MediaGalleryPrefInfo::kUserAdded
                   : MediaGalleryPrefInfo::kAutoDetected;
    return gallery_prefs()->AddOrUpdateGalleryInternal(
        device_id, display_name, relative_path, type, std::u16string(),
        std::u16string(), std::u16string(), 0, base::Time(), false, 0, 0, 0, 0,
        MediaGalleryPrefInfo::kNotDefault);
  }

  MediaGalleryPrefId AddGalleryWithNameV1(const std::string& device_id,
                                          const std::u16string& display_name,
                                          const base::FilePath& relative_path,
                                          bool user_added) {
    MediaGalleryPrefInfo::Type type =
        user_added ? MediaGalleryPrefInfo::kUserAdded
                   : MediaGalleryPrefInfo::kAutoDetected;
    return gallery_prefs()->AddOrUpdateGalleryInternal(
        device_id, display_name, relative_path, type, std::u16string(),
        std::u16string(), std::u16string(), 0, base::Time(), false, 0, 0, 0, 1,
        MediaGalleryPrefInfo::kNotDefault);
  }

  MediaGalleryPrefId AddGalleryWithNameV2(const std::string& device_id,
                                          const std::u16string& display_name,
                                          const base::FilePath& relative_path,
                                          MediaGalleryPrefInfo::Type type) {
    return gallery_prefs()->AddOrUpdateGalleryInternal(
        device_id, display_name, relative_path, type, std::u16string(),
        std::u16string(), std::u16string(), 0, base::Time(), false, 0, 0, 0, 2,
        MediaGalleryPrefInfo::kNotDefault);
  }

  MediaGalleryPrefId AddFixedGalleryWithExpectation(
      const std::string& path_name,
      const std::string& name,
      MediaGalleryPrefInfo::Type type) {
    base::FilePath path = MakeMediaGalleriesTestingPath(path_name);
    StorageInfo info;
    base::FilePath relative_path;
    MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
    std::u16string gallery_name = base::ASCIIToUTF16(name);
    MediaGalleryPrefId id = AddGalleryWithNameV2(info.device_id(), gallery_name,
                                                relative_path, type);
    AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                          type);
    Verify();
    return id;
  }

  scoped_refptr<extensions::Extension> all_permission_extension;
  scoped_refptr<extensions::Extension> regular_permission_extension;
  scoped_refptr<extensions::Extension> no_permissions_extension;

  std::set<MediaGalleryPrefId> expected_galleries_for_all;
  std::set<MediaGalleryPrefId> expected_galleries_for_regular;

  DeviceIdPrefIdsMap expected_device_map;

  MediaGalleriesPrefInfoMap expected_galleries_;

 private:
  // Needed for extension service & friends to work.
  content::BrowserTaskEnvironment task_environment_;

  EnsureMediaDirectoriesExists mock_gallery_locations_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          ash::CrosSettings::Get())};
#endif

  TestStorageMonitor monitor_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MediaGalleriesPreferences> gallery_prefs_;

  uint64_t default_galleries_count_;
};

TEST_F(MediaGalleriesPreferencesTest, GalleryManagement) {
  MediaGalleryPrefId auto_id, user_added_id, scan_id, id;
  base::FilePath path;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detected gallery.
  path = MakeMediaGalleriesTestingPath("new_auto");
  StorageInfo info;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  std::u16string gallery_name = u"NewAutoGallery";
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  auto_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Add it as other types, nothing should happen.
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(auto_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(auto_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(auto_id, id);

  // Add a new user added gallery.
  path = MakeMediaGalleriesTestingPath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  gallery_name = u"NewUserGallery";
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 2UL, id);
  user_added_id = id;
  const std::string user_added_device_id = info.device_id();
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  // Add it as other types, nothing should happen.
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(user_added_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(user_added_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(user_added_id, id);
  Verify();

  // Add a new scan result gallery.
  path = MakeMediaGalleriesTestingPath("new_scan");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  gallery_name = u"NewScanGallery";
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 3UL, id);
  scan_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kScanResult);
  Verify();

  // Add it as other types, nothing should happen.
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(scan_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(scan_id, id);
  Verify();
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(scan_id, id);
  Verify();

  // Lookup some galleries.
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_auto"), nullptr));
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_user"), nullptr));
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_scan"), nullptr));
  EXPECT_FALSE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("other"), nullptr));

  // Check that we always get the gallery info.
  MediaGalleryPrefInfo gallery_info;
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_auto"), &gallery_info));
  VerifyGalleryInfo(gallery_info, auto_id);
  EXPECT_FALSE(gallery_info.volume_metadata_valid);
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_user"), &gallery_info));
  VerifyGalleryInfo(gallery_info, user_added_id);
  EXPECT_FALSE(gallery_info.volume_metadata_valid);
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_scan"), &gallery_info));
  VerifyGalleryInfo(gallery_info, scan_id);
  EXPECT_FALSE(gallery_info.volume_metadata_valid);

  path = MakeMediaGalleriesTestingPath("other");
  EXPECT_FALSE(gallery_prefs()->LookUpGalleryByPath(path, &gallery_info));
  EXPECT_EQ(kInvalidMediaGalleryPrefId, gallery_info.pref_id);

  StorageInfo other_info;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &other_info, &relative_path);
  EXPECT_EQ(other_info.device_id(), gallery_info.device_id);
  EXPECT_EQ(relative_path.value(), gallery_info.path.value());

  // Remove an auto added gallery (i.e. make it blocklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlockListed;
  expected_galleries_for_all.erase(auto_id);
  Verify();

  // Remove a scan result (i.e. make it blocklisted).
  gallery_prefs()->ForgetGalleryById(scan_id);
  expected_galleries_[scan_id].type = MediaGalleryPrefInfo::kRemovedScan;
  Verify();

  // Remove a user added gallery and it should go away.
  gallery_prefs()->ForgetGalleryById(user_added_id);
  expected_galleries_.erase(user_added_id);
  expected_device_map[user_added_device_id].erase(user_added_id);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, ForgetAndErase) {
  MediaGalleryPrefId user_erase = AddFixedGalleryWithExpectation(
      "user_erase", "UserErase", MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 1UL, user_erase);
  MediaGalleryPrefId user_forget = AddFixedGalleryWithExpectation(
      "user_forget", "UserForget", MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 2UL, user_forget);

  MediaGalleryPrefId auto_erase = AddFixedGalleryWithExpectation(
      "auto_erase", "AutoErase", MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(default_galleries_count() + 3UL, auto_erase);
  MediaGalleryPrefId auto_forget = AddFixedGalleryWithExpectation(
      "auto_forget", "AutoForget", MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(default_galleries_count() + 4UL, auto_forget);

  MediaGalleryPrefId scan_erase = AddFixedGalleryWithExpectation(
      "scan_erase", "ScanErase", MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 5UL, scan_erase);
  MediaGalleryPrefId scan_forget = AddFixedGalleryWithExpectation(
      "scan_forget", "ScanForget", MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 6UL, scan_forget);

  Verify();
  std::string device_id;

  gallery_prefs()->ForgetGalleryById(user_forget);
  device_id = expected_galleries_[user_forget].device_id;
  expected_galleries_.erase(user_forget);
  expected_device_map[device_id].erase(user_forget);
  Verify();

  gallery_prefs()->ForgetGalleryById(auto_forget);
  expected_galleries_[auto_forget].type = MediaGalleryPrefInfo::kBlockListed;
  expected_galleries_for_all.erase(auto_forget);
  Verify();

  gallery_prefs()->ForgetGalleryById(scan_forget);
  expected_galleries_[scan_forget].type = MediaGalleryPrefInfo::kRemovedScan;
  Verify();

  gallery_prefs()->EraseGalleryById(user_erase);
  device_id = expected_galleries_[user_erase].device_id;
  expected_galleries_.erase(user_erase);
  expected_device_map[device_id].erase(user_erase);
  Verify();

  gallery_prefs()->EraseGalleryById(auto_erase);
  device_id = expected_galleries_[auto_erase].device_id;
  expected_galleries_.erase(auto_erase);
  expected_device_map[device_id].erase(auto_erase);
  expected_galleries_for_all.erase(auto_erase);
  Verify();

  gallery_prefs()->EraseGalleryById(scan_erase);
  device_id = expected_galleries_[scan_erase].device_id;
  expected_galleries_.erase(scan_erase);
  expected_device_map[device_id].erase(scan_erase);
  Verify();

  // Also erase the previously forgetten ones to check erasing blocklisted ones.
  gallery_prefs()->EraseGalleryById(auto_forget);
  device_id = expected_galleries_[auto_forget].device_id;
  expected_galleries_.erase(auto_forget);
  expected_device_map[device_id].erase(auto_forget);
  Verify();

  gallery_prefs()->EraseGalleryById(scan_forget);
  device_id = expected_galleries_[scan_forget].device_id;
  expected_galleries_.erase(scan_forget);
  expected_device_map[device_id].erase(scan_forget);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, AddGalleryWithVolumeMetadata) {
  MediaGalleryPrefId id;
  StorageInfo info;
  base::FilePath path;
  base::FilePath relative_path;
  base::Time now = base::Time::Now();
  Verify();

  // Add a new auto detected gallery.
  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  id = gallery_prefs()->AddGallery(
      info.device_id(), relative_path, MediaGalleryPrefInfo::kAutoDetected,
      u"volume label", u"vendor name", u"model name", 1000000ULL, now, 0, 0, 0);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddGalleryExpectation(id, std::u16string(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  MediaGalleryPrefInfo gallery_info;
  EXPECT_TRUE(gallery_prefs()->LookUpGalleryByPath(
      MakeMediaGalleriesTestingPath("new_auto"), &gallery_info));
  EXPECT_TRUE(gallery_info.volume_metadata_valid);
  EXPECT_EQ(u"volume label", gallery_info.volume_label);
  EXPECT_EQ(u"vendor name", gallery_info.vendor_name);
  EXPECT_EQ(u"model name", gallery_info.model_name);
  EXPECT_EQ(1000000ULL, gallery_info.total_size_in_bytes);
  // Note: we put the microseconds time into a double, so there'll
  // be some possible rounding errors. If it's less than 100, we don't
  // care.
  EXPECT_LE(std::abs(now.ToInternalValue() -
                     gallery_info.last_attach_time.ToInternalValue()),
            100);
}

TEST_F(MediaGalleriesPreferencesTest, ReplaceGalleryWithVolumeMetadata) {
  MediaGalleryPrefId id, metadata_id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  base::Time now = base::Time::Now();
  Verify();

  // Add an auto detected gallery in the prefs version 0 format.
  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  std::u16string gallery_name = u"NewAutoGallery";
  id = AddGalleryWithNameV0(info.device_id(), gallery_name, relative_path,
                            false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  metadata_id = gallery_prefs()->AddGallery(
      info.device_id(), relative_path, MediaGalleryPrefInfo::kAutoDetected,
      u"volume label", u"vendor name", u"model name", 1000000ULL, now, 0, 0, 0);
  EXPECT_EQ(id, metadata_id);
  AddGalleryExpectation(id, std::u16string(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);

  // Make sure the display_name is set to empty now, as the metadata
  // upgrade should set the manual override name empty.
  Verify();
}

// Whenever an "AutoDetected" gallery is removed, it is moved to a block listed
// state.  When the gallery is added again, the block listed state is updated
// back to the "AutoDetected" type.
TEST_F(MediaGalleriesPreferencesTest, AutoAddedBlockListing) {
  MediaGalleryPrefId auto_id, id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detect gallery to test with.
  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  std::u16string gallery_name = u"NewAutoGallery";
  id = AddGalleryWithNameV1(info.device_id(), gallery_name,
                            relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  auto_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Remove an auto added gallery (i.e. make it blocklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlockListed;
  expected_galleries_for_all.erase(auto_id);
  Verify();

  // Try adding the gallery again automatically and it should be a no-op.
  id = AddGalleryWithNameV1(info.device_id(), gallery_name, relative_path,
                            false /*auto*/);
  EXPECT_EQ(auto_id, id);
  Verify();

  // Add the gallery again as a user action.
  id = gallery_prefs()->AddGalleryByPath(path,
                                         MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(auto_id, id);
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();
}

// Whenever a "ScanResult" gallery is removed, it is moved to a block listed
// state.  When the gallery is added again, the block listed state is updated
// back to the "ScanResult" type.
TEST_F(MediaGalleriesPreferencesTest, ScanResultBlockListing) {
  MediaGalleryPrefId scan_id, id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add a new scan result gallery to test with.
  path = MakeMediaGalleriesTestingPath("new_scan");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  std::u16string gallery_name = u"NewScanGallery";
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  scan_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kScanResult);
  Verify();

  // Remove a scan result gallery (i.e. make it blocklisted).
  gallery_prefs()->ForgetGalleryById(scan_id);
  expected_galleries_[scan_id].type = MediaGalleryPrefInfo::kRemovedScan;
  expected_galleries_for_all.erase(scan_id);
  Verify();

  // Try adding the gallery again as a scan result it should be a no-op.
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(scan_id, id);
  Verify();

  // Add the gallery again as a user action.
  id = gallery_prefs()->AddGalleryByPath(path,
                                         MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(scan_id, id);
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, UpdateGalleryNameV2) {
  // Add a new auto detect gallery to test with.
  base::FilePath path = MakeMediaGalleriesTestingPath("new_auto");
  StorageInfo info;
  base::FilePath relative_path;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  std::u16string gallery_name = u"NewAutoGallery";
  MediaGalleryPrefId id =
      AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                           MediaGalleryPrefInfo::kAutoDetected);
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Won't override the name -- don't change any expectation.
  gallery_name = std::u16string();
  AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                       MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  gallery_name = u"NewName";
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kAutoDetected);
  // Note: will really just update the existing expectation.
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, GalleryPermissions) {
  MediaGalleryPrefId auto_id, user_added_id, to_blocklist_id, scan_id,
      to_scan_remove_id, id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add some galleries to test with.
  path = MakeMediaGalleriesTestingPath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  std::u16string gallery_name = u"NewUserGallery";
  id = AddGalleryWithNameV1(info.device_id(), gallery_name, relative_path,
                            true /*user*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  user_added_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  gallery_name = u"NewAutoGallery";
  id = AddGalleryWithNameV1(info.device_id(), gallery_name, relative_path,
                            false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 2UL, id);
  auto_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  path = MakeMediaGalleriesTestingPath("to_blocklist");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  gallery_name = u"ToBlocklistGallery";
  id = AddGalleryWithNameV1(info.device_id(), gallery_name, relative_path,
                            false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 3UL, id);
  to_blocklist_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  path = MakeMediaGalleriesTestingPath("new_scan");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  gallery_name = u"NewScanGallery";
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 4UL, id);
  scan_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kScanResult);
  Verify();

  path = MakeMediaGalleriesTestingPath("to_scan_remove");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  gallery_name = u"ToScanRemoveGallery";
  id = AddGalleryWithNameV2(info.device_id(), gallery_name, relative_path,
                            MediaGalleryPrefInfo::kScanResult);
  EXPECT_EQ(default_galleries_count() + 5UL, id);
  to_scan_remove_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kScanResult);
  Verify();

  // Remove permission for all galleries from the all-permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), auto_id, false);
  expected_galleries_for_all.erase(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), user_added_id, false);
  expected_galleries_for_all.erase(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_blocklist_id, false);
  expected_galleries_for_all.erase(to_blocklist_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), scan_id, false);
  expected_galleries_for_all.erase(scan_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_scan_remove_id, false);
  expected_galleries_for_all.erase(to_scan_remove_id);
  Verify();

  // Add permission back for all galleries to the all-permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), auto_id, true);
  expected_galleries_for_all.insert(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), user_added_id, true);
  expected_galleries_for_all.insert(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_blocklist_id, true);
  expected_galleries_for_all.insert(to_blocklist_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), scan_id, true);
  expected_galleries_for_all.insert(scan_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *all_permission_extension.get(), to_scan_remove_id, true);
  expected_galleries_for_all.insert(to_scan_remove_id);
  Verify();

  // Add permission for all galleries to the regular permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), auto_id, true);
  expected_galleries_for_regular.insert(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), user_added_id, true);
  expected_galleries_for_regular.insert(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), to_blocklist_id, true);
  expected_galleries_for_regular.insert(to_blocklist_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), scan_id, true);
  expected_galleries_for_regular.insert(scan_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), to_scan_remove_id, true);
  expected_galleries_for_regular.insert(to_scan_remove_id);
  Verify();

  // Blocklist the to be block listed gallery
  gallery_prefs()->ForgetGalleryById(to_blocklist_id);
  expected_galleries_[to_blocklist_id].type =
      MediaGalleryPrefInfo::kBlockListed;
  expected_galleries_for_all.erase(to_blocklist_id);
  expected_galleries_for_regular.erase(to_blocklist_id);
  Verify();

  gallery_prefs()->ForgetGalleryById(to_scan_remove_id);
  expected_galleries_[to_scan_remove_id].type =
      MediaGalleryPrefInfo::kRemovedScan;
  expected_galleries_for_all.erase(to_scan_remove_id);
  expected_galleries_for_regular.erase(to_scan_remove_id);
  Verify();

  // Remove permission for all galleries to the regular permission extension.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), auto_id, false);
  expected_galleries_for_regular.erase(auto_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), user_added_id, false);
  expected_galleries_for_regular.erase(user_added_id);
  Verify();

  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), scan_id, false);
  expected_galleries_for_regular.erase(scan_id);
  Verify();

  // Add permission for an invalid gallery id.
  gallery_prefs()->SetGalleryPermissionForExtension(
      *regular_permission_extension.get(), 9999L, true);
  Verify();
}

// What an existing gallery is added again, update the gallery information if
// needed.
TEST_F(MediaGalleriesPreferencesTest, UpdateGalleryDetails) {
  MediaGalleryPrefId auto_id, id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add a new auto detect gallery to test with.
  path = MakeMediaGalleriesTestingPath("new_auto");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  std::u16string gallery_name = u"NewAutoGallery";
  id = AddGalleryWithNameV1(info.device_id(), gallery_name,
                            relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  auto_id = id;
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();

  // Update the device name and add the gallery again.
  gallery_name = u"AutoGallery2";
  id = AddGalleryWithNameV1(info.device_id(), gallery_name, relative_path,
                            false /*auto*/);
  EXPECT_EQ(auto_id, id);
  AddGalleryExpectation(id, gallery_name, info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kAutoDetected);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, MultipleGalleriesPerDevices) {
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  Verify();

  // Add a regular gallery
  path = MakeMediaGalleriesTestingPath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  std::u16string gallery_name = u"NewUserGallery";
  MediaGalleryPrefId user_added_id =
      AddGalleryWithNameV1(info.device_id(), gallery_name, relative_path,
                           true /*user*/);
  EXPECT_EQ(default_galleries_count() + 1UL, user_added_id);
  AddGalleryExpectation(user_added_id, gallery_name, info.device_id(),
                        relative_path, MediaGalleryPrefInfo::kUserAdded);
  Verify();

  // Find it by device id and fail to find something related.
  MediaGalleryPrefIdSet pref_id_set;
  pref_id_set = gallery_prefs()->LookUpGalleriesByDeviceId(info.device_id());
  EXPECT_EQ(1U, pref_id_set.size());
  EXPECT_TRUE(pref_id_set.find(user_added_id) != pref_id_set.end());

  MediaStorageUtil::GetDeviceInfoFromPath(
      MakeMediaGalleriesTestingPath("new_user/foo"), &info, &relative_path);
  pref_id_set = gallery_prefs()->LookUpGalleriesByDeviceId(info.device_id());
  EXPECT_EQ(0U, pref_id_set.size());

  // Add some galleries on the same device.
  relative_path = base::FilePath(FILE_PATH_LITERAL("path1/on/device1"));
  gallery_name = u"Device1Path1";
  std::string device_id = "path:device1";
  MediaGalleryPrefId dev1_path1_id = AddGalleryWithNameV1(
      device_id, gallery_name, relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 2UL, dev1_path1_id);
  AddGalleryExpectation(dev1_path1_id, gallery_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  relative_path = base::FilePath(FILE_PATH_LITERAL("path2/on/device1"));
  gallery_name = u"Device1Path2";
  MediaGalleryPrefId dev1_path2_id = AddGalleryWithNameV1(
      device_id, gallery_name, relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 3UL, dev1_path2_id);
  AddGalleryExpectation(dev1_path2_id, gallery_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  relative_path = base::FilePath(FILE_PATH_LITERAL("path1/on/device2"));
  gallery_name = u"Device2Path1";
  device_id = "path:device2";
  MediaGalleryPrefId dev2_path1_id = AddGalleryWithNameV1(
      device_id, gallery_name, relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 4UL, dev2_path1_id);
  AddGalleryExpectation(dev2_path1_id, gallery_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  relative_path = base::FilePath(FILE_PATH_LITERAL("path2/on/device2"));
  gallery_name = u"Device2Path2";
  MediaGalleryPrefId dev2_path2_id = AddGalleryWithNameV1(
      device_id, gallery_name, relative_path, true /*user*/);
  EXPECT_EQ(default_galleries_count() + 5UL, dev2_path2_id);
  AddGalleryExpectation(dev2_path2_id, gallery_name, device_id, relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();

  // Check that adding one of them again works as expected.
  MediaGalleryPrefId id = AddGalleryWithNameV1(
      device_id, gallery_name, relative_path, true /*user*/);
  EXPECT_EQ(dev2_path2_id, id);
  Verify();
}

TEST_F(MediaGalleriesPreferencesTest, GalleryChangeObserver) {
  // Start with one observer.
  MockGalleryChangeObserver observer1(gallery_prefs());
  gallery_prefs()->AddGalleryChangeObserver(&observer1);

  // Add a new auto detected gallery.
  base::FilePath path = MakeMediaGalleriesTestingPath("new_auto");
  StorageInfo info;
  base::FilePath relative_path;
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  std::u16string gallery_name = u"NewAutoGallery";
  MediaGalleryPrefId auto_id = AddGalleryWithNameV1(
      info.device_id(), gallery_name, relative_path, false /*auto*/);
  EXPECT_EQ(default_galleries_count() + 1UL, auto_id);
  AddGalleryExpectation(auto_id, gallery_name, info.device_id(),
                        relative_path, MediaGalleryPrefInfo::kAutoDetected);
  EXPECT_EQ(1, observer1.notifications());

  // Add a second observer.
  MockGalleryChangeObserver observer2(gallery_prefs());
  gallery_prefs()->AddGalleryChangeObserver(&observer2);

  // Add a new user added gallery.
  path = MakeMediaGalleriesTestingPath("new_user");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  gallery_name = u"NewUserGallery";
  MediaGalleryPrefId user_added_id =
      AddGalleryWithNameV1(info.device_id(), gallery_name, relative_path,
                           true /*user*/);
  AddGalleryExpectation(user_added_id, gallery_name, info.device_id(),
                        relative_path, MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 2UL, user_added_id);
  EXPECT_EQ(2, observer1.notifications());
  EXPECT_EQ(1, observer2.notifications());

  // Remove the first observer.
  gallery_prefs()->RemoveGalleryChangeObserver(&observer1);

  // Remove an auto added gallery (i.e. make it blocklisted).
  gallery_prefs()->ForgetGalleryById(auto_id);
  expected_galleries_[auto_id].type = MediaGalleryPrefInfo::kBlockListed;
  expected_galleries_for_all.erase(auto_id);

  EXPECT_EQ(2, observer1.notifications());
  EXPECT_EQ(2, observer2.notifications());

  // Remove a user added gallery and it should go away.
  gallery_prefs()->ForgetGalleryById(user_added_id);
  expected_galleries_.erase(user_added_id);
  expected_device_map[info.device_id()].erase(user_added_id);

  EXPECT_EQ(2, observer1.notifications());
  EXPECT_EQ(3, observer2.notifications());
}

TEST_F(MediaGalleriesPreferencesTest, ScanResults) {
  MediaGalleryPrefId id;
  base::FilePath path;
  StorageInfo info;
  base::FilePath relative_path;
  base::Time now = base::Time::Now();
  Verify();

  // Add a new scan result gallery to test with.
  path = MakeMediaGalleriesTestingPath("new_scan");
  MediaStorageUtil::GetDeviceInfoFromPath(path, &info, &relative_path);
  id = gallery_prefs()->AddGallery(
      info.device_id(), relative_path, MediaGalleryPrefInfo::kScanResult,
      u"volume label", u"vendor name", u"model name", 1000000ULL, now, 1, 2, 3);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddScanResultExpectation(id, std::u16string(), info.device_id(),
                           relative_path, 1, 2, 3);
  Verify();

  // Update the found media count.
  id = gallery_prefs()->AddGallery(
      info.device_id(), relative_path, MediaGalleryPrefInfo::kScanResult,
      u"volume label", u"vendor name", u"model name", 1000000ULL, now, 4, 5, 6);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddScanResultExpectation(id, std::u16string(), info.device_id(),
                           relative_path, 4, 5, 6);
  Verify();

  // Remove a scan result (i.e. make it blocklisted).
  gallery_prefs()->ForgetGalleryById(id);
  expected_galleries_[id].type = MediaGalleryPrefInfo::kRemovedScan;
  expected_galleries_[id].audio_count = 0;
  expected_galleries_[id].image_count = 0;
  expected_galleries_[id].video_count = 0;
  Verify();

  // Try adding the gallery again as a scan result it should be a no-op.
  id = gallery_prefs()->AddGallery(
      info.device_id(), relative_path, MediaGalleryPrefInfo::kScanResult,
      u"volume label", u"vendor name", u"model name", 1000000ULL, now, 7, 8, 9);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  Verify();

  // Add the gallery again as a user action.
  id = gallery_prefs()->AddGalleryByPath(path,
                                         MediaGalleryPrefInfo::kUserAdded);
  EXPECT_EQ(default_galleries_count() + 1UL, id);
  AddGalleryExpectation(id, std::u16string(), info.device_id(), relative_path,
                        MediaGalleryPrefInfo::kUserAdded);
  Verify();
}

TEST(MediaGalleriesPrefInfoTest, NameGeneration) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);

  ASSERT_TRUE(TestStorageMonitor::CreateAndInstall());

  MediaGalleryPrefInfo info;
  info.pref_id = 1;
  info.display_name = u"override";
  info.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM, "unique");

  EXPECT_EQ(u"override", info.GetGalleryDisplayName());

  info.display_name = u"o2";
  EXPECT_EQ(u"o2", info.GetGalleryDisplayName());

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_MEDIA_GALLERIES_DIALOG_DEVICE_NOT_ATTACHED),
            info.GetGalleryAdditionalDetails());

  info.last_attach_time = base::Time::Now();
  EXPECT_NE(l10n_util::GetStringUTF16(
                IDS_MEDIA_GALLERIES_DIALOG_DEVICE_NOT_ATTACHED),
            info.GetGalleryAdditionalDetails());
  EXPECT_NE(l10n_util::GetStringUTF16(
                IDS_MEDIA_GALLERIES_DIALOG_DEVICE_ATTACHED),
            info.GetGalleryAdditionalDetails());

  info.volume_label = u"vol";
  info.vendor_name = u"vendor";
  info.model_name = u"model";
  EXPECT_EQ(u"o2", info.GetGalleryDisplayName());

  info.display_name = std::u16string();
  EXPECT_EQ(u"vol", info.GetGalleryDisplayName());
  info.volume_label = std::u16string();
  EXPECT_EQ(u"vendor, model", info.GetGalleryDisplayName());

  info.device_id = StorageInfo::MakeDeviceId(
      StorageInfo::FIXED_MASS_STORAGE, "unique");
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("unique")).AsUTF8Unsafe(),
            base::UTF16ToUTF8(info.GetGalleryTooltip()));

  TestStorageMonitor::Destroy();
}

TEST_F(MediaGalleriesPreferencesTest, SetsDefaultGalleryTypeField) {
  // Tests that default galleries (Music, Pictures, Video) have the correct
  // default_gallery field set.

  // No default galleries exist on CrOS so this test isn't relevant there.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath music_path;
  base::FilePath pictures_path;
  base::FilePath videos_path;
  bool got_music_path =
      base::PathService::Get(chrome::DIR_USER_MUSIC, &music_path);
  bool got_pictures_path =
      base::PathService::Get(chrome::DIR_USER_PICTURES, &pictures_path);
  bool got_videos_path =
      base::PathService::Get(chrome::DIR_USER_VIDEOS, &videos_path);

  int num_default_galleries = 0;

  const MediaGalleriesPrefInfoMap& known_galleries =
      gallery_prefs()->known_galleries();
  for (auto it = known_galleries.begin(); it != known_galleries.end(); ++it) {
    if (it->second.type != MediaGalleryPrefInfo::kAutoDetected)
      continue;

    std::string unique_id;
    if (!StorageInfo::CrackDeviceId(it->second.device_id, NULL, &unique_id))
      continue;

    if (got_music_path && unique_id == music_path.AsUTF8Unsafe()) {
      EXPECT_EQ(MediaGalleryPrefInfo::DefaultGalleryType::kMusicDefault,
                it->second.default_gallery_type);
      num_default_galleries++;
    } else if (got_pictures_path && unique_id == pictures_path.AsUTF8Unsafe()) {
      EXPECT_EQ(MediaGalleryPrefInfo::DefaultGalleryType::kPicturesDefault,
                it->second.default_gallery_type);
      num_default_galleries++;
    } else if (got_videos_path && unique_id == videos_path.AsUTF8Unsafe()) {
      EXPECT_EQ(MediaGalleryPrefInfo::DefaultGalleryType::kVideosDefault,
                it->second.default_gallery_type);
      num_default_galleries++;
    } else {
      EXPECT_EQ(MediaGalleryPrefInfo::DefaultGalleryType::kNotDefault,
                it->second.default_gallery_type);
    }
  }

  EXPECT_EQ(3, num_default_galleries);
#endif
}

TEST_F(MediaGalleriesPreferencesTest, UpdatesDefaultGalleryType) {
  // Tests that if the path of a default gallery changed since last init,
  // then when the MediaGalleriesPreferences is initialized, it will
  // rewrite the device ID in prefs to include the new path.

  // No default galleries exist on CrOS so this test isn't relevant there.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath old_music_path;
  base::FilePath old_pictures_path;
  base::FilePath old_videos_path;
  bool got_old_music_path =
      base::PathService::Get(chrome::DIR_USER_MUSIC, &old_music_path);
  bool got_old_pictures_path =
      base::PathService::Get(chrome::DIR_USER_PICTURES, &old_pictures_path);
  bool got_old_videos_path =
      base::PathService::Get(chrome::DIR_USER_VIDEOS, &old_videos_path);

  bool found_music = false;
  bool found_pictures = false;
  bool found_videos = false;

  const MediaGalleriesPrefInfoMap& old_known_galleries =
      gallery_prefs()->known_galleries();
  for (auto it = old_known_galleries.begin(); it != old_known_galleries.end();
       ++it) {
    if (it->second.type == MediaGalleryPrefInfo::kAutoDetected) {
      std::string unique_id;
      if (!StorageInfo::CrackDeviceId(it->second.device_id, NULL, &unique_id))
        continue;

      if (got_old_music_path &&
          it->second.default_gallery_type ==
          MediaGalleryPrefInfo::DefaultGalleryType::kMusicDefault) {
        EXPECT_EQ(old_music_path.AsUTF8Unsafe(), unique_id);
        found_music = true;
      } else if (got_old_pictures_path &&
                 it->second.default_gallery_type ==
                 MediaGalleryPrefInfo::DefaultGalleryType::kPicturesDefault) {
        EXPECT_EQ(old_pictures_path.AsUTF8Unsafe(), unique_id);
        found_pictures = true;
      } else if (got_old_videos_path &&
                 it->second.default_gallery_type ==
                 MediaGalleryPrefInfo::DefaultGalleryType::kVideosDefault) {
        EXPECT_EQ(old_videos_path.AsUTF8Unsafe(), unique_id);
        found_videos = true;
      }
    }
  }

  EXPECT_TRUE(found_music);
  EXPECT_TRUE(found_pictures);
  EXPECT_TRUE(found_videos);

  ChangeMediaPathOverrides();
  ReinitPrefsAndExpectations();

  base::FilePath new_music_path;
  base::FilePath new_pictures_path;
  base::FilePath new_videos_path;
  bool got_new_music_path =
      base::PathService::Get(chrome::DIR_USER_MUSIC, &new_music_path);
  bool got_new_pictures_path =
      base::PathService::Get(chrome::DIR_USER_PICTURES, &new_pictures_path);
  bool got_new_videos_path =
      base::PathService::Get(chrome::DIR_USER_VIDEOS, &new_videos_path);

  EXPECT_NE(new_music_path, old_music_path);
  EXPECT_NE(new_pictures_path, old_pictures_path);
  EXPECT_NE(new_videos_path, old_videos_path);

  found_music = false;
  found_pictures = false;
  found_videos = false;

  const MediaGalleriesPrefInfoMap& known_galleries =
      gallery_prefs()->known_galleries();
  for (auto it = known_galleries.begin(); it != known_galleries.end(); ++it) {
    if (it->second.type == MediaGalleryPrefInfo::kAutoDetected) {
      std::string unique_id;
      if (!StorageInfo::CrackDeviceId(it->second.device_id, NULL, &unique_id))
        continue;

      if (got_new_music_path &&
          it->second.default_gallery_type ==
          MediaGalleryPrefInfo::DefaultGalleryType::kMusicDefault) {
        EXPECT_EQ(new_music_path.AsUTF8Unsafe(), unique_id);
        found_music = true;
      } else if (got_new_pictures_path &&
                 it->second.default_gallery_type ==
                 MediaGalleryPrefInfo::DefaultGalleryType::kPicturesDefault) {
        EXPECT_EQ(new_pictures_path.AsUTF8Unsafe(), unique_id);
        found_pictures = true;
      } else if (got_new_videos_path &&
                 it->second.default_gallery_type ==
                 MediaGalleryPrefInfo::DefaultGalleryType::kVideosDefault) {
        EXPECT_EQ(new_videos_path.AsUTF8Unsafe(), unique_id);
        found_videos = true;
      }
    }
  }

  EXPECT_TRUE(found_music);
  EXPECT_TRUE(found_pictures);
  EXPECT_TRUE(found_videos);
#endif
}

TEST_F(MediaGalleriesPreferencesTest, UpdateAddsDefaultGalleryTypeIfMissing) {
  // Tests that if no default_gallery_type was specified for an existing prefs
  // info object corresponding to a particular gallery, then when the
  // MediaGalleriesPreferences is initialized, it assigns the proper one.

  // No default galleries exist on CrOS so this test isn't relevant there.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Add a new user added gallery.
  AddFixedGalleryWithExpectation("user_added", "UserAdded",
                                 MediaGalleryPrefInfo::kUserAdded);

  // Remove the "default_gallery_type" field completely from the persisted data
  // for the prefs info object. This simulates the case where a user updated
  // Chrome from a version without that field to one with it.
  RemovePersistedDefaultGalleryValues();

  // Reinitializing the MediaGalleriesPreferences should populate the
  // default_gallery_type field with the correct value for each gallery.
  ReinitPrefsAndExpectations();

  base::FilePath music_path;
  base::FilePath pictures_path;
  base::FilePath videos_path;
  bool got_music_path =
      base::PathService::Get(chrome::DIR_USER_MUSIC, &music_path);
  bool got_pictures_path =
      base::PathService::Get(chrome::DIR_USER_PICTURES, &pictures_path);
  bool got_videos_path =
      base::PathService::Get(chrome::DIR_USER_VIDEOS, &videos_path);

  bool found_music = false;
  bool found_pictures = false;
  bool found_videos = false;
  bool found_user_added = false;

  const MediaGalleriesPrefInfoMap& known_galleries =
      gallery_prefs()->known_galleries();
  for (auto it = known_galleries.begin(); it != known_galleries.end(); ++it) {
    std::string unique_id;
    if (!StorageInfo::CrackDeviceId(it->second.device_id, NULL, &unique_id))
      continue;

    if (got_music_path &&
        it->second.default_gallery_type ==
        MediaGalleryPrefInfo::DefaultGalleryType::kMusicDefault) {
      EXPECT_EQ(music_path.AsUTF8Unsafe(), unique_id);
      found_music = true;
    } else if (got_pictures_path &&
               it->second.default_gallery_type ==
               MediaGalleryPrefInfo::DefaultGalleryType::kPicturesDefault) {
      EXPECT_EQ(pictures_path.AsUTF8Unsafe(), unique_id);
      found_pictures = true;
    } else if (got_videos_path &&
               it->second.default_gallery_type ==
               MediaGalleryPrefInfo::DefaultGalleryType::kVideosDefault) {
      EXPECT_EQ(videos_path.AsUTF8Unsafe(), unique_id);
      found_videos = true;
    } else if (it->second.default_gallery_type ==
               MediaGalleryPrefInfo::DefaultGalleryType::kNotDefault) {
      found_user_added = true;
    }
  }

  EXPECT_TRUE(found_music);
  EXPECT_TRUE(found_pictures);
  EXPECT_TRUE(found_videos);
  EXPECT_TRUE(found_user_added);
#endif
}
