// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry unit tests.

#include "chrome/browser/media_galleries/media_file_system_registry.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media_galleries/media_file_system_context.h"
#include "chrome/browser/media_galleries/media_galleries_preferences_factory.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/removable_device_constants.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif

namespace content {
class SiteInstance;
}

using content::BrowserThread;
using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;
using storage_monitor::TestStorageMonitor;

// Not anonymous so it can be friends with MediaFileSystemRegistry.
class TestMediaFileSystemContext : public MediaFileSystemContext {
 public:
  struct FSInfo {
    FSInfo() {}
    FSInfo(const std::string& device_id, const base::FilePath& path,
           const std::string& fs_name);

    bool operator<(const FSInfo& other) const;

    std::string device_id;
    base::FilePath path;
    std::string fs_name;
  };

  explicit TestMediaFileSystemContext(MediaFileSystemRegistry* registry);
  ~TestMediaFileSystemContext() override {}

  // MediaFileSystemContext implementation.
  bool RegisterFileSystem(const std::string& device_id,
                          const std::string& fs_name,
                          const base::FilePath& path) override;

  void RevokeFileSystem(const std::string& fs_name) override;

  base::FilePath GetRegisteredPath(const std::string& fs_name) const override;

  MediaFileSystemRegistry* registry() { return registry_; }

 private:
  void AddFSEntry(const std::string& device_id,
                  const base::FilePath& path,
                  const std::string& fs_name);

  MediaFileSystemRegistry* registry_;

  // The currently allocated mock file systems.
  std::map<std::string /*fs_name*/, FSInfo> file_systems_by_name_;
};

TestMediaFileSystemContext::FSInfo::FSInfo(const std::string& device_id,
                                           const base::FilePath& path,
                                           const std::string& fs_name)
    : device_id(device_id),
      path(path),
      fs_name(fs_name) {
}

bool TestMediaFileSystemContext::FSInfo::operator<(const FSInfo& other) const {
  if (device_id != other.device_id)
    return device_id < other.device_id;
  if (path.value() != other.path.value())
    return path.value() < other.path.value();
  return fs_name < other.fs_name;
}

TestMediaFileSystemContext::TestMediaFileSystemContext(
    MediaFileSystemRegistry* registry)
    : registry_(registry) {
  registry_->file_system_context_.reset(this);
}

bool TestMediaFileSystemContext::RegisterFileSystem(
    const std::string& device_id,
    const std::string& fs_name,
    const base::FilePath& path) {
  AddFSEntry(device_id, path, fs_name);
  return true;
}

void TestMediaFileSystemContext::RevokeFileSystem(const std::string& fs_name) {
  if (!base::Contains(file_systems_by_name_, fs_name))
    return;
  EXPECT_EQ(1U, file_systems_by_name_.erase(fs_name));
}

base::FilePath TestMediaFileSystemContext::GetRegisteredPath(
    const std::string& fs_name) const {
  auto it = file_systems_by_name_.find(fs_name);
  if (it == file_systems_by_name_.end())
    return base::FilePath();
  return it->second.path;
}

void TestMediaFileSystemContext::AddFSEntry(const std::string& device_id,
                                            const base::FilePath& path,
                                            const std::string& fs_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(path.IsAbsolute());
  DCHECK(!path.ReferencesParent());

  FSInfo info(device_id, path, fs_name);
  file_systems_by_name_[fs_name] = info;
}

