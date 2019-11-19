// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ChangeType = arc::ArcDocumentsProviderRoot::ChangeType;
using Document = arc::FakeFileSystemInstance::Document;
using EntryList = storage::AsyncFileUtil::EntryList;

namespace arc {

namespace {

// Simliar as FakeFileSystemInstance::Document, but all fields are primitives
// so that values can be constexpr.
struct DocumentSpec {
  const char* document_id;
  const char* parent_document_id;
  const char* display_name;
  const char* mime_type;
  int64_t size;
  uint64_t last_modified;
  bool supports_delete;
  bool supports_rename;
  bool dir_supports_create;
};

// Fake file system hierarchy:
//
// <path>            <type>      <ID>
// (root)/           dir         root-id
//   dir/            dir         dir-id
//     photo.jpg     image/jpeg  photo-id
//     music.bin     audio/mp3   music-id
//     no-delete.jpg image/jpeg  no-delete-id  // Non-deletable file
//     no-rename.jpg image/jpeg  no-rename-id  // Non-renamable file
//   dups/           dir         dups-id
//     dup.mp4       video/mp4   dup1-id
//     dup.mp4       video/mp4   dup2-id
//     dup.mp4       video/mp4   dup3-id
//     dup.mp4       video/mp4   dup4-id
//   ro-dir/         dir         ro-dir-id     // Read-only directory
constexpr char kAuthority[] = "org.chromium.test";
constexpr DocumentSpec kRootSpec{
    "root-id", "", "", kAndroidDirectoryMimeType, -1, 0, false, false, true};
constexpr DocumentSpec kDirSpec{"dir-id", kRootSpec.document_id,
                                "dir",    kAndroidDirectoryMimeType,
                                -1,       22,
                                true,     true,
                                true};
constexpr DocumentSpec kPhotoSpec{"photo-id",  kDirSpec.document_id,
                                  "photo.jpg", "image/jpeg",
                                  3,           33,
                                  true,        true,
                                  true};
constexpr DocumentSpec kMusicSpec{"music-id",  kDirSpec.document_id,
                                  "music.bin", "audio/mp3",
                                  4,           44,
                                  true,        true,
                                  true};
constexpr DocumentSpec kNoDeleteSpec{"no-delete-id",
                                     kDirSpec.document_id,
                                     "no-delete.jpg",
                                     "image/jpeg",
                                     3,
                                     45,
                                     false,
                                     true,
                                     true};
constexpr DocumentSpec kNoRenameSpec{"no-rename-id",
                                     kDirSpec.document_id,
                                     "no-rename.jpg",
                                     "image/jpeg",
                                     3,
                                     46,
                                     true,
                                     false,
                                     true};
constexpr DocumentSpec kDupsSpec{"dups-id", kRootSpec.document_id,
                                 "dups",    kAndroidDirectoryMimeType,
                                 -1,        55,
                                 true,      true,
                                 true};
constexpr DocumentSpec kDup1Spec{
    "dup1-id", kDupsSpec.document_id, "dup.mp4", "video/mp4", 6, 66, true, true,
    true};
constexpr DocumentSpec kDup2Spec{
    "dup2-id", kDupsSpec.document_id, "dup.mp4", "video/mp4", 7, 77, true, true,
    true};
constexpr DocumentSpec kDup3Spec{
    "dup3-id", kDupsSpec.document_id, "dup.mp4", "video/mp4", 8, 88, true, true,
    true};
constexpr DocumentSpec kDup4Spec{
    "dup4-id", kDupsSpec.document_id, "dup.mp4", "video/mp4", 9, 99, true, true,
    true};
constexpr DocumentSpec kRoDirSpec{"ro-dir-id", kRootSpec.document_id,
                                  "ro-dir",    kAndroidDirectoryMimeType,
                                  -1,          56,
                                  false,       false,
                                  false};

// The order is intentionally shuffled here so that
// FileSystemInstance::GetChildDocuments() returns documents in shuffled order.
// See ResolveToContentUrlDups test below.
constexpr DocumentSpec kAllSpecs[] = {
    kRootSpec, kDirSpec,  kPhotoSpec, kMusicSpec, kNoDeleteSpec, kNoRenameSpec,
    kDupsSpec, kDup2Spec, kDup1Spec,  kDup4Spec,  kDup3Spec,     kRoDirSpec};

Document ToDocument(const DocumentSpec& spec) {
  return Document(kAuthority, spec.document_id, spec.parent_document_id,
                  spec.display_name, spec.mime_type, spec.size,
                  spec.last_modified, spec.supports_delete,
                  spec.supports_rename, spec.dir_supports_create);
}

void ExpectMatchesSpec(const base::File::Info& info, const DocumentSpec& spec) {
  EXPECT_EQ(spec.size, info.size);
  if (spec.mime_type == kAndroidDirectoryMimeType) {
    EXPECT_TRUE(info.is_directory);
  } else {
    EXPECT_FALSE(info.is_directory);
  }
  EXPECT_FALSE(info.is_symbolic_link);
  EXPECT_EQ(spec.last_modified,
            static_cast<uint64_t>(info.last_modified.ToJavaTime()));
  EXPECT_EQ(spec.last_modified,
            static_cast<uint64_t>(info.last_accessed.ToJavaTime()));
  EXPECT_EQ(spec.last_modified,
            static_cast<uint64_t>(info.creation_time.ToJavaTime()));
}

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return ArcFileSystemOperationRunner::CreateForTesting(
      context, ArcServiceManager::Get()->arc_bridge_service());
}

class ArcDocumentsProviderRootTest : public testing::Test {
 public:
  ArcDocumentsProviderRootTest() = default;
  ~ArcDocumentsProviderRootTest() override = default;

