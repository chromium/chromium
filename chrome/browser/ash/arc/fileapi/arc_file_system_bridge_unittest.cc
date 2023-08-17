// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"

#include <utility>
#include <vector>

#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/fileapi/chrome_content_provider_url_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/virtual_file_provider/fake_virtual_file_provider_client.h"
#include "chromeos/ash/components/dbus/virtual_file_provider/virtual_file_provider_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kTestingProfileName[] = "test-user";

// Values set by FakeProvidedFileSystem.
constexpr char kTestUrl[] = "externalfile:abc:test-filesystem:/hello.txt";
constexpr char kTestFileType[] = "text/plain";
constexpr int64_t kTestFileSize = 55;
constexpr char kTestFileLastModified[] = "Fri, 25 Apr 2014 01:47:53";
constexpr char kExtensionId[] = "abc";
constexpr char kFileSystemId[] = "test-filesystem";

}  // namespace

class ArcFileSystemBridgeTest : public testing::Test {
 public:
  ArcFileSystemBridgeTest() = default;
  ~ArcFileSystemBridgeTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::VirtualFileProviderClient::InitializeFake();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(kTestingProfileName);
    auto fake_provider =
        ash::file_system_provider::FakeExtensionProvider::Create(kExtensionId);
    const auto kProviderId = fake_provider->GetId();
    auto* service = ash::file_system_provider::Service::Get(profile_);
    service->RegisterProvider(std::move(fake_provider));
    service->MountFileSystem(kProviderId,
                             ash::file_system_provider::MountOptions(
                                 kFileSystemId, "Test FileSystem"));

    arc_file_system_bridge_ =
        std::make_unique<ArcFileSystemBridge>(profile_, &arc_bridge_service_);
    arc_bridge_service_.file_system()->SetInstance(&fake_file_system_);
    WaitForInstanceReady(arc_bridge_service_.file_system());
  }

  void TearDown() override {
    arc_bridge_service_.file_system()->CloseInstance(&fake_file_system_);
    arc_file_system_bridge_.reset();
    profile_manager_.reset();
    ash::VirtualFileProviderClient::Shutdown();
    ash::ConciergeClient::Shutdown();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile, DanglingUntriaged | ExperimentalAsh> profile_ = nullptr;

  FakeFileSystemInstance fake_file_system_;
  ArcBridgeService arc_bridge_service_;
  std::unique_ptr<ArcFileSystemBridge> arc_file_system_bridge_;
};

TEST_F(ArcFileSystemBridgeTest, GetFileName) {
  base::RunLoop run_loop;
  arc_file_system_bridge_->GetFileName(
      EncodeToChromeContentProviderUrl(GURL(kTestUrl)).spec(),
      base::BindLambdaForTesting(
          [&](const absl::optional<std::string>& result) {
            run_loop.Quit();
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ("hello.txt", result.value());
          }));
  run_loop.Run();
}

TEST_F(ArcFileSystemBridgeTest, GetFileNameNonASCII) {
  const std::string filename = base::UTF16ToUTF8(std::u16string({
      0x307b,  // HIRAGANA_LETTER_HO
      0x3052,  // HIRAGANA_LETTER_GE
  }));
  const GURL url("externalfile:abc:test-filesystem:/" + filename);

  base::RunLoop run_loop;
  arc_file_system_bridge_->GetFileName(
      EncodeToChromeContentProviderUrl(url).spec(),
      base::BindLambdaForTesting(
          [&](const absl::optional<std::string>& result) {
            run_loop.Quit();
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(filename, result.value());
          }));
  run_loop.Run();
}

// base::UnescapeURLComponent() leaves UTF-8 lock icons escaped, but they're
// valid file names, so shouldn't be left escaped here.
TEST_F(ArcFileSystemBridgeTest, GetFileNameLockIcon) {
  const GURL url("externalfile:abc:test-filesystem:/%F0%9F%94%92");

  base::RunLoop run_loop;
  arc_file_system_bridge_->GetFileName(
      EncodeToChromeContentProviderUrl(url).spec(),
      base::BindLambdaForTesting(
          [&](const absl::optional<std::string>& result) {
            run_loop.Quit();
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ("\xF0\x9F\x94\x92", result.value());
          }));
  run_loop.Run();
}

// An escaped path separator should cause GetFileName() to fail.
TEST_F(ArcFileSystemBridgeTest, GetFileNameEscapedPathSeparator) {
  const GURL url("externalfile:abc:test-filesystem:/foo%2F");

  base::RunLoop run_loop;
  arc_file_system_bridge_->GetFileName(
      EncodeToChromeContentProviderUrl(url).spec(),
      base::BindLambdaForTesting(
          [&](const absl::optional<std::string>& result) {
            run_loop.Quit();
            ASSERT_FALSE(result.has_value());
          }));
  run_loop.Run();
}