namespace {

typedef std::map<MediaGalleryPrefId, MediaFileSystemInfo> FSInfoMap;

void GetGalleryInfoCallback(
    FSInfoMap* results,
    const std::vector<MediaFileSystemInfo>& file_systems) {
  for (size_t i = 0; i < file_systems.size(); ++i) {
    ASSERT_FALSE(base::Contains(*results, file_systems[i].pref_id));
    (*results)[file_systems[i].pref_id] = file_systems[i];
  }
}

void CheckGalleryInfo(const MediaFileSystemInfo& info,
                      TestMediaFileSystemContext* fs_context,
                      const base::FilePath& path,
                      bool removable,
                      bool media_device) {
  EXPECT_EQ(path, info.path);
  EXPECT_EQ(removable, info.removable);
  EXPECT_EQ(media_device, info.media_device);
  EXPECT_NE(0UL, info.pref_id);

  if (removable)
    EXPECT_NE(0UL, info.transient_device_id.size());
  else
    EXPECT_EQ(0UL, info.transient_device_id.size());

  base::FilePath fsid_path = fs_context->GetRegisteredPath(info.fsid);
  EXPECT_EQ(path, fsid_path);
}

class MockProfileSharedRenderProcessHostFactory
    : public content::RenderProcessHostFactory {
 public:
  MockProfileSharedRenderProcessHostFactory() {}
  ~MockProfileSharedRenderProcessHostFactory() override;

  // RPH created with this factory are owned by it.  If the RPH is destroyed
  // for testing purposes, it must be removed from the factory first.
  content::MockRenderProcessHost* ReleaseRPH(
      content::BrowserContext* browser_context);

  content::RenderProcessHost* CreateRenderProcessHost(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) override;

 private:
  class SharedMockRenderProcessHost : public content::MockRenderProcessHost {
   public:
    explicit SharedMockRenderProcessHost(
        content::BrowserContext* browser_context)
        : content::MockRenderProcessHost(browser_context) {}

    // This test class lies that the process has not been used to allow
    // testing of process sharing/reuse inherent in the unit tests that depend
    // on the MockProfileSharedRenderProcessHostFactory.
    bool HostHasNotBeenUsed() override { return true; }

   private:
    DISALLOW_COPY_AND_ASSIGN(SharedMockRenderProcessHost);
  };

  mutable std::map<content::BrowserContext*,
                   std::unique_ptr<content::MockRenderProcessHost>>
      rph_map_;

  DISALLOW_COPY_AND_ASSIGN(MockProfileSharedRenderProcessHostFactory);
};

class ProfileState {
 public:
  explicit ProfileState(MockProfileSharedRenderProcessHostFactory* rph_factory);
  ~ProfileState();

  MediaGalleriesPreferences* GetMediaGalleriesPrefs();

  void CheckGalleries(
      const std::string& test,
      const std::vector<MediaFileSystemInfo>& regular_extension_galleries,
      const std::vector<MediaFileSystemInfo>& all_extension_galleries);

  FSInfoMap GetGalleriesInfo(extensions::Extension* extension);

  extensions::Extension* all_permission_extension();
  extensions::Extension* regular_permission_extension();
  Profile* profile();

  void AddNameForReadCompare(const base::string16& name);
  void AddNameForAllCompare(const base::string16& name);

 private:
  void CompareResults(const std::string& test,
                      const std::vector<base::string16>& names,
                      const std::vector<MediaFileSystemInfo>& expected,
                      const std::vector<MediaFileSystemInfo>& actual);
  bool ContainsEntry(const MediaFileSystemInfo& info,
                     const std::vector<MediaFileSystemInfo>& container);

  int GetAndClearComparisonCount();

  int num_comparisons_;

  std::unique_ptr<TestingProfile> profile_;

  scoped_refptr<extensions::Extension> all_permission_extension_;
  scoped_refptr<extensions::Extension> regular_permission_extension_;
  scoped_refptr<extensions::Extension> no_permissions_extension_;

  std::unique_ptr<content::WebContents> single_web_contents_;
  std::unique_ptr<content::WebContents> shared_web_contents1_;
  std::unique_ptr<content::WebContents> shared_web_contents2_;

  // The RenderProcessHosts are freed when their respective WebContents /
  // RenderViewHosts go away.
  content::MockRenderProcessHost* single_rph_;
  content::MockRenderProcessHost* shared_rph_;

  std::vector<base::string16> compare_names_read_;
  std::vector<base::string16> compare_names_all_;

  DISALLOW_COPY_AND_ASSIGN(ProfileState);
};

base::string16 GetExpectedFolderName(const base::FilePath& path) {
#if defined(OS_CHROMEOS)
  return path.BaseName().LossyDisplayName();
#else
  return path.LossyDisplayName();
#endif
}

}  // namespace

class MediaFileSystemRegistryTest : public ChromeRenderViewHostTestHarness {
 public:
  MediaFileSystemRegistryTest() = default;

  ~MediaFileSystemRegistryTest() override = default;

  void CreateProfileState(size_t profile_count);

  ProfileState* GetProfileState(size_t i);

  MediaGalleriesPreferences* GetPreferences(Profile* profile);

  base::FilePath empty_dir() {
    return empty_dir_;
  }

  base::FilePath dcim_dir() {
    return dcim_dir_;
  }

  TestMediaFileSystemContext* test_file_system_context() {
    return test_file_system_context_;
  }

  // Create a user added gallery based on the information passed and add it to
  // |profiles|. Returns the device id.
  std::string AddUserGallery(StorageInfo::Type type,
                             const std::string& unique_id,
                             const base::FilePath& path);

  // Returns the device id.
  std::string AttachDevice(StorageInfo::Type type,
                           const std::string& unique_id,
                           const base::FilePath& location);

  void DetachDevice(const std::string& device_id);

