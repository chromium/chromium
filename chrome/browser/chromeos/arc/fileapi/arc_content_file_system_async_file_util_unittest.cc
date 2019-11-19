// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_async_file_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using File = arc::FakeFileSystemInstance::File;

namespace arc {

namespace {

constexpr char kArcUrl[] = "content://org.chromium.foo/bar";
constexpr char kData[] = "abcdef";
constexpr char kMimeType[] = "application/octet-stream";

std::unique_ptr<KeyedService> CreateArcFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return ArcFileSystemOperationRunner::CreateForTesting(
      context, ArcServiceManager::Get()->arc_bridge_service());
}

class ArcContentFileSystemAsyncFileUtilTest : public testing::Test {
 public:
  ArcContentFileSystemAsyncFileUtilTest() = default;
  ~ArcContentFileSystemAsyncFileUtilTest() override = default;

  void SetUp() override {
    fake_file_system_.AddFile(
        File(kArcUrl, kData, kMimeType, File::Seekable::NO));

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    arc_service_manager_->set_browser_context(profile_.get());
    ArcFileSystemOperationRunner::GetFactory()->SetTestingFactoryAndUse(
        profile_.get(),
        base::BindRepeating(&CreateArcFileSystemOperationRunnerForTesting));
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
    WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());

    async_file_util_ = std::make_unique<ArcContentFileSystemAsyncFileUtil>();
  }

  void TearDown() override {
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);
  }

 protected:
  storage::FileSystemURL ExternalFileURLToFileSystemURL(const GURL& url) {
    base::FilePath mount_point_virtual_path =
        base::FilePath::FromUTF8Unsafe(kContentFileSystemMountPointName);
    base::FilePath virtual_path = chromeos::ExternalFileURLToVirtualPath(url);
    base::FilePath path(kContentFileSystemMountPointPath);
    EXPECT_TRUE(
        mount_point_virtual_path.AppendRelativePath(virtual_path, &path));
    return storage::FileSystemURL::CreateForTest(
        url::Origin(),  // origin
        storage::kFileSystemTypeArcContent, path);
  }

  content::BrowserTaskEnvironment task_environment_;
  FakeFileSystemInstance fake_file_system_;

  // Use the same initialization/destruction order as
  // ChromeBrowserMainPartsChromeos.
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcContentFileSystemAsyncFileUtil> async_file_util_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcContentFileSystemAsyncFileUtilTest);
};

}  // namespace

TEST_F(ArcContentFileSystemAsyncFileUtilTest, GetFileInfo) {
  GURL externalfile_url = ArcUrlToExternalFileUrl(GURL(kArcUrl));

  base::RunLoop run_loop;
  async_file_util_->GetFileInfo(
      std::unique_ptr<storage::FileSystemOperationContext>(),
      ExternalFileURLToFileSystemURL(externalfile_url),
      -1,  // fields
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error,
             const base::File::Info& info) {
            EXPECT_EQ(base::File::FILE_OK, error);
            EXPECT_EQ(static_cast<int64_t>(strlen(kData)), info.size);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace arc
