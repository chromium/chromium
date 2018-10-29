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
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/file_system.mojom.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/watcher_manager.h"
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
};

// Fake file system hierarchy:
//
// <path>            <type>      <ID>
// (root)/           dir         root-id
//   dir/            dir         dir-id
//     photo.jpg     image/jpeg  photo-id
//     music.bin     audio/mp3   music-id
//   dups/           dir         dups-id
//     dup.mp4       video/mp4   dup1-id
//     dup.mp4       video/mp4   dup2-id
//     dup.mp4       video/mp4   dup3-id
//     dup.mp4       video/mp4   dup4-id
constexpr char kAuthority[] = "org.chromium.test";
constexpr DocumentSpec kRootSpec{"root-id", "", "", kAndroidDirectoryMimeType,
                                 -1,        0};
constexpr DocumentSpec kDirSpec{
    "dir-id", kRootSpec.document_id, "dir", kAndroidDirectoryMimeType, -1, 22};
constexpr DocumentSpec kPhotoSpec{
    "photo-id", kDirSpec.document_id, "photo.jpg", "image/jpeg", 3, 33};
constexpr DocumentSpec kMusicSpec{
    "music-id", kDirSpec.document_id, "music.bin", "audio/mp3", 4, 44};
constexpr DocumentSpec kDupsSpec{"dups-id", kRootSpec.document_id,
                                 "dups",    kAndroidDirectoryMimeType,
                                 -1,        55};
constexpr DocumentSpec kDup1Spec{
    "dup1-id", kDupsSpec.document_id, "dup.mp4", "video/mp4", 6, 66};
constexpr DocumentSpec kDup2Spec{
    "dup2-id", kDupsSpec.document_id, "dup.mp4", "video/mp4", 7, 77};
constexpr DocumentSpec kDup3Spec{
    "dup3-id", kDupsSpec.document_id, "dup.mp4", "video/mp4", 8, 88};
constexpr DocumentSpec kDup4Spec{
    "dup4-id", kDupsSpec.document_id, "dup.mp4", "video/mp4", 9, 99};

// The order is intentionally shuffled here so that
// FileSystemInstance::GetChildDocuments() returns documents in shuffled order.
// See ResolveToContentUrlDups test below.
constexpr DocumentSpec kAllSpecs[] = {kRootSpec,  kDirSpec,  kPhotoSpec,
                                      kMusicSpec, kDupsSpec, kDup2Spec,
                                      kDup1Spec,  kDup4Spec, kDup3Spec};

Document ToDocument(const DocumentSpec& spec) {
  return Document(kAuthority, spec.document_id, spec.parent_document_id,
                  spec.display_name, spec.mime_type, spec.size,
                  spec.last_modified);
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
        kAuthority, kRootSpec.document_id);
  }

  void TearDown() override {
    root_.reset();
    arc_service_manager_->arc_bridge_service()->file_system()->CloseInstance(
        &fake_file_system_);

    // Run all pending tasks before destroying testing profile.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
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
            ASSERT_EQ(2u, file_list.size());
            EXPECT_EQ(FILE_PATH_LITERAL("music.bin.mp3"), file_list[0].name);
            EXPECT_EQ("music-id", file_list[0].document_id);
            EXPECT_FALSE(file_list[0].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(44), file_list[0].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("photo.jpg"), file_list[1].name);
            EXPECT_EQ("photo-id", file_list[1].document_id);
            EXPECT_FALSE(file_list[1].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(33), file_list[1].last_modified);
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
            ASSERT_EQ(2u, file_list.size());
            EXPECT_EQ(FILE_PATH_LITERAL("dir"), file_list[0].name);
            EXPECT_EQ("dir-id", file_list[0].document_id);
            EXPECT_TRUE(file_list[0].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(22), file_list[0].last_modified);
            EXPECT_EQ(FILE_PATH_LITERAL("dups"), file_list[1].name);
            EXPECT_EQ("dups-id", file_list[1].document_id);
            EXPECT_TRUE(file_list[1].is_directory);
            EXPECT_EQ(base::Time::FromJavaTime(55), file_list[1].last_modified);
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

}  // namespace arc