  void SetGalleryPermission(ProfileState* profile_state,
                            extensions::Extension* extension,
                            const std::string& device_id,
                            bool has_access);

  void AssertAllAutoAddedGalleries();

  void InitForGalleriesInfoTest(FSInfoMap* galleries_info);

  void CheckNewGalleryInfo(ProfileState* profile_state,
                           const FSInfoMap& galleries_info,
                           const base::FilePath& location,
                           bool removable,
                           bool media_device);

  std::vector<MediaFileSystemInfo> GetAutoAddedGalleries(
      ProfileState* profile_state);

  void ProcessAttach(const std::string& id,
                     const base::string16& name,
                     const base::FilePath::StringType& location) {
    StorageInfo info(id, location, name, base::string16(), base::string16(), 0);
    StorageMonitor::GetInstance()->receiver()->ProcessAttach(info);
  }

  void ProcessDetach(const std::string& id) {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(id);
  }

  MediaFileSystemRegistry* registry() {
    return test_file_system_context_->registry();
  }

  size_t GetExtensionGalleriesHostCount(
      const MediaFileSystemRegistry* registry) const;

  int num_auto_galleries() {
    return media_directories_.num_galleries();
  }

 protected:
  void SetUp() override;
  void TearDown() override;

 private:
  // This makes sure that at least one default gallery exists on the file
  // system.
  EnsureMediaDirectoriesExists media_directories_;

  // Some test gallery directories.
  base::ScopedTempDir galleries_dir_;
  // An empty directory in |galleries_dir_|
  base::FilePath empty_dir_;
  // A directory in |galleries_dir_| with a DCIM directory in it.
  base::FilePath dcim_dir_;

  // MediaFileSystemRegistry owns this.
  TestMediaFileSystemContext* test_file_system_context_;

  // Needed for extension service & friends to work.

#if defined(OS_CHROMEOS)
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
#endif

  MockProfileSharedRenderProcessHostFactory rph_factory_;

  std::vector<std::unique_ptr<ProfileState>> profile_states_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistryTest);
};

namespace {

bool MediaFileSystemInfoComparator(const MediaFileSystemInfo& a,
                                   const MediaFileSystemInfo& b) {
  CHECK_NE(a.name, b.name);  // Name must be unique.
  return a.name < b.name;
}

///////////////////////////////////////////////
// MockProfileSharedRenderProcessHostFactory //
///////////////////////////////////////////////

MockProfileSharedRenderProcessHostFactory::
    ~MockProfileSharedRenderProcessHostFactory() {
}

content::MockRenderProcessHost*
MockProfileSharedRenderProcessHostFactory::ReleaseRPH(
    content::BrowserContext* browser_context) {
  auto existing = rph_map_.find(browser_context);
  if (existing == rph_map_.end())
    return nullptr;
  std::unique_ptr<content::MockRenderProcessHost> result =
      std::move(existing->second);
  rph_map_.erase(existing);
  return result.release();
}

content::RenderProcessHost*
MockProfileSharedRenderProcessHostFactory::CreateRenderProcessHost(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  auto existing = rph_map_.find(browser_context);
  if (existing != rph_map_.end())
    return existing->second.get();
  rph_map_[browser_context] =
      std::make_unique<SharedMockRenderProcessHost>(browser_context);
  return rph_map_[browser_context].get();
}

//////////////////
// ProfileState //
//////////////////

ProfileState::ProfileState(
    MockProfileSharedRenderProcessHostFactory* rph_factory)
    : num_comparisons_(0), profile_(new TestingProfile()) {
  extensions::TestExtensionSystem* extension_system(
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile_.get())));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

  std::vector<std::string> all_permissions;
  all_permissions.push_back("allAutoDetected");
  all_permissions.push_back("read");
  std::vector<std::string> read_permissions;
  read_permissions.push_back("read");

  all_permission_extension_ =
      AddMediaGalleriesApp("all", all_permissions, profile_.get());
  regular_permission_extension_ =
      AddMediaGalleriesApp("regular", read_permissions, profile_.get());
  no_permissions_extension_ =
      AddMediaGalleriesApp("no", read_permissions, profile_.get());

  single_web_contents_ = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), nullptr);
  single_rph_ = rph_factory->ReleaseRPH(profile_.get());

  shared_web_contents1_ = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), nullptr);
  shared_web_contents2_ = content::WebContentsTester::CreateTestWebContents(
      profile_.get(), nullptr);
  shared_rph_ = rph_factory->ReleaseRPH(profile_.get());
}

