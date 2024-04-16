// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/file_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace apps {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class FileUtilsTest : public ::testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing_profile");
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            mount_name_, storage::FileSystemType::kFileSystemTypeExternal,
            storage::FileSystemMountOption(), base::FilePath(fs_root_)));
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
            mount_name_));
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  TestingProfile* GetProfile() { return profile_; }

  base::FilePath GetTempDir() { return scoped_temp_dir_.GetPath(); }

  // FileUtils explicitly relies on ChromeOS Files.app for files manipulation.
  const url::Origin GetFileManagerOrigin() {
    return url::Origin::Create(file_manager::util::GetFileManagerURL());
  }

  // Converts the given virtual |path| to a file system URL. Uses test file
  // system type.
  storage::FileSystemURL ToTestFileSystemURL(const std::string& path) {
    return storage::FileSystemURL::CreateForTest(
        blink::StorageKey::CreateFirstParty(GetFileManagerOrigin()),
        storage::FileSystemType::kFileSystemTypeTest, base::FilePath(path));
  }

  // For a given |root| converts the given virtual |path| to a GURL.
  GURL ToGURL(const base::FilePath& root, const std::string& path) {
    const std::string abs_path = root.Append(path).value();
    return GURL(base::StrCat({url::kFileSystemScheme, ":",
                              GetFileManagerOrigin().Serialize(), abs_path}));
  }

 protected:
  const std::string mount_name_ = "TestMountName";
  const std::string fs_root_ = "/path/to/test/filesystemroot";

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::ScopedTempDir scoped_temp_dir_;
  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(FileUtilsTest, GetFileSystemURL) {
  std::vector<GURL> url_list;
  std::vector<storage::FileSystemURL> fsurl_list;

  // Case 1: url_list is empty.
  fsurl_list = GetFileSystemURL(GetProfile(), url_list);
  EXPECT_THAT(fsurl_list, IsEmpty());

  // Case 2: url_list contains a GURL.
  const std::string path = "Documents/foo.txt";
  url_list.push_back(ToGURL(base::FilePath(storage::kTestDir), path));
  fsurl_list = GetFileSystemURL(GetProfile(), url_list);
  EXPECT_THAT(fsurl_list, ElementsAre(ToTestFileSystemURL(path)));
}

TEST_F(FileUtilsTest, GetFileSystemUrls) {
  std::vector<GURL> url_list;
  std::vector<base::FilePath> fp_list;

  // Case 1: fp_list is empty.
  url_list = GetFileSystemUrls(GetProfile(), fp_list);
  EXPECT_THAT(url_list, IsEmpty());

  // Case 2: fp_list contain some paths.
  const std::string path = "Images/foo.jpg";
  fp_list.push_back(base::FilePath(fs_root_).Append(path));
  url_list = GetFileSystemUrls(GetProfile(), fp_list);
  // Given a list of absolute file paths, return a list of filesystem:// URLs
  // that use the kFileSystemTypeExternal type with Files Manager's origin.
  // TODO(crbug.com/40763788): The use of Files Manager origin in these URLs is
  // probably incorrect and should be revisited.
  EXPECT_THAT(
      url_list,
      ElementsAre(ToGURL(
          base::FilePath(storage::kExternalDir).Append(mount_name_), path)));

  // Case 3: paths not originating in a known root are ignored.
  fp_list.push_back(base::FilePath("/not/a/known/root").Append(path));
  url_list = GetFileSystemUrls(GetProfile(), fp_list);
  // Still just one path corresponding to foo.jpg under a known root.
  EXPECT_THAT(
      url_list,
      ElementsAre(ToGURL(
          base::FilePath(storage::kExternalDir).Append(mount_name_), path)));
}

TEST_F(FileUtilsTest, GetSingleFileSystemURL) {
  GURL url;
  storage::FileSystemURL fsurl;

  const std::string path = "Documents/foo.txt";
  url = ToGURL(base::FilePath(storage::kTestDir), path);
  fsurl = GetFileSystemURL(GetProfile(), url);
  EXPECT_EQ(fsurl, ToTestFileSystemURL(path));
}

TEST_F(FileUtilsTest, GetSingleFileSystemUrl) {
  GURL url;
  base::FilePath file_path;

  const std::string path = "Images/foo.jpg";
  file_path = base::FilePath(fs_root_).Append(path);
  url = GetFileSystemUrl(GetProfile(), file_path);
  EXPECT_EQ(
      url,
      ToGURL(base::FilePath(storage::kExternalDir).Append(mount_name_), path));
}

}  // namespace

}  // namespace apps
