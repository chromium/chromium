// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_async_file_util.h"

#include <string.h>

#include <memory>
#include <string>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
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

  ArcContentFileSystemAsyncFileUtilTest(
      const ArcContentFileSystemAsyncFileUtilTest&) = delete;
  ArcContentFileSystemAsyncFileUtilTest& operator=(
      const ArcContentFileSystemAsyncFileUtilTest&) = delete;

  ~ArcContentFileSystemAsyncFileUtilTest() override = default;

  void SetUp() override {
    fake_file_system_.AddFile(
        File(kArcUrl, kData, kMimeType, File::Seekable::YES));

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
    // Before destroying objects, flush tasks posted to run
    // ArcFileSystemOperationRunner::CloseFileSession().
    task_environment_.RunUntilIdle();

    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);
    arc_service_manager_->set_browser_context(nullptr);
  }

 protected:
  storage::FileSystemURL ExternalFileURLToFileSystemURL(const GURL& url) {
    base::FilePath mount_point_virtual_path =
        base::FilePath::FromASCII(kContentFileSystemMountPointName);
    base::FilePath virtual_path = ash::ExternalFileURLToVirtualPath(url);
    base::FilePath path(kContentFileSystemMountPointPath);
    EXPECT_TRUE(
        mount_point_virtual_path.AppendRelativePath(virtual_path, &path));
    return storage::FileSystemURL::CreateForTest(
        blink::StorageKey(), storage::kFileSystemTypeArcContent, path);
  }

  content::BrowserTaskEnvironment task_environment_;
  FakeFileSystemInstance fake_file_system_;

  // Use the same initialization/destruction order as
  // `ChromeBrowserMainPartsAsh`.
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcContentFileSystemAsyncFileUtil> async_file_util_;
};

}  // namespace

TEST_F(ArcContentFileSystemAsyncFileUtilTest, GetFileInfo) {
  GURL externalfile_url = ArcUrlToExternalFileUrl(GURL(kArcUrl));

  base::RunLoop run_loop;
  async_file_util_->GetFileInfo(
      std::unique_ptr<storage::FileSystemOperationContext>(),
      ExternalFileURLToFileSystemURL(externalfile_url),
      storage::FileSystemOperation::GetMetadataFieldSet::All(),
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

TEST_F(ArcContentFileSystemAsyncFileUtilTest, Truncate) {
  // Currently, truncate() is disabled if ARCVM is not enabled.
  // For this test, just pretend ARCVM is enabled.
  // TODO(b/223247850) Fix this.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableArcVm);

  GURL externalfile_url = ArcUrlToExternalFileUrl(GURL(kArcUrl));
  const uint64_t kLength = strlen(kData) / 2;

  base::RunLoop run_loop;
  async_file_util_->Truncate(
      std::unique_ptr<storage::FileSystemOperationContext>(),
      ExternalFileURLToFileSystemURL(externalfile_url), kLength,
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            EXPECT_EQ(base::File::FILE_OK, error);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();

  EXPECT_EQ(fake_file_system_.GetFileContent(kArcUrl).size(), kLength);
}

}  // namespace arc