ProfileState::~ProfileState() {
  // TestExtensionSystem uses DeleteSoon, so we need to delete the profiles
  // and then run the message queue to clean up.  But first we have to
  // delete everything that references the profile.
  single_web_contents_.reset();
  shared_web_contents1_.reset();
  shared_web_contents2_.reset();
  profile_.reset();

  content::RunAllTasksUntilIdle();
}

MediaGalleriesPreferences* ProfileState::GetMediaGalleriesPrefs() {
  MediaGalleriesPreferences* prefs =
      MediaGalleriesPreferencesFactory::GetForProfile(profile_.get());
  base::RunLoop loop;
  prefs->EnsureInitialized(loop.QuitClosure());
  loop.Run();
  return prefs;
}

void ProfileState::CheckGalleries(
    const std::string& test,
    const std::vector<MediaFileSystemInfo>& regular_extension_galleries,
    const std::vector<MediaFileSystemInfo>& all_extension_galleries) {
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();

  // No Media Galleries permissions.
  std::vector<MediaFileSystemInfo> empty_expectation;
  std::vector<base::string16> empty_names;
  registry->GetMediaFileSystemsForExtension(
      single_web_contents_.get(), no_permissions_extension_.get(),
      base::Bind(&ProfileState::CompareResults, base::Unretained(this),
                 base::StringPrintf("%s (no permission)", test.c_str()),
                 std::cref(empty_names), std::cref(empty_expectation)));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetAndClearComparisonCount());

  // Read permission only.
  registry->GetMediaFileSystemsForExtension(
      single_web_contents_.get(), regular_permission_extension_.get(),
      base::Bind(&ProfileState::CompareResults, base::Unretained(this),
                 base::StringPrintf("%s (regular permission)", test.c_str()),
                 std::cref(compare_names_read_),
                 std::cref(regular_extension_galleries)));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetAndClearComparisonCount());

  // All galleries permission.
  registry->GetMediaFileSystemsForExtension(
      single_web_contents_.get(), all_permission_extension_.get(),
      base::Bind(&ProfileState::CompareResults, base::Unretained(this),
                 base::StringPrintf("%s (all permission)", test.c_str()),
                 std::cref(compare_names_all_),
                 std::cref(all_extension_galleries)));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, GetAndClearComparisonCount());
}

FSInfoMap ProfileState::GetGalleriesInfo(extensions::Extension* extension) {
  FSInfoMap results;
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  registry->GetMediaFileSystemsForExtension(
      single_web_contents_.get(), extension,
      base::Bind(&GetGalleryInfoCallback, base::Unretained(&results)));
  content::RunAllTasksUntilIdle();
  return results;
}

extensions::Extension* ProfileState::all_permission_extension() {
  return all_permission_extension_.get();
}

extensions::Extension* ProfileState::regular_permission_extension() {
  return regular_permission_extension_.get();
}

Profile* ProfileState::profile() {
  return profile_.get();
}

void ProfileState::AddNameForReadCompare(const base::string16& name) {
  compare_names_read_.push_back(name);
}

void ProfileState::AddNameForAllCompare(const base::string16& name) {
  compare_names_all_.push_back(name);
}

bool ProfileState::ContainsEntry(
    const MediaFileSystemInfo& info,
    const std::vector<MediaFileSystemInfo>& container) {
  for (size_t i = 0; i < container.size(); ++i) {
    if (info.path.value() == container[i].path.value()) {
      EXPECT_FALSE(container[i].fsid.empty());
      if (!info.fsid.empty())
        EXPECT_EQ(info.fsid, container[i].fsid);
      return true;
    }
  }
  return false;
}

void ProfileState::CompareResults(
    const std::string& test,
    const std::vector<base::string16>& names,
    const std::vector<MediaFileSystemInfo>& expected,
    const std::vector<MediaFileSystemInfo>& actual) {
  num_comparisons_++;
  EXPECT_EQ(expected.size(), actual.size()) << test;

  // Order isn't important, so sort the results.
  std::vector<MediaFileSystemInfo> sorted(actual);
  std::sort(sorted.begin(), sorted.end(), MediaFileSystemInfoComparator);
  std::vector<MediaFileSystemInfo> expect(expected);
  std::sort(expect.begin(), expect.end(), MediaFileSystemInfoComparator);
  std::vector<base::string16> expect_names(names);
  std::sort(expect_names.begin(), expect_names.end());

  for (size_t i = 0; i < expect.size() && i < sorted.size(); ++i) {
    if (expect_names.size() > i)
      EXPECT_EQ(expect_names[i], sorted[i].name) << test;
    EXPECT_TRUE(ContainsEntry(expect[i], sorted)) << test;
  }
}

int ProfileState::GetAndClearComparisonCount() {
  int result = num_comparisons_;
  num_comparisons_ = 0;
  return result;
}

}  // namespace