TEST_F(ArcFileSystemBridgeTest, GetFileSize) {
  base::RunLoop run_loop;
  arc_file_system_bridge_->GetFileSize(
      EncodeToChromeContentProviderUrl(GURL(kTestUrl)).spec(),
      base::BindLambdaForTesting([&](int64_t result) {
        EXPECT_EQ(kTestFileSize, result);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ArcFileSystemBridgeTest, GetLastModified) {
  base::Time expected;
  ASSERT_TRUE(base::Time::FromUTCString(kTestFileLastModified, &expected));

  base::RunLoop run_loop;
  arc_file_system_bridge_->GetLastModified(
      EncodeToChromeContentProviderUrl(GURL(kTestUrl)),
      base::BindLambdaForTesting([&](const absl::optional<base::Time> result) {
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(expected, result.value());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ArcFileSystemBridgeTest, GetFileType) {
  base::RunLoop run_loop;
  arc_file_system_bridge_->GetFileType(
      EncodeToChromeContentProviderUrl(GURL(kTestUrl)).spec(),
      base::BindLambdaForTesting(
          [&](const absl::optional<std::string>& result) {
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(kTestFileType, result.value());
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ArcFileSystemBridgeTest, GetVirtualFileId) {
  // Set up fake virtual file provider client.
  constexpr char kId[] = "testfile";
  auto* fake_client = static_cast<ash::FakeVirtualFileProviderClient*>(
      ash::VirtualFileProviderClient::Get());
  fake_client->set_expected_size(kTestFileSize);
  fake_client->set_result_id(kId);

  // GetVirtualFileId().
  base::RunLoop run_loop;
  arc_file_system_bridge_->GetVirtualFileId(
      EncodeToChromeContentProviderUrl(GURL(kTestUrl)).spec(),
      base::BindLambdaForTesting([&](const absl::optional<std::string>& id) {
        ASSERT_NE(absl::nullopt, id);
        EXPECT_EQ(kId, id.value());
        run_loop.Quit();
      }));
  run_loop.Run();

  content::RunAllTasksUntilIdle();

  // ID is released.
  EXPECT_TRUE(arc_file_system_bridge_->HandleIdReleased(kId));
}

TEST_F(ArcFileSystemBridgeTest, OpenFileToRead) {
  // Set up fake virtual file provider client.
  base::FilePath temp_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_path));
  base::File temp_file(temp_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(temp_file.IsValid());

  constexpr char kId[] = "testfile";
  auto* fake_client = static_cast<ash::FakeVirtualFileProviderClient*>(
      ash::VirtualFileProviderClient::Get());
  fake_client->set_expected_size(kTestFileSize);
  fake_client->set_result_id(kId);
  fake_client->set_result_fd(base::ScopedFD(temp_file.TakePlatformFile()));

  // OpenFileToRead().
  base::RunLoop run_loop;
  arc_file_system_bridge_->OpenFileToRead(
      EncodeToChromeContentProviderUrl(GURL(kTestUrl)).spec(),
      base::BindLambdaForTesting([&](mojo::ScopedHandle result) {
        EXPECT_TRUE(result.is_valid());
        run_loop.Quit();
      }));
  run_loop.Run();

  // HandleReadRequest().
  int pipe_fds[2] = {-1, -1};
  ASSERT_EQ(0, pipe(pipe_fds));
  base::ScopedFD pipe_read_end(pipe_fds[0]), pipe_write_end(pipe_fds[1]);
  arc_file_system_bridge_->HandleReadRequest(kId, 0, kTestFileSize,
                                             std::move(pipe_write_end));
  content::RunAllTasksUntilIdle();

  // Requested number of bytes are written to the pipe.
  std::vector<char> buf(kTestFileSize);
  ASSERT_TRUE(base::ReadFromFD(pipe_read_end.get(), buf.data(), buf.size()));

  // ID is released.
  EXPECT_TRUE(arc_file_system_bridge_->HandleIdReleased(kId));
}

TEST_F(ArcFileSystemBridgeTest, GetLinuxVFSPathFromExternalFileURL) {
  storage::ExternalMountPoints* system_mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  // Check: FSPs aren't visible on the VFS so should yield no path.
  base::FilePath fsp_path =
      arc_file_system_bridge_->GetLinuxVFSPathFromExternalFileURL(
          profile_, GURL(kTestUrl));
  EXPECT_EQ(fsp_path, base::FilePath());

  // SmbFs is visible on the VFS, so should yield a path.
  constexpr char kSmbFsTestMountName[] = "test-smb";
  constexpr char kSmbFsTestMountPoint[] = "/dummy/mount";
  constexpr char kTestPathInsideMount[] = "path/to/file";
  EXPECT_TRUE(system_mount_points->RegisterFileSystem(
      kSmbFsTestMountName, storage::FileSystemType::kFileSystemTypeSmbFs, {},
      base::FilePath(kSmbFsTestMountPoint)));

  base::FilePath smbfs_path_expected(
      base::FilePath(kSmbFsTestMountPoint).Append(kTestPathInsideMount));

  // Create externalfile: URL as would be encoded inside the
  // ChromeContentProvider URL.
  GURL smbfs_url =
      ash::CreateExternalFileURLFromPath(profile_, smbfs_path_expected, true);

  // Check: The path returned matches the path encoded into the URL.
  base::FilePath smbfs_path =
      arc_file_system_bridge_->GetLinuxVFSPathFromExternalFileURL(profile_,
                                                                  smbfs_url);
  EXPECT_EQ(smbfs_path, smbfs_path_expected);
  system_mount_points->RevokeFileSystem(kSmbFsTestMountName);
}

TEST_F(ArcFileSystemBridgeTest, GetLinuxVFSPathForPathOnFileSystemType) {
  // Check: DriveFS paths are returned as passed in.
  const base::FilePath filesystem_path("/path/on/filesystem/file");
  base::FilePath drivefs_vfs_path =
      arc_file_system_bridge_->GetLinuxVFSPathForPathOnFileSystemType(
          profile_, filesystem_path, storage::kFileSystemTypeDriveFs);
  EXPECT_EQ(drivefs_vfs_path, filesystem_path);

  // Check: SmbFs paths are returned as passed in.
  base::FilePath smbfs_vfs_path =
      arc_file_system_bridge_->GetLinuxVFSPathForPathOnFileSystemType(
          profile_, filesystem_path, storage::kFileSystemTypeSmbFs);
  EXPECT_EQ(smbfs_vfs_path, filesystem_path);

  // Check: Crostini paths are returned as passed in.
  const base::FilePath crostini_path =
      file_manager::util::GetCrostiniMountDirectory(profile_).Append(
          "path/to/file");
  base::FilePath crostini_vfs_path =
      arc_file_system_bridge_->GetLinuxVFSPathForPathOnFileSystemType(
          profile_, crostini_path, storage::kFileSystemTypeLocal);
  EXPECT_EQ(crostini_vfs_path, crostini_path);

  // Check: fuse-zip and rar2fs paths are returned as passed in.
  const base::FilePath archive_path =
      base::FilePath(file_manager::util::kArchiveMountPath)
          .Append("path/to/file");
  base::FilePath archive_vfs_path =
      arc_file_system_bridge_->GetLinuxVFSPathForPathOnFileSystemType(
          profile_, archive_path, storage::kFileSystemTypeLocal);
  EXPECT_EQ(archive_vfs_path, archive_path);

  // Check: Other kFileSystemTypeLocal paths that are not descendants of
  // the Crostini, fuse-zip or rar2fs mount points return an empty path.
  const base::FilePath empty_path,
      unsupported_local_path = base::FilePath("/path/to/file");
  base::FilePath unsupported_local_vfs_path =
      arc_file_system_bridge_->GetLinuxVFSPathForPathOnFileSystemType(
          profile_, unsupported_local_path, storage::kFileSystemTypeLocal);
  EXPECT_EQ(empty_path, unsupported_local_vfs_path);

  // Check: Paths from unsupported FileSystemTypes return an empty path.
  const base::FilePath unsupported_filesystem_path =
      base::FilePath("/special/path");
  base::FilePath unsupported_filesystem_vfs_path =
      arc_file_system_bridge_->GetLinuxVFSPathForPathOnFileSystemType(
          profile_, unsupported_filesystem_path,
          storage::kFileSystemTypeProvided);
  EXPECT_EQ(empty_path, unsupported_filesystem_vfs_path);
}

TEST_F(ArcFileSystemBridgeTest, PropagatesOnMediaStoreUriAddedEvents) {
  class MockObserver : public ArcFileSystemBridge::Observer {
   public:
    MOCK_METHOD(void,
                OnMediaStoreUriAdded,
                (const GURL& uri, const mojom::MediaStoreMetadata& metadata),
                (override));
  };

  // Register a mock observer.
  testing::NiceMock<MockObserver> observer;
  base::ScopedObservation<ArcFileSystemBridge, ArcFileSystemBridge::Observer>
      observation{&observer};
  observation.Observe(arc_file_system_bridge_.get());

  // Prepare data for an `OnMediaStoreUriAdded()` event.
  const GURL uri("uri");
  auto metadata = mojom::MediaStoreMetadata::NewDownload(
      mojom::MediaStoreDownloadMetadata::New(
          /*display_name=*/"foo.pdf",
          /*owner_package_name=*/"com.android.documentsui",
          /*relative_path=*/base::FilePath("Download/")));

  // Expect observer to be notified of an `OnMediaStoreUriAdded()` event.
  EXPECT_CALL(observer,
              OnMediaStoreUriAdded(testing::Eq(uri),
                                   testing::Eq(testing::ByRef(*metadata))));

  // Simulate an `OnMediaStoreUriAdded()` event from ARC.
  arc_file_system_bridge_->OnMediaStoreUriAdded(uri, mojo::Clone(metadata));
}

}  // namespace arc