  void SetUp() override {
    for (auto spec : kAllSpecs) {
      fake_file_system_.AddDocument(ToDocument(spec));
    }

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    arc_service_manager_->set_browser_context(profile_.get());
    ArcFileSystemOperationRunner::GetFactory()->SetTestingFactoryAndUse(
        profile_.get(),
        base::BindRepeating(&CreateFileSystemOperationRunnerForTesting));
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
    WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->file_system());

    // Run the message loop until FileSystemInstance::Init() is called.
    ASSERT_TRUE(fake_file_system_.InitCalled());

    root_ = std::make_unique<ArcDocumentsProviderRoot>(
        ArcFileSystemOperationRunner::GetForBrowserContext(profile_.get()),
        kAuthority, kRootSpec.document_id, "", false,
        std::vector<std::string>());
  }

  void TearDown() override {
    root_.reset();
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);

    // Run all pending tasks before destroying testing profile.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  FakeFileSystemInstance fake_file_system_;

  // Use the same initialization/destruction order as
  // ChromeBrowserMainPartsChromeos.
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcDocumentsProviderRoot> root_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDocumentsProviderRootTest);
};

}  // namespace

TEST_F(ArcDocumentsProviderRootTest, GetFileInfo) {
  base::RunLoop run_loop;
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
                     base::BindOnce(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           run_loop->Quit();
                           EXPECT_EQ(base::File::FILE_OK, error);
                           ExpectMatchesSpec(info, kPhotoSpec);
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoDirectory) {
  base::RunLoop run_loop;
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("dir")),
                     base::BindOnce(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           run_loop->Quit();
                           EXPECT_EQ(base::File::FILE_OK, error);
                           ExpectMatchesSpec(info, kDirSpec);
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoRoot) {
  base::RunLoop run_loop;
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("")),
                     base::BindOnce(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           run_loop->Quit();
                           EXPECT_EQ(base::File::FILE_OK, error);
                           ExpectMatchesSpec(info, kRootSpec);
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoNoSuchFile) {
  base::RunLoop run_loop;
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("dir/missing.jpg")),
                     base::BindOnce(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           run_loop->Quit();
                           EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoDups) {
  base::RunLoop run_loop;
  // "dup (2).mp4" should map to the 3rd instance of "dup.mp4" regardless of the
  // order returned from FileSystemInstance.
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("dups/dup (2).mp4")),
                     base::BindOnce(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           run_loop->Quit();
                           EXPECT_EQ(base::File::FILE_OK, error);
                           ExpectMatchesSpec(info, kDup3Spec);
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoWithCache) {
  {
    base::RunLoop run_loop;
    root_->GetFileInfo(
        base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
        base::BindOnce([](base::RunLoop* run_loop, base::File::Error error,
                          const base::File::Info& info) { run_loop->Quit(); },
                       &run_loop));
    run_loop.Run();
  }

  int last_count = fake_file_system_.get_child_documents_count();

  {
    base::RunLoop run_loop;
    root_->GetFileInfo(
        base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
        base::BindOnce([](base::RunLoop* run_loop, base::File::Error error,
                          const base::File::Info& info) { run_loop->Quit(); },
                       &run_loop));
    run_loop.Run();
  }

  // GetFileInfo() against the same file shall not issue a new
  // GetChildDocuments() call.
  EXPECT_EQ(last_count, fake_file_system_.get_child_documents_count());
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoWithCacheExpired) {
  root_->SetDirectoryCacheExpireSoonForTesting();

  {
    base::RunLoop run_loop;
    root_->GetFileInfo(
        base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
        base::BindOnce([](base::RunLoop* run_loop, base::File::Error error,
                          const base::File::Info& info) { run_loop->Quit(); },
                       &run_loop));
    run_loop.Run();
  }

  int last_count = fake_file_system_.get_child_documents_count();

  // Make sure directory caches expire.
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    root_->GetFileInfo(
        base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
        base::BindOnce([](base::RunLoop* run_loop, base::File::Error error,
                          const base::File::Info& info) { run_loop->Quit(); },
                       &run_loop));
    run_loop.Run();
  }

  // If cache expires, two GetChildDocuments() calls will be issued for
  // "/" and "/dir".
  EXPECT_EQ(last_count + 2, fake_file_system_.get_child_documents_count());
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectory) {
  base::RunLoop run_loop;
  root_->ReadDirectory(
      base::FilePath(FILE_PATH_LITERAL("dir")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error,
             std::vector<ArcDocumentsProviderRoot::ThinFileInfo> file_list) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
            ASSERT_EQ(4u, file_list.size());
            EXPECT_EQ(FILE_PATH_LITERAL("music.bin.mp3"), file_list[0].name);
            EXPECT_EQ("music-id", file_list[0].document_id);
            EXPECT_FALSE(file_list[0].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(44), file_list[0].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("no-delete.jpg"), file_list[1].name);
            EXPECT_EQ("no-delete-id", file_list[1].document_id);
            EXPECT_FALSE(file_list[1].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(45), file_list[1].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("no-rename.jpg"), file_list[2].name);
            EXPECT_EQ("no-rename-id", file_list[2].document_id);
            EXPECT_FALSE(file_list[2].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(46), file_list[2].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("photo.jpg"), file_list[3].name);
            EXPECT_EQ("photo-id", file_list[3].document_id);
            EXPECT_FALSE(file_list[3].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(33), file_list[3].last_modified);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectoryRoot) {
  base::RunLoop run_loop;
  root_->ReadDirectory(
      base::FilePath(FILE_PATH_LITERAL("")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error,
             std::vector<ArcDocumentsProviderRoot::ThinFileInfo> file_list) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
            ASSERT_EQ(3u, file_list.size());
            EXPECT_EQ(FILE_PATH_LITERAL("dir"), file_list[0].name);
            EXPECT_EQ("dir-id", file_list[0].document_id);
            EXPECT_TRUE(file_list[0].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(22), file_list[0].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("dups"), file_list[1].name);
            EXPECT_EQ("dups-id", file_list[1].document_id);
            EXPECT_TRUE(file_list[1].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(55), file_list[1].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("ro-dir"), file_list[2].name);
            EXPECT_EQ("ro-dir-id", file_list[2].document_id);
            EXPECT_TRUE(file_list[2].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(56), file_list[2].last_modified);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectoryNoSuchDirectory) {
  base::RunLoop run_loop;
  root_->ReadDirectory(
      base::FilePath(FILE_PATH_LITERAL("missing")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error,
             std::vector<ArcDocumentsProviderRoot::ThinFileInfo> file_list) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
            EXPECT_EQ(0u, file_list.size());
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectoryDups) {
  base::RunLoop run_loop;
  root_->ReadDirectory(
      base::FilePath(FILE_PATH_LITERAL("dups")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error,
             std::vector<ArcDocumentsProviderRoot::ThinFileInfo> file_list) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
            ASSERT_EQ(4u, file_list.size());
            // Files are sorted lexicographically.
            EXPECT_EQ(FILE_PATH_LITERAL("dup (1).mp4"), file_list[0].name);
            EXPECT_EQ("dup2-id", file_list[0].document_id);
            EXPECT_FALSE(file_list[0].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(77), file_list[0].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("dup (2).mp4"), file_list[1].name);
            EXPECT_EQ("dup3-id", file_list[1].document_id);
            EXPECT_FALSE(file_list[1].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(88), file_list[1].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("dup (3).mp4"), file_list[2].name);
            EXPECT_EQ("dup4-id", file_list[2].document_id);
            EXPECT_FALSE(file_list[2].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(99), file_list[2].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("dup.mp4"), file_list[3].name);
            EXPECT_EQ("dup1-id", file_list[3].document_id);
            EXPECT_FALSE(file_list[3].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(66), file_list[3].last_modified);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectoryWithCache) {
  {
    base::RunLoop run_loop;
    root_->ReadDirectory(
        base::FilePath(FILE_PATH_LITERAL("dir")),
        base::BindOnce(
            [](base::RunLoop* run_loop, base::File::Error error,
               std::vector<ArcDocumentsProviderRoot::ThinFileInfo> file_list) {
              run_loop->Quit();
            },
            &run_loop));
    run_loop.Run();
  }

  int last_count = fake_file_system_.get_child_documents_count();

  {
    base::RunLoop run_loop;
    root_->ReadDirectory(
        base::FilePath(FILE_PATH_LITERAL("dir")),
        base::BindOnce(
            [](base::RunLoop* run_loop, base::File::Error error,
               std::vector<ArcDocumentsProviderRoot::ThinFileInfo> file_list) {
              run_loop->Quit();
            },
            &run_loop));
    run_loop.Run();
  }

  // ReadDirectory() against the same directory shall issue one new
  // GetChildDocuments() call.
  EXPECT_EQ(last_count + 1, fake_file_system_.get_child_documents_count());
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectoryWithCacheExpired) {
  root_->SetDirectoryCacheExpireSoonForTesting();

  {
    base::RunLoop run_loop;
    root_->ReadDirectory(
        base::FilePath(FILE_PATH_LITERAL("dir")),
        base::BindOnce(
            [](base::RunLoop* run_loop, base::File::Error error,
               std::vector<ArcDocumentsProviderRoot::ThinFileInfo> file_list) {
              run_loop->Quit();
            },
            &run_loop));
    run_loop.Run();
  }

  int last_count = fake_file_system_.get_child_documents_count();

  // Make sure directory caches expire.
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    root_->ReadDirectory(
        base::FilePath(FILE_PATH_LITERAL("dir")),
        base::BindOnce(
            [](base::RunLoop* run_loop, base::File::Error error,
               std::vector<ArcDocumentsProviderRoot::ThinFileInfo> file_list) {
              run_loop->Quit();
            },
            &run_loop));
    run_loop.Run();
  }

  // If cache expires, two GetChildDocuments() calls will be issued for
  // "/" and "/dir".
  EXPECT_EQ(last_count + 2, fake_file_system_.get_child_documents_count());
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectoryPendingCallbacks) {
  int num_callbacks = 0;

  int last_count = fake_file_system_.get_child_documents_count();

  for (int i = 0; i < 3; ++i) {
    root_->ReadDirectory(
        base::FilePath(FILE_PATH_LITERAL("dir")),
        base::BindOnce(
            [](int* num_callbacks, base::File::Error error,
               std::vector<ArcDocumentsProviderRoot::ThinFileInfo> file_list) {
              ++*num_callbacks;
            },
            &num_callbacks));
  }

  // FakeFileSystemInstance guarantees callbacks are not invoked immediately,
  // so callbacks to ReadDirectory() have not been called at this point.
  EXPECT_EQ(0, num_callbacks);

  // GetChildDocuments() should have been called only once even though we called
  // ReadDirectory() three times due to batching.
  EXPECT_EQ(last_count + 1, fake_file_system_.get_child_documents_count());

  // Process FakeFileSystemInstance callbacks.
  base::RunLoop().RunUntilIdle();

  // All callbacks should have been invoked.
  EXPECT_EQ(3, num_callbacks);
}

TEST_F(ArcDocumentsProviderRootTest, DeleteFile) {
  // Make sure that dir/photo.jpg exists.
  ASSERT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg"))));

  base::RunLoop run_loop;
  root_->DeleteFile(base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
                    base::BindOnce(
                        [](base::RunLoop* run_loop, base::File::Error error) {
                          run_loop->Quit();
                          EXPECT_EQ(base::File::FILE_OK, error);
                        },
                        &run_loop));
  run_loop.Run();
  // dir/photo.jpg should have been removed.
  EXPECT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg"))));
}

TEST_F(ArcDocumentsProviderRootTest, DeleteFileNonExist) {
  base::RunLoop run_loop;
  root_->DeleteFile(base::FilePath(FILE_PATH_LITERAL("dir/non_exist.jpg")),
                    base::BindOnce(
                        [](base::RunLoop* run_loop, base::File::Error error) {
                          run_loop->Quit();
                          // An attempt to delete a non-existent file should
                          // return NOT_FOUND error.
                          EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
                        },
                        &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, DeleteFileNonDeletable) {
  base::RunLoop run_loop;
  root_->DeleteFile(base::FilePath(FILE_PATH_LITERAL("dir/no-delete.jpg")),
                    base::BindOnce(
                        [](base::RunLoop* run_loop, base::File::Error error) {
                          run_loop->Quit();
                          // This operation fails not because of the tested
                          // class but because the FakeFileSystemInstance is
                          // handling it.
                          // TODO(fukino): Handle this failure in the
                          // ArcDocumentsProviderRoot class to avoid unnecessary
                          // Mojo calls. crbug.com/956852.
                          EXPECT_EQ(base::File::FILE_ERROR_FAILED, error);
                        },
                        &run_loop));
  run_loop.Run();
  // dir/no-delete.jpg shouldn't have been deleted
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/no-delete.jpg"))));
}

TEST_F(ArcDocumentsProviderRootTest, DeleteDirectory) {
  // Make sure that dir (directory) exists.
  ASSERT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir"))));
  // Confirm that "dir" is actually a directory.
  ASSERT_EQ(kAndroidDirectoryMimeType,
            fake_file_system_
                .GetDocument(kAuthority, kRootSpec.document_id,
                             base::FilePath(FILE_PATH_LITERAL("dir")))
                .mime_type);
  EXPECT_TRUE(
      fake_file_system_.DocumentExists(kAuthority, kDirSpec.document_id));
  base::RunLoop run_loop;
  root_->DeleteFile(base::FilePath(FILE_PATH_LITERAL("dir")),
                    base::BindOnce(
                        [](base::RunLoop* run_loop, base::File::Error error) {
                          run_loop->Quit();
                          EXPECT_EQ(base::File::FILE_OK, error);
                        },
                        &run_loop));
  run_loop.Run();
  // dir should have been removed.
  EXPECT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir"))));
}

TEST_F(ArcDocumentsProviderRootTest, CreateFile) {
  // Make sure that dir/new.txt doesn't exist.
  ASSERT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/new.txt"))));
  base::RunLoop run_loop;
  root_->CreateFile(base::FilePath(FILE_PATH_LITERAL("dir/new.txt")),
                    base::BindOnce(
                        [](base::RunLoop* run_loop, base::File::Error error) {
                          run_loop->Quit();
                          EXPECT_EQ(base::File::FILE_OK, error);
                        },
                        &run_loop));
  run_loop.Run();
  // The dir/new.txt should have been created.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/new.txt"))));
  // The *.txt file's MIME type should be regarded as "text/plain".
  EXPECT_EQ("text/plain",
            fake_file_system_
                .GetDocument(kAuthority, kRootSpec.document_id,
                             base::FilePath(FILE_PATH_LITERAL("dir/new.txt")))
                .mime_type);
}

TEST_F(ArcDocumentsProviderRootTest, CreateFileWithFallbackMimeType) {
  base::RunLoop run_loop;
  root_->CreateFile(base::FilePath(FILE_PATH_LITERAL("dir/new")),
                    base::BindOnce(
                        [](base::RunLoop* run_loop, base::File::Error error) {
                          run_loop->Quit();
                          EXPECT_EQ(base::File::FILE_OK, error);
                        },
                        &run_loop));
  run_loop.Run();
  // The dir/new.txt should have been created.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/new"))));
  // If the file's extension is unknown, MIME type should be regarded as
  // "application/octet-stream" as a fallback.
  EXPECT_EQ("application/octet-stream",
            fake_file_system_
                .GetDocument(kAuthority, kRootSpec.document_id,
                             base::FilePath(FILE_PATH_LITERAL("dir/new")))
                .mime_type);
}

TEST_F(ArcDocumentsProviderRootTest, CreateFileAlreadyExists) {
  // Make sure that dir/new.txt already exists.
  ASSERT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg"))));
  base::RunLoop run_loop;
  root_->CreateFile(base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
                    base::BindOnce(
                        [](base::RunLoop* run_loop, base::File::Error error) {
                          run_loop->Quit();
                          EXPECT_EQ(base::File::FILE_ERROR_EXISTS, error);
                        },
                        &run_loop));
  run_loop.Run();
  // The dir/photo.jpg is still there.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg"))));
  // The document ID of dir/photo.jpg shouldn't have changed.
  EXPECT_EQ(kPhotoSpec.document_id,
            fake_file_system_
                .GetDocument(kAuthority, kRootSpec.document_id,
                             base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")))
                .document_id);
}

TEST_F(ArcDocumentsProviderRootTest, CreateFileParentNotFound) {
  base::RunLoop run_loop;
  root_->CreateFile(base::FilePath(FILE_PATH_LITERAL("dir3/photo.jpg")),
                    base::BindOnce(
                        [](base::RunLoop* run_loop, base::File::Error error) {
                          run_loop->Quit();
                          EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
                        },
                        &run_loop));
  run_loop.Run();
  EXPECT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir3/photo.jpg"))));
}

TEST_F(ArcDocumentsProviderRootTest, CreateFileInReadOnlyDirectory) {
  base::RunLoop run_loop;
  root_->CreateFile(base::FilePath(FILE_PATH_LITERAL("ro-dir/photo.jpg")),
                    base::BindOnce(
                        [](base::RunLoop* run_loop, base::File::Error error) {
                          run_loop->Quit();
                          // This operation fails not because of the tested
                          // class but because the FakeFileSystemInstance is
                          // handling it.
                          // TODO(fukino): Handle this failure in the
                          // ArcDocumentsProviderRoot class to avoid unnecessary
                          // Mojo calls. crbug.com/956852.
                          EXPECT_EQ(base::File::FILE_ERROR_FAILED, error);
                        },
                        &run_loop));
  run_loop.Run();
  // ro-dir/photo.jpg shouldn't have been created.
  EXPECT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir3/photo.jpg"))));
}

TEST_F(ArcDocumentsProviderRootTest, CreateDirectory) {
  // Make sure that directory "dir2" doesn't exist.
  ASSERT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir2"))));
  base::RunLoop run_loop;
  root_->CreateDirectory(
      base::FilePath(FILE_PATH_LITERAL("dir2")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
          },
          &run_loop));
  run_loop.Run();
  // The dir2 should have been created.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir2"))));
  // The dir2 should be a directory.
  EXPECT_EQ(kAndroidDirectoryMimeType,
            fake_file_system_
                .GetDocument(kAuthority, kRootSpec.document_id,
                             base::FilePath(FILE_PATH_LITERAL("dir2")))
                .mime_type);
}

TEST_F(ArcDocumentsProviderRootTest, CreateDirectoryExists) {
  // Make sure that directory "dir" already exists.
  ASSERT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir"))));
  base::RunLoop run_loop;
  root_->CreateDirectory(
      base::FilePath(FILE_PATH_LITERAL("dir")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_ERROR_EXISTS, error);
          },
          &run_loop));
  run_loop.Run();
  // The "dir" is still there.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir"))));
  // The document ID of "dir" shouldn't have changed.
  EXPECT_EQ(kDirSpec.document_id,
            fake_file_system_
                .GetDocument(kAuthority, kRootSpec.document_id,
                             base::FilePath(FILE_PATH_LITERAL("dir")))
                .document_id);
}

TEST_F(ArcDocumentsProviderRootTest, CreateDirectoryParentNotFound) {
  base::RunLoop run_loop;
  root_->CreateDirectory(
      base::FilePath(FILE_PATH_LITERAL("dir3/new_dir")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
          },
          &run_loop));
  run_loop.Run();
  EXPECT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir3/new_dir"))));
}

TEST_F(ArcDocumentsProviderRootTest, CopyFile) {
  base::RunLoop run_loop;
  root_->CopyFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
      base::FilePath(FILE_PATH_LITERAL("dir/photo2.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
          },
          &run_loop));
  run_loop.Run();
  // dir/photo2.jpg should be created by the copy.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo2.jpg"))));
  // The source file should still be there.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg"))));
}

TEST_F(ArcDocumentsProviderRootTest, CopyFileToDifferentDirectory) {
  base::RunLoop run_loop;
  root_->CopyFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
      base::FilePath(FILE_PATH_LITERAL("photo.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
          },
          &run_loop));
  run_loop.Run();
  // photo.jpg should be created by the copy.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("photo.jpg"))));
  // The source file should still be there.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg"))));
}