/////////////////////////////////
// MediaFileSystemRegistryTest //
/////////////////////////////////

void MediaFileSystemRegistryTest::CreateProfileState(size_t profile_count) {
  for (size_t i = 0; i < profile_count; ++i) {
    profile_states_.push_back(std::make_unique<ProfileState>(&rph_factory_));
  }
}

ProfileState* MediaFileSystemRegistryTest::GetProfileState(size_t i) {
  return profile_states_[i].get();
}

MediaGalleriesPreferences* MediaFileSystemRegistryTest::GetPreferences(
    Profile* profile) {
  MediaGalleriesPreferences* prefs = registry()->GetPreferences(profile);
  base::RunLoop loop;
  prefs->EnsureInitialized(loop.QuitClosure());
  loop.Run();
  return prefs;
}

std::string MediaFileSystemRegistryTest::AddUserGallery(
    StorageInfo::Type type,
    const std::string& unique_id,
    const base::FilePath& path) {
  std::string device_id = StorageInfo::MakeDeviceId(type, unique_id);
  DCHECK(!StorageInfo::IsMediaDevice(device_id));

  for (size_t i = 0; i < profile_states_.size(); ++i) {
    profile_states_[i]->GetMediaGalleriesPrefs()->AddGallery(
        device_id, base::FilePath(), MediaGalleryPrefInfo::kUserAdded,
        base::string16(), base::string16(), base::string16(), 0,
        base::Time::Now(), 0, 0, 0);
  }
  return device_id;
}

std::string MediaFileSystemRegistryTest::AttachDevice(
    StorageInfo::Type type,
    const std::string& unique_id,
    const base::FilePath& location) {
  std::string device_id = StorageInfo::MakeDeviceId(type, unique_id);
  DCHECK(StorageInfo::IsRemovableDevice(device_id));
  base::string16 label = location.BaseName().LossyDisplayName();
  ProcessAttach(device_id, label, location.value());
  content::RunAllTasksUntilIdle();
  return device_id;
}

void MediaFileSystemRegistryTest::DetachDevice(const std::string& device_id) {
  DCHECK(StorageInfo::IsRemovableDevice(device_id));
  ProcessDetach(device_id);
  content::RunAllTasksUntilIdle();
}

void MediaFileSystemRegistryTest::SetGalleryPermission(
    ProfileState* profile_state, extensions::Extension* extension,
    const std::string& device_id, bool has_access) {
  MediaGalleriesPreferences* preferences =
      profile_state->GetMediaGalleriesPrefs();
  MediaGalleryPrefIdSet pref_id =
      preferences->LookUpGalleriesByDeviceId(device_id);
  ASSERT_EQ(1U, pref_id.size());
  preferences->SetGalleryPermissionForExtension(*extension, *pref_id.begin(),
                                                has_access);
}

void MediaFileSystemRegistryTest::AssertAllAutoAddedGalleries() {
  for (size_t i = 0; i < profile_states_.size(); ++i) {
    MediaGalleriesPreferences* prefs =
        profile_states_[0]->GetMediaGalleriesPrefs();

    // Make sure that we have at least one gallery and that they are all
    // auto added galleries.
    const MediaGalleriesPrefInfoMap& galleries = prefs->known_galleries();
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
    ASSERT_GT(galleries.size(), 0U);
#endif
    for (auto it = galleries.begin(); it != galleries.end(); ++it) {
      ASSERT_EQ(MediaGalleryPrefInfo::kAutoDetected, it->second.type);
    }
  }
}

void MediaFileSystemRegistryTest::InitForGalleriesInfoTest(
    FSInfoMap* galleries_info) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  // Get all existing gallery names.
  ProfileState* profile_state = GetProfileState(0U);
  *galleries_info = profile_state->GetGalleriesInfo(
      profile_state->all_permission_extension());
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  ASSERT_EQ(3U, galleries_info->size());
#else
  ASSERT_EQ(0U, galleries_info->size());
#endif
}

void MediaFileSystemRegistryTest::CheckNewGalleryInfo(
    ProfileState* profile_state,
    const FSInfoMap& galleries_info,
    const base::FilePath& location,
    bool removable,
    bool media_device) {
  // Get new galleries.
  FSInfoMap new_galleries_info = profile_state->GetGalleriesInfo(
      profile_state->all_permission_extension());
  ASSERT_EQ(galleries_info.size() + 1U, new_galleries_info.size());

  bool found_new = false;
  for (FSInfoMap::const_iterator it = new_galleries_info.begin();
       it != new_galleries_info.end();
       ++it) {
    if (base::Contains(galleries_info, it->first))
      continue;

    ASSERT_FALSE(found_new);
    CheckGalleryInfo(it->second, test_file_system_context_, location,
                     removable, media_device);
    found_new = true;
  }
  ASSERT_TRUE(found_new);
}

