// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_prefs_unittest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AddGalleryPermission(MediaGalleryPrefId gallery,
                          bool has_access,
                          std::vector<MediaGalleryPermission>* vector) {
  MediaGalleryPermission permission;
  permission.pref_id = gallery;
  permission.has_permission = has_access;
  vector->push_back(permission);
}

}  // namespace

// Test the MediaGalleries permissions functions.
class MediaGalleriesPermissionsTest : public extensions::ExtensionPrefsTest {
 protected:
  MediaGalleriesPermissionsTest() {}
  ~MediaGalleriesPermissionsTest() override {}

  // This is the same implementation as ExtensionPrefsTest::TearDown(), except
  // for also resetting the ExtensionPrefs used by |gallery_prefs_| after
  // TestExtensionPrefs reconstructs them.
  void TearDown() override {
    Verify();

    // Reset ExtensionPrefs, and re-verify.
    prefs_.ResetPrefRegistry();
    RegisterPreferences(prefs_.pref_registry().get());
    prefs_.RecreateExtensionPrefs();
    gallery_prefs_->SetExtensionPrefsForTesting(prefs());
    Verify();
    prefs_.pref_service()->CommitPendingWrite();
    base::RunLoop().RunUntilIdle();

    storage_monitor::TestStorageMonitor::Destroy();

    testing::Test::TearDown();
  }

  void Initialize() override {
    ASSERT_TRUE(storage_monitor::TestStorageMonitor::CreateAndInstall());
    profile_.reset(new TestingProfile);
    gallery_prefs_.reset(new MediaGalleriesPreferences(profile_.get()));
    base::RunLoop loop;
    gallery_prefs_->EnsureInitialized(loop.QuitClosure());
    loop.Run();

    gallery_prefs_->SetExtensionPrefsForTesting(prefs());

    extension1_id_ = prefs_.AddExtensionAndReturnId("test1");
    extension2_id_ = prefs_.AddExtensionAndReturnId("test2");
    extension3_id_ = prefs_.AddExtensionAndReturnId("test3");
    // Id4 isn't used to ensure that an empty permission list is ok.
    extension4_id_ = prefs_.AddExtensionAndReturnId("test4");
    Verify();

    gallery_prefs_->SetGalleryPermissionInPrefs(extension1_id_, 1, false);
    AddGalleryPermission(1, false, &extension1_expectation_);
    Verify();

    gallery_prefs_->SetGalleryPermissionInPrefs(extension1_id_, 2, true);
    AddGalleryPermission(2, true, &extension1_expectation_);
    Verify();

    gallery_prefs_->SetGalleryPermissionInPrefs(extension1_id_, 2, false);
    extension1_expectation_[1].has_permission = false;
    Verify();

    gallery_prefs_->SetGalleryPermissionInPrefs(extension2_id_, 1, true);
    gallery_prefs_->SetGalleryPermissionInPrefs(extension2_id_, 3, true);
    gallery_prefs_->SetGalleryPermissionInPrefs(extension2_id_, 4, true);
    AddGalleryPermission(1, true, &extension2_expectation_);
    AddGalleryPermission(3, true, &extension2_expectation_);
    AddGalleryPermission(4, true, &extension2_expectation_);
    Verify();

    gallery_prefs_->SetGalleryPermissionInPrefs(extension3_id_, 3, true);
    AddGalleryPermission(3, true, &extension3_expectation_);
    Verify();

    gallery_prefs_->RemoveGalleryPermissionsFromPrefs(3);
    extension2_expectation_.erase(extension2_expectation_.begin() + 1);
    extension3_expectation_.erase(extension3_expectation_.begin());
    Verify();

    gallery_prefs_->UnsetGalleryPermissionInPrefs(extension1_id_, 1);
    extension1_expectation_.erase(extension1_expectation_.begin());
    Verify();
  }

  void Verify() override {
    struct TestData {
      std::string* id;
      std::vector<MediaGalleryPermission>* expectation;
    };

    const TestData test_data[] = {{&extension1_id_, &extension1_expectation_},
                                  {&extension2_id_, &extension2_expectation_},
                                  {&extension3_id_, &extension3_expectation_},
                                  {&extension4_id_, &extension4_expectation_}};
    for (size_t i = 0; i < base::size(test_data); i++) {
      std::vector<MediaGalleryPermission> actual =
          gallery_prefs_->GetGalleryPermissionsFromPrefs(*test_data[i].id);
      EXPECT_EQ(test_data[i].expectation->size(), actual.size());
      for (size_t permission_entry = 0;
           permission_entry < test_data[i].expectation->size() &&
               permission_entry < actual.size();
           permission_entry++) {
        EXPECT_EQ(test_data[i].expectation->at(permission_entry).pref_id,
                  actual[permission_entry].pref_id);
        EXPECT_EQ(test_data[i].expectation->at(permission_entry).has_permission,
                  actual[permission_entry].has_permission);
      }
    }
  }

  std::string extension1_id_;
  std::string extension2_id_;
  std::string extension3_id_;
  std::string extension4_id_;

  std::vector<MediaGalleryPermission> extension1_expectation_;
  std::vector<MediaGalleryPermission> extension2_expectation_;
  std::vector<MediaGalleryPermission> extension3_expectation_;
  std::vector<MediaGalleryPermission> extension4_expectation_;

  // Needed for |gallery_prefs_| to initialize correctly.
  EnsureMediaDirectoriesExists ensure_media_directories_exists_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MediaGalleriesPreferences> gallery_prefs_;
};

TEST_F(MediaGalleriesPermissionsTest, MediaGalleries) {}