TEST_F(ArcDocumentsProviderRootTest, CopyFileSrcNotFound) {
  base::RunLoop run_loop;
  root_->CopyFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir/photo3.jpg")),
      base::FilePath(FILE_PATH_LITERAL("dir/photo2.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, CopyFileDestParentNotFound) {
  base::RunLoop run_loop;
  root_->CopyFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
      base::FilePath(FILE_PATH_LITERAL("dir3/photo2.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, RenameFile) {
  base::RunLoop run_loop;
  root_->MoveFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
      base::FilePath(FILE_PATH_LITERAL("dir/photo2.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
          },
          &run_loop));
  run_loop.Run();
  // There should be dir/photo2.jpg which was renamed from dir/photo.jpg.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo2.jpg"))));
  // The source file should be gone by rename.
  EXPECT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg"))));
  // The renamed file should have the same document ID as the source.
  EXPECT_EQ(
      kPhotoSpec.document_id,
      fake_file_system_
          .GetDocument(kAuthority, kRootSpec.document_id,
                       base::FilePath(FILE_PATH_LITERAL("dir/photo2.jpg")))
          .document_id);
}

TEST_F(ArcDocumentsProviderRootTest, RenameFileNotRenamable) {
  base::RunLoop run_loop;
  root_->MoveFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir/no-rename.jpg")),
      base::FilePath(FILE_PATH_LITERAL("dir/no-rename2.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            // This operation fails not because of the tested class but because
            // the FakeFileSystemInstance is handling it.
            // TODO(fukino): Handle this failure in the ArcDocumentsProviderRoot
            // class to avoid unnecessary Mojo calls. crbug.com/956852.
            EXPECT_EQ(base::File::FILE_ERROR_FAILED, error);
          },
          &run_loop));
  run_loop.Run();
  // dir/no-rename.jpg should still be there.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/no-rename.jpg"))));
  // dir/no-rename2.jpg shouldn't be there".
  EXPECT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/no-rename2.jpg"))));
}

TEST_F(ArcDocumentsProviderRootTest, MoveFile) {
  base::RunLoop run_loop;
  root_->MoveFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
      base::FilePath(FILE_PATH_LITERAL("photo.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
          },
          &run_loop));
  run_loop.Run();
  // There should be photo.jpg which was moved from dir/photo.jpg.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("photo.jpg"))));
  // The source file should be gone by move.
  EXPECT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg"))));
  // The moved file should have the same document ID as the source.
  EXPECT_EQ(kPhotoSpec.document_id,
            fake_file_system_
                .GetDocument(kAuthority, kRootSpec.document_id,
                             base::FilePath(FILE_PATH_LITERAL("photo.jpg")))
                .document_id);
}

TEST_F(ArcDocumentsProviderRootTest, MoveFileWithDifferentName) {
  base::RunLoop run_loop;
  root_->MoveFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
      base::FilePath(FILE_PATH_LITERAL("photo2.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
          },
          &run_loop));
  run_loop.Run();
  // There should be photo.jpg which was moved from dir/photo.jpg.
  EXPECT_TRUE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("photo2.jpg"))));
  // The source file should be gone by move.
  EXPECT_FALSE(fake_file_system_.DocumentExists(
      kAuthority, kRootSpec.document_id,
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg"))));
  // The moved file should have the same document ID as the source.
  EXPECT_EQ(kPhotoSpec.document_id,
            fake_file_system_
                .GetDocument(kAuthority, kRootSpec.document_id,
                             base::FilePath(FILE_PATH_LITERAL("photo2.jpg")))
                .document_id);
}

TEST_F(ArcDocumentsProviderRootTest, MoveFileSrcNotFound) {
  base::RunLoop run_loop;
  root_->MoveFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir3/photo.jpg")),
      base::FilePath(FILE_PATH_LITERAL("photo2.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, MoveFileDestParentNotFound) {
  base::RunLoop run_loop;
  root_->MoveFileLocal(
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
      base::FilePath(FILE_PATH_LITERAL("dir3/photo.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, WatchChanged) {
  int num_called = 0;
  auto watcher_callback = base::Bind(
      [](int* num_called, ChangeType type) {
        EXPECT_EQ(ChangeType::CHANGED, type);
        ++(*num_called);
      },
      &num_called);

  {
    base::RunLoop run_loop;
    root_->AddWatcher(base::FilePath(FILE_PATH_LITERAL("dir")),
                      watcher_callback,
                      base::Bind(
                          [](base::RunLoop* run_loop, base::File::Error error) {
                            run_loop->Quit();
                            EXPECT_EQ(base::File::FILE_OK, error);
                          },
                          &run_loop));
    run_loop.Run();
  }

  // Even if AddWatcher() returns, the watch may not have started. In order to
  // make installation finish we run the message loop until idle. This depends
  // on the behavior of FakeFileSystemInstance.
  //
  // TODO(crbug.com/698624): Remove the hack to make AddWatcher() return
  // immediately.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, num_called);
  fake_file_system_.TriggerWatchers(kAuthority, kDirSpec.document_id,
                                    storage::WatcherManager::CHANGED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_called);

  {
    base::RunLoop run_loop;
    root_->RemoveWatcher(
        base::FilePath(FILE_PATH_LITERAL("dir")),
        base::Bind(
            [](base::RunLoop* run_loop, base::File::Error error) {
              run_loop->Quit();
              EXPECT_EQ(base::File::FILE_OK, error);
            },
            &run_loop));
    run_loop.Run();
  }
}

TEST_F(ArcDocumentsProviderRootTest, WatchDeleted) {
  int num_called = 0;
  auto watcher_callback = base::Bind(
      [](int* num_called, ChangeType type) {
        EXPECT_EQ(ChangeType::DELETED, type);
        ++(*num_called);
      },
      &num_called);

  {
    base::RunLoop run_loop;
    root_->AddWatcher(base::FilePath(FILE_PATH_LITERAL("dir")),
                      watcher_callback,
                      base::Bind(
                          [](base::RunLoop* run_loop, base::File::Error error) {
                            run_loop->Quit();
                            EXPECT_EQ(base::File::FILE_OK, error);
                          },
                          &run_loop));
    run_loop.Run();
  }

  // Even if AddWatcher() returns, the watch may not have started. In order to
  // make installation finish we run the message loop until idle. This depends
  // on the behavior of FakeFileSystemInstance.
  //
  // TODO(crbug.com/698624): Remove the hack to make AddWatcher() return
  // immediately.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, num_called);
  fake_file_system_.TriggerWatchers(kAuthority, kDirSpec.document_id,
                                    storage::WatcherManager::DELETED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_called);

  // Even if the watched file was deleted, the watcher is still alive and we
  // should clean it up.
  {
    base::RunLoop run_loop;
    root_->RemoveWatcher(
        base::FilePath(FILE_PATH_LITERAL("dir")),
        base::Bind(
            [](base::RunLoop* run_loop, base::File::Error error) {
              run_loop->Quit();
              EXPECT_EQ(base::File::FILE_OK, error);
            },
            &run_loop));
    run_loop.Run();
  }
}

TEST_F(ArcDocumentsProviderRootTest, ResolveToContentUrl) {
  base::RunLoop run_loop;
  root_->ResolveToContentUrl(
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
      base::Bind(
          [](base::RunLoop* run_loop, const GURL& url) {
            run_loop->Quit();
            EXPECT_EQ(GURL("content://org.chromium.test/document/photo-id"),
                      url);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ResolveToContentUrlRoot) {
  base::RunLoop run_loop;
  root_->ResolveToContentUrl(
      base::FilePath(FILE_PATH_LITERAL("")),
      base::Bind(
          [](base::RunLoop* run_loop, const GURL& url) {
            run_loop->Quit();
            EXPECT_EQ(GURL("content://org.chromium.test/document/root-id"),
                      url);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ResolveToContentUrlNoSuchFile) {
  base::RunLoop run_loop;
  root_->ResolveToContentUrl(base::FilePath(FILE_PATH_LITERAL("missing")),
                             base::Bind(
                                 [](base::RunLoop* run_loop, const GURL& url) {
                                   run_loop->Quit();
                                   EXPECT_EQ(GURL(), url);
                                 },
                                 &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ResolveToContentUrlDups) {
  base::RunLoop run_loop;
  // "dup 2.mp4" should map to the 3rd instance of "dup.mp4" regardless of the
  // order returned from FileSystemInstance.
  root_->ResolveToContentUrl(
      base::FilePath(FILE_PATH_LITERAL("dups/dup (2).mp4")),
      base::Bind(
          [](base::RunLoop* run_loop, const GURL& url) {
            run_loop->Quit();
            EXPECT_EQ(GURL("content://org.chromium.test/document/dup3-id"),
                      url);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetMetadataNonDeletable) {
  base::RunLoop run_loop;
  root_->GetMetadata(
      base::FilePath(FILE_PATH_LITERAL("dir/no-delete.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error,
             const ArcDocumentsProviderRoot::ExtraFileMetadata& metadata) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
            EXPECT_FALSE(metadata.supports_delete);
            EXPECT_TRUE(metadata.supports_rename);
            EXPECT_TRUE(metadata.dir_supports_create);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetMetadataNonRenamable) {
  base::RunLoop run_loop;
  root_->GetMetadata(
      base::FilePath(FILE_PATH_LITERAL("dir/no-rename.jpg")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error,
             const ArcDocumentsProviderRoot::ExtraFileMetadata& metadata) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
            EXPECT_TRUE(metadata.supports_delete);
            EXPECT_FALSE(metadata.supports_rename);
            EXPECT_TRUE(metadata.dir_supports_create);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetMetadataReadOnlyDirectory) {
  base::RunLoop run_loop;
  root_->GetMetadata(
      base::FilePath(FILE_PATH_LITERAL("ro-dir")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error,
             const ArcDocumentsProviderRoot::ExtraFileMetadata& metadata) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_OK, error);
            EXPECT_FALSE(metadata.supports_delete);
            EXPECT_FALSE(metadata.supports_rename);
            EXPECT_FALSE(metadata.dir_supports_create);
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetMetadataNonExist) {
  base::RunLoop run_loop;
  root_->GetMetadata(
      base::FilePath(FILE_PATH_LITERAL("dir/no-exist-file")),
      base::BindOnce(
          [](base::RunLoop* run_loop, base::File::Error error,
             const ArcDocumentsProviderRoot::ExtraFileMetadata& metadata) {
            run_loop->Quit();
            EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
            EXPECT_FALSE(metadata.supports_delete);
            EXPECT_FALSE(metadata.supports_rename);
            EXPECT_FALSE(metadata.dir_supports_create);
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace arc