std::vector<MediaFileSystemInfo>
MediaFileSystemRegistryTest::GetAutoAddedGalleries(
    ProfileState* profile_state) {
  const MediaGalleriesPrefInfoMap& galleries =
      profile_state->GetMediaGalleriesPrefs()->known_galleries();
  std::vector<MediaFileSystemInfo> result;
  for (auto it = galleries.begin(); it != galleries.end(); ++it) {
    if (it->second.type == MediaGalleryPrefInfo::kAutoDetected) {
      base::FilePath path = it->second.AbsolutePath();
      MediaFileSystemInfo info(path.BaseName().LossyDisplayName(), path,
                               std::string(), 0, std::string(), false, false);
      result.push_back(info);
    }
  }
  std::sort(result.begin(), result.end(), MediaFileSystemInfoComparator);
  return result;
}

size_t MediaFileSystemRegistryTest::GetExtensionGalleriesHostCount(
    const MediaFileSystemRegistry* registry) const {
  size_t extension_galleries_host_count = 0;
  for (auto it = registry->extension_hosts_map_.begin();
       it != registry->extension_hosts_map_.end(); ++it) {
    extension_galleries_host_count += it->second.size();
  }
  return extension_galleries_host_count;
}


void MediaFileSystemRegistryTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ASSERT_TRUE(TestStorageMonitor::CreateAndInstall());

  DeleteContents();
  SetRenderProcessHostFactory(&rph_factory_);

  test_file_system_context_ = new TestMediaFileSystemContext(
      g_browser_process->media_file_system_registry());

  ASSERT_TRUE(galleries_dir_.CreateUniqueTempDir());
  empty_dir_ = galleries_dir_.GetPath().AppendASCII("empty");
  ASSERT_TRUE(base::CreateDirectory(empty_dir_));
  dcim_dir_ = galleries_dir_.GetPath().AppendASCII("with_dcim");
  ASSERT_TRUE(base::CreateDirectory(dcim_dir_));
  ASSERT_TRUE(base::CreateDirectory(
      dcim_dir_.Append(storage_monitor::kDCIMDirectoryName)));
}

void MediaFileSystemRegistryTest::TearDown() {
  profile_states_.clear();
  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  EXPECT_EQ(0U, GetExtensionGalleriesHostCount(registry));

  // The TestingProfile must be destroyed before the TestingBrowserProcess
  // because it uses it in its destructor.
  ChromeRenderViewHostTestHarness::TearDown();

  // The MediaFileSystemRegistry owned by the TestingBrowserProcess must be
  // destroyed before the StorageMonitor because it calls
  // StorageMonitor::RemoveObserver() in its destructor.
  TestingBrowserProcess::DeleteInstance();

  TestStorageMonitor::Destroy();
}

///////////
// Tests //
///////////

TEST_F(MediaFileSystemRegistryTest, Basic) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  ProfileState* profile_state = GetProfileState(0);
  std::vector<MediaFileSystemInfo> auto_galleries =
      GetAutoAddedGalleries(profile_state);
  std::vector<MediaFileSystemInfo> empty_expectation;
  profile_state->CheckGalleries("basic", empty_expectation, auto_galleries);
}

TEST_F(MediaFileSystemRegistryTest, UserAddedGallery) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();
  ProfileState* profile_state = GetProfileState(0);
  std::vector<MediaFileSystemInfo> auto_galleries =
      GetAutoAddedGalleries(profile_state);
  std::vector<MediaFileSystemInfo> added_galleries;
  profile_state->CheckGalleries("user added init", added_galleries,
                                auto_galleries);

  // Add a user gallery to the regular permission extension.
  std::string device_id = AddUserGallery(StorageInfo::FIXED_MASS_STORAGE,
                                         empty_dir().AsUTF8Unsafe(),
                                         empty_dir());
  SetGalleryPermission(profile_state,
                       profile_state->regular_permission_extension(),
                       device_id,
                       true /*has access*/);
  MediaFileSystemInfo added_info(empty_dir().LossyDisplayName(), empty_dir(),
                                 std::string(), 0, std::string(), false, false);
  added_galleries.push_back(added_info);
  profile_state->CheckGalleries("user added regular", added_galleries,
                                auto_galleries);

  // Add it to the all galleries extension.
  SetGalleryPermission(profile_state,
                       profile_state->all_permission_extension(),
                       device_id,
                       true /*has access*/);
  auto_galleries.push_back(added_info);
  profile_state->CheckGalleries("user added all", added_galleries,
                                auto_galleries);
}

// Regression test to make sure erasing galleries does not result a crash.
TEST_F(MediaFileSystemRegistryTest, EraseGalleries) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  ProfileState* profile_state = GetProfileState(0);
  std::vector<MediaFileSystemInfo> auto_galleries =
      GetAutoAddedGalleries(profile_state);
  std::vector<MediaFileSystemInfo> empty_expectation;
  profile_state->CheckGalleries("erase", empty_expectation, auto_galleries);

  MediaGalleriesPreferences* prefs = profile_state->GetMediaGalleriesPrefs();
  MediaGalleriesPrefInfoMap galleries = prefs->known_galleries();
  for (MediaGalleriesPrefInfoMap::const_iterator it = galleries.begin();
       it != galleries.end(); ++it) {
    prefs->ForgetGalleryById(it->first);
  }
}

// Regression test to make sure calling GetPreferences() does not re-insert
// galleries on auto-detected removable devices that were blacklisted.
TEST_F(MediaFileSystemRegistryTest,
       GetPreferencesDoesNotReinsertBlacklistedGalleries) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  ProfileState* profile_state = GetProfileState(0);
  const size_t gallery_count = GetAutoAddedGalleries(profile_state).size();

  // Attach a device.
  const std::string device_id = AttachDevice(
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM,
      "removable_dcim_fake_id",
      dcim_dir());
  EXPECT_EQ(gallery_count + 1, GetAutoAddedGalleries(profile_state).size());

  // Forget the device.
  bool forget_gallery = false;
  MediaGalleriesPreferences* prefs = GetPreferences(profile_state->profile());
  const MediaGalleriesPrefInfoMap& galleries = prefs->known_galleries();
  for (auto it = galleries.begin(); it != galleries.end(); ++it) {
    if (it->second.device_id == device_id) {
      prefs->ForgetGalleryById(it->first);
      forget_gallery = true;
      break;
    }
  }
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(forget_gallery);
  EXPECT_EQ(gallery_count, GetAutoAddedGalleries(profile_state).size());

  // Call GetPreferences() and the gallery count should not change.
  prefs = GetPreferences(profile_state->profile());
  EXPECT_EQ(gallery_count, GetAutoAddedGalleries(profile_state).size());
}

TEST_F(MediaFileSystemRegistryTest, GalleryNameDefault) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

  for (FSInfoMap::const_iterator it = galleries_info.begin();
       it != galleries_info.end();
       ++it) {
    CheckGalleryInfo(it->second, test_file_system_context(),
                     it->second.path, false, false);
  }
}

// TODO(gbillock): Move the remaining test into the linux directory.
#if !defined(OS_MACOSX) && !defined(OS_WIN)
TEST_F(MediaFileSystemRegistryTest, GalleryMTP) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

  base::FilePath location(FILE_PATH_LITERAL("/mtp_bogus"));
  AttachDevice(StorageInfo::MTP_OR_PTP, "mtp_fake_id", location);
  CheckNewGalleryInfo(GetProfileState(0U), galleries_info, location,
                      true /*removable*/, true /* media device */);
}
#endif

TEST_F(MediaFileSystemRegistryTest, GalleryDCIM) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

  AttachDevice(StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM,
               "removable_dcim_fake_id",
               dcim_dir());
  CheckNewGalleryInfo(GetProfileState(0U), galleries_info, dcim_dir(),
                      true /*removable*/, true /* media device */);
}

TEST_F(MediaFileSystemRegistryTest, GalleryNoDCIM) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

  std::string device_id =
      AttachDevice(StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM,
                   empty_dir().AsUTF8Unsafe(),
                   empty_dir());
  std::string device_id2 =
      AddUserGallery(StorageInfo::REMOVABLE_MASS_STORAGE_NO_DCIM,
                     empty_dir().AsUTF8Unsafe(),
                     empty_dir());
  ASSERT_EQ(device_id, device_id2);
  // Add permission for new non-default gallery.
  ProfileState* profile_state = GetProfileState(0U);
  SetGalleryPermission(profile_state,
                       profile_state->all_permission_extension(),
                       device_id,
                       true /*has access*/);
  CheckNewGalleryInfo(profile_state, galleries_info, empty_dir(),
                      true /*removable*/, false /* media device */);
}

TEST_F(MediaFileSystemRegistryTest, GalleryUserAddedPath) {
  FSInfoMap galleries_info;
  InitForGalleriesInfoTest(&galleries_info);

  std::string device_id = AddUserGallery(StorageInfo::FIXED_MASS_STORAGE,
                                         empty_dir().AsUTF8Unsafe(),
                                         empty_dir());
  // Add permission for new non-default gallery.
  ProfileState* profile_state = GetProfileState(0U);
  SetGalleryPermission(profile_state,
                       profile_state->all_permission_extension(),
                       device_id,
                       true /*has access*/);
  CheckNewGalleryInfo(profile_state, galleries_info, empty_dir(),
                      false /*removable*/, false /* media device */);
}

TEST_F(MediaFileSystemRegistryTest, DetachedDeviceGalleryPath) {
  const std::string device_id = AttachDevice(
      StorageInfo::REMOVABLE_MASS_STORAGE_WITH_DCIM,
      "removable_dcim_fake_id",
      dcim_dir());

  MediaGalleryPrefInfo pref_info;
  pref_info.device_id = device_id;
  EXPECT_EQ(dcim_dir().value(), pref_info.AbsolutePath().value());

  MediaGalleryPrefInfo pref_info_with_relpath;
  pref_info_with_relpath.path =
      base::FilePath(FILE_PATH_LITERAL("test_relpath"));
  pref_info_with_relpath.device_id = device_id;
  EXPECT_EQ(dcim_dir().Append(pref_info_with_relpath.path).value(),
            pref_info_with_relpath.AbsolutePath().value());

  DetachDevice(device_id);
  EXPECT_TRUE(pref_info.AbsolutePath().empty());
  EXPECT_TRUE(pref_info_with_relpath.AbsolutePath().empty());
}

TEST_F(MediaFileSystemRegistryTest, TestNameConstruction) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  ProfileState* profile_state = GetProfileState(0);

  std::string user_gallery = AddUserGallery(StorageInfo::FIXED_MASS_STORAGE,
                                            empty_dir().AsUTF8Unsafe(),
                                            empty_dir());
  SetGalleryPermission(profile_state,
                       profile_state->regular_permission_extension(),
                       user_gallery,
                       true /*has access*/);
  SetGalleryPermission(profile_state,
                       profile_state->all_permission_extension(),
                       user_gallery,
                       true /*has access*/);

  std::vector<MediaFileSystemInfo> auto_galleries =
      GetAutoAddedGalleries(profile_state);
  MediaFileSystemInfo added_info(empty_dir().BaseName().LossyDisplayName(),
                                 empty_dir(), std::string(), 0, std::string(),
                                 false, false);
  auto_galleries.push_back(added_info);
  std::vector<MediaFileSystemInfo> one_expectation;
  one_expectation.push_back(added_info);

  base::string16 empty_dir_name = GetExpectedFolderName(empty_dir());
  profile_state->AddNameForReadCompare(empty_dir_name);
  profile_state->AddNameForAllCompare(empty_dir_name);

  // This part of the test is conditional on default directories existing
  // on the test platform. In ChromeOS, these directories do not exist.
  base::FilePath path;
  if (num_auto_galleries() > 0) {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_MUSIC, &path));
    profile_state->AddNameForAllCompare(GetExpectedFolderName(path));
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_PICTURES, &path));
    profile_state->AddNameForAllCompare(GetExpectedFolderName(path));
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_VIDEOS, &path));
    profile_state->AddNameForAllCompare(GetExpectedFolderName(path));

    profile_state->CheckGalleries("names-dir", one_expectation, auto_galleries);
  } else {
    profile_state->CheckGalleries("names", one_expectation, one_expectation);
  }
}

TEST_F(MediaFileSystemRegistryTest, PreferenceListener) {
  CreateProfileState(1);
  AssertAllAutoAddedGalleries();

  // Add a user gallery to the regular permission extension.
  std::string device_id = AddUserGallery(StorageInfo::FIXED_MASS_STORAGE,
                                         empty_dir().AsUTF8Unsafe(),
                                         empty_dir());
  ProfileState* profile_state = GetProfileState(0);
  SetGalleryPermission(profile_state,
                       profile_state->regular_permission_extension(),
                       device_id,
                       true /*has access*/);

  FSInfoMap fs_info = profile_state->GetGalleriesInfo(
      profile_state->regular_permission_extension());
  ASSERT_EQ(1U, fs_info.size());
  EXPECT_FALSE(test_file_system_context()->GetRegisteredPath(
      fs_info.begin()->second.fsid).empty());

  // Revoke permission and ensure that the file system is revoked.
  SetGalleryPermission(profile_state,
                       profile_state->regular_permission_extension(),
                       device_id,
                       false /*has access*/);
  EXPECT_TRUE(test_file_system_context()->GetRegisteredPath(
      fs_info.begin()->second.fsid).empty());
}
