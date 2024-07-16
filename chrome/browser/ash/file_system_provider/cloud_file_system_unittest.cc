// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using base::test::IsNotNullCallback;
using base::test::RunClosure;
using base::test::RunOnceCallback;
using base::test::TestFuture;
using testing::_;
using testing::DoAll;
using testing::Field;
using testing::IsEmpty;
using testing::IsFalse;
using testing::Return;
using testing::ReturnRef;

using OpenFileFuture =
    TestFuture<int, base::File::Error, std::unique_ptr<EntryMetadata>>;
using ReadFileFuture = TestFuture<int, bool, base::File::Error>;
using GetMetadataFuture =
    TestFuture<std::unique_ptr<EntryMetadata>, base::File::Error>;
using FileErrorFuture = TestFuture<base::File::Error>;

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "cloud-fs-id";
const char kDisplayName[] = "Cloud FS";

// An action for a mock function which is equivalent to `RunOnceCallback<I>()`,
// `run_loop.Quit()` and `Return(AbortCallback())`. That is, it invokes the
// Run() method on the I-th (0-based) argument of the mock function with
// argument `result`, quits the `run_loop` and then returns an
// `AbortCallback()`. This is used when mocking `ProvidedFileSystemInterface`
// functions that have a `StatusCallback`. This waits for the function to be
// called, runs the callback and returns an AbortCallback().
template <size_t I>
auto RunStatusCallbackAndQuitRunLoopAndReturnAbortCallback(
    base::RunLoop& run_loop,
    base::File::Error result) {
  return [&run_loop, result](auto&&... args) -> AbortCallback {
    auto callback =
        std::move(base::gmock_callback_support_internal::get<I>(args...));
    std::move(callback).Run(result);
    run_loop.Quit();
    return AbortCallback();
  };
}

class MockCacheManager : public CacheManager {
 public:
  MOCK_METHOD(void,
              InitializeForProvider,
              (const ProvidedFileSystemInfo& file_system_info,
               FileErrorOrContentCacheCallback callback),
              (override));
  MOCK_METHOD(void,
              UninitializeForProvider,
              (const ProvidedFileSystemInfo& file_system_info),
              (override));
  MOCK_METHOD(bool,
              IsProviderInitialized,
              (const ProvidedFileSystemInfo& file_system_info),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
};

class MockContentCache : public ContentCache {
 public:
  MOCK_METHOD(void, SetMaxCacheItems, (size_t max_cache_items), (override));
  MOCK_METHOD(void,
              ReadBytes,
              (const OpenedCloudFile& file,
               scoped_refptr<net::IOBuffer> buffer,
               int64_t offset,
               int length,
               ProvidedFileSystemInterface::ReadChunkReceivedCallback callback),
              (override));
  MOCK_METHOD(void,
              WriteBytes,
              (const OpenedCloudFile& file,
               scoped_refptr<net::IOBuffer> buffer,
               int64_t offset,
               int length,
               FileErrorCallback callback),
              (override));
  MOCK_METHOD(void, CloseFile, (const OpenedCloudFile& file), (override));
  MOCK_METHOD(void, LoadFromDisk, (base::OnceClosure callback), (override));
  MOCK_METHOD(std::vector<base::FilePath>, GetCachedFilePaths, (), (override));
  MOCK_METHOD(void,
              Notify,
              (ProvidedFileSystemObserver::Changes & changes),
              (override));
  MOCK_METHOD(void,
              ObservedVersionTag,
              (const base::FilePath& entry_path,
               const std::string& version_tag),
              (override));
  MOCK_METHOD(void, Evict, (const base::FilePath& file_path), (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));

  base::WeakPtr<MockContentCache> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockContentCache> weak_ptr_factory_{this};
};

class MockContentCacheObserver : public ContentCache::Observer {
 public:
  MOCK_METHOD(void,
              OnItemEvicted,
              (const base::FilePath& fsp_path),
              (override));
  MOCK_METHOD(void,
              OnItemRemovedFromDisk,
              (const base::FilePath& fsp_path, int64_t bytes_removed),
              (override));
};

class MockProvidedFileSystem : public ProvidedFileSystemInterface {
 public:
  MOCK_METHOD(AbortCallback,
              RequestUnmount,
              (storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              GetMetadata,
              (const base::FilePath& entry_path,
               ProvidedFileSystemInterface::MetadataFieldMask fields,
               ProvidedFileSystemInterface::GetMetadataCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              GetActions,
              (const std::vector<base::FilePath>& entry_paths,
               GetActionsCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              ExecuteAction,
              (const std::vector<base::FilePath>& entry_paths,
               const std::string& action_id,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              ReadDirectory,
              (const base::FilePath& directory_path,
               storage::AsyncFileUtil::ReadDirectoryCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              OpenFile,
              (const base::FilePath& file_path,
               OpenFileMode mode,
               OpenFileCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              CloseFile,
              (int file_handle,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              ReadFile,
              (int file_handle,
               net::IOBuffer* buffer,
               int64_t offset,
               int length,
               ReadChunkReceivedCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              CreateDirectory,
              (const base::FilePath& directory_path,
               bool recursive,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              DeleteEntry,
              (const base::FilePath& entry_path,
               bool recursive,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              CreateFile,
              (const base::FilePath& file_path,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              CopyEntry,
              (const base::FilePath& source_path,
               const base::FilePath& target_path,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              MoveEntry,
              (const base::FilePath& source_path,
               const base::FilePath& target_path,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              Truncate,
              (const base::FilePath& file_path,
               int64_t length,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              WriteFile,
              (int file_handle,
               net::IOBuffer* buffer,
               int64_t offset,
               int length,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(AbortCallback,
              FlushFile,
              (int file_handle,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(
      AbortCallback,
      AddWatcher,
      (const GURL& origin,
       const base::FilePath& entry_path,
       bool recursive,
       bool persistent,
       storage::AsyncFileUtil::StatusCallback callback,
       storage::WatcherManager::NotificationCallback notification_callback),
      (override));
  MOCK_METHOD(void,
              RemoveWatcher,
              (const GURL& origin,
               const base::FilePath& entry_path,
               bool recursive,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(const ProvidedFileSystemInfo&,
              GetFileSystemInfo,
              (),
              (const, override));
  MOCK_METHOD(OperationRequestManager*, GetRequestManager, (), (override));
  MOCK_METHOD(Watchers*, GetWatchers, (), (override));
  MOCK_METHOD(const OpenedFiles&, GetOpenedFiles, (), (const, override));
  MOCK_METHOD(void,
              AddObserver,
              (ProvidedFileSystemObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (ProvidedFileSystemObserver * observer),
              (override));
  MOCK_METHOD(void,
              Notify,
              (const base::FilePath& entry_path,
               bool recursive,
               storage::WatcherManager::ChangeType change_type,
               std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
               const std::string& tag,
               storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(void,
              Configure,
              (storage::AsyncFileUtil::StatusCallback callback),
              (override));
  MOCK_METHOD(base::WeakPtr<ProvidedFileSystemInterface>,
              GetWeakPtr,
              (),
              (override));
  MOCK_METHOD(std::unique_ptr<ScopedUserInteraction>,
              StartUserInteraction,
              (),
              (override));

  base::WeakPtr<MockProvidedFileSystem> GetMockWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockProvidedFileSystem> weak_ptr_factory_{this};
};

// Holder for the constructed mock content cache and the cloud file system.
struct MockContentCacheAndCloudFileSystemAndFakeFsp {
  base::WeakPtr<MockContentCache> mock_content_cache;
  base::WeakPtr<FakeProvidedFileSystem> fake_fsp;
  std::unique_ptr<CloudFileSystem> cloud_file_system;
};

struct MockContentCacheObserverAndCloudFileSystemAndFakeFsp {
  std::unique_ptr<MockContentCacheObserver> mock_content_cache_observer;
  base::WeakPtr<FakeProvidedFileSystem> fake_fsp;
  std::unique_ptr<CloudFileSystem> cloud_file_system;
};

class FileSystemProviderCloudFileSystemTest : public testing::Test,
                                              public CacheManager::Observer {
 protected:
  FileSystemProviderCloudFileSystemTest() = default;
  ~FileSystemProviderCloudFileSystemTest() override = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  const ProvidedFileSystemInfo GetFileSystemInfo(bool with_mock_cache_manager) {
    MountOptions mount_options;
    mount_options.file_system_id = kFileSystemId;
    mount_options.display_name = kDisplayName;
    mount_options.supports_notify_tag = true;
    mount_options.writable = true;
    const base::FilePath mount_path = util::GetMountPath(
        profile_.get(), ProviderId::CreateFromExtensionId(kExtensionId),
        kFileSystemId);
    return ProvidedFileSystemInfo(
        kExtensionId, mount_options, mount_path,
        /*configurable=*/false,
        /*watchable=*/true, extensions::SOURCE_NETWORK, IconSet(),
        with_mock_cache_manager ? CacheType::LRU : CacheType::NONE);
  }

  // Creates a CloudFileSystem which wraps a FakeProvidedFileSystem.
  std::unique_ptr<CloudFileSystem> CreateCloudFileSystem(
      std::unique_ptr<ProvidedFileSystemInterface> provided_file_system,
      bool with_mock_cache_manager) {
    // Start the CloudFileSystem initialisation.
    std::unique_ptr<CloudFileSystem> cloud_file_system =
        std::make_unique<CloudFileSystem>(
            std::move(provided_file_system),
            with_mock_cache_manager ? &mock_cache_manager_ : nullptr);
    return cloud_file_system;
  }

  base::WeakPtr<MockContentCache> CreateMockContentCache() {
    std::unique_ptr<MockContentCache> mock_content_cache =
        std::make_unique<MockContentCache>();
    base::WeakPtr<MockContentCache> cache_weak_ptr =
        mock_content_cache->GetWeakPtr();
    EXPECT_CALL(mock_cache_manager_,
                InitializeForProvider(_, IsNotNullCallback()))
        .WillOnce(RunOnceCallback<1>(std::move(mock_content_cache)));
    return cache_weak_ptr;
  }

  std::unique_ptr<MockContentCacheObserver>
  CreateContentCacheAndMockObserver() {
    EXPECT_TRUE(temp_cache_dir_.CreateUniqueTempDir());

    // Initialize a `ContextDatabase` in memory on a blocking task runner.
    std::unique_ptr<ContextDatabase> context_db =
        std::make_unique<ContextDatabase>(base::FilePath());
    scoped_refptr<base::SequencedTaskRunner> db_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    BoundContextDatabase db(db_task_runner, std::move(context_db));
    TestFuture<bool> future;
    db.AsyncCall(&ContextDatabase::Initialize).Then(future.GetCallback());
    EXPECT_TRUE(future.Get());

    auto content_cache_observer = std::make_unique<MockContentCacheObserver>();
    std::unique_ptr<ContentCache> content_cache =
        ContentCacheImpl::Create(temp_cache_dir_.GetPath(), std::move(db));
    content_cache->AddObserver(content_cache_observer.get());

    EXPECT_CALL(mock_cache_manager_,
                InitializeForProvider(_, IsNotNullCallback()))
        .WillOnce(RunOnceCallback<1>(std::move(content_cache)));
    return content_cache_observer;
  }

  MockContentCacheObserverAndCloudFileSystemAndFakeFsp
  CreateContentCacheAndObserverAndCloudFileSystemWithFakeFsp() {
    std::unique_ptr<FakeProvidedFileSystem> provided_file_system =
        std::make_unique<FakeProvidedFileSystem>(
            GetFileSystemInfo(/*with_mock_cache_manager=*/true));
    return MockContentCacheObserverAndCloudFileSystemAndFakeFsp{
        .mock_content_cache_observer = CreateContentCacheAndMockObserver(),
        .fake_fsp = provided_file_system->GetFakeWeakPtr(),
        .cloud_file_system = CreateCloudFileSystem(
            std::move(provided_file_system), /*with_mock_cache_manager=*/true)};
  }

  MockContentCacheAndCloudFileSystemAndFakeFsp
  CreateMockContentCacheAndCloudFileSystemWithFakeFsp() {
    std::unique_ptr<FakeProvidedFileSystem> provided_file_system =
        std::make_unique<FakeProvidedFileSystem>(
            GetFileSystemInfo(/*with_mock_cache_manager=*/true));
    return MockContentCacheAndCloudFileSystemAndFakeFsp{
        .mock_content_cache = CreateMockContentCache(),
        .fake_fsp = provided_file_system->GetFakeWeakPtr(),
        .cloud_file_system = CreateCloudFileSystem(
            std::move(provided_file_system), /*with_mock_cache_manager=*/true)};
  }

  void CloseFileSuccessfully(CloudFileSystem& cloud_file_system,
                             int file_handle) {
    FileErrorFuture close_file_future;
    cloud_file_system.CloseFile(file_handle, close_file_future.GetCallback());
    EXPECT_EQ(close_file_future.Get(), base::File::FILE_OK);
  }

  void ReadFileSuccessfully(CloudFileSystem& cloud_file_system,
                            int file_handle,
                            scoped_refptr<net::IOBuffer> buffer,
                            int64_t offset = 0,
                            int length = 1) {
    ReadFileFuture read_file_future;
    cloud_file_system.ReadFile(file_handle, buffer.get(), offset, length,
                               read_file_future.GetRepeatingCallback());
    EXPECT_EQ(read_file_future.Get<int>(), 1);
    EXPECT_EQ(read_file_future.Get<base::File::Error>(), base::File::FILE_OK);
  }

  void WriteFileSuccessfully(CloudFileSystem& cloud_file_system,
                             int file_handle,
                             scoped_refptr<net::IOBuffer> buffer,
                             int64_t offset = 0,
                             int length = 1) {
    FileErrorFuture write_file_future;
    cloud_file_system.WriteFile(file_handle, buffer.get(), offset, length,
                                write_file_future.GetRepeatingCallback());
    EXPECT_EQ(write_file_future.Get(), base::File::FILE_OK);
  }

  void DeleteFileSuccessfully(CloudFileSystem& cloud_file_system,
                              const base::FilePath& entry_path) {
    FileErrorFuture delete_file_future;
    cloud_file_system.DeleteEntry(entry_path, /*recursive=*/false,
                                  delete_file_future.GetRepeatingCallback());
    EXPECT_EQ(delete_file_future.Get(), base::File::FILE_OK);
  }

  int GetFileHandleFromSuccessfulOpenFile(
      CloudFileSystem& cloud_file_system,
      const base::FilePath& file_path,
      OpenFileMode mode = OPEN_FILE_MODE_READ) {
    OpenFileFuture open_file_future;
    cloud_file_system.OpenFile(file_path, mode, open_file_future.GetCallback());
    EXPECT_EQ(open_file_future.Get<base::File::Error>(), base::File::FILE_OK);
    return open_file_future.Get<int>();
  }

  void DeleteEntryOnFakeFileSystem(
      base::WeakPtr<FakeProvidedFileSystem> fake_fsp,
      const base::FilePath& entry_path) {
    FileErrorFuture delete_entry_future;
    fake_fsp->DeleteEntry(entry_path, /*recursive=*/true,
                          delete_entry_future.GetCallback());
    EXPECT_EQ(delete_entry_future.Get(), base::File::FILE_OK);
  }

  void UpdateEntryWithVersionTagOnFakeFileSystem(
      base::WeakPtr<FakeProvidedFileSystem> fake_fsp,
      const base::FilePath& entry_path,
      const std::string& version_tag) {
    fake_fsp->GetEntry(entry_path)->metadata->cloud_file_info->version_tag =
        version_tag;
  }

  MockCacheManager mock_cache_manager_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_cache_dir_;
};

TEST_F(FileSystemProviderCloudFileSystemTest, ContiguousReadsWriteToCache) {
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // Open the `kFakeFilePath` file to stage it in the `FakeProvidedFileSystem`.
  int file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath));

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);

  // Set the first read bytes to return `base::File::FILE_ERROR_NOT_FOUND`, this
  // indicates that the data is not cached in the content cache.
  EXPECT_CALL(*mock_content_cache, ReadBytes(_, buffer, /*offset=*/0,
                                             /*length=*/1, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(/*bytes_read=*/-1, /*has_more=*/false,
                                   base::File::FILE_ERROR_NOT_FOUND));

  // Set the first write bytes to return successfully, this indicates the post
  // FSP stream to disk succeeded.
  EXPECT_CALL(*mock_content_cache,
              WriteBytes(_, buffer, /*offset=*/0,
                         /*length=*/1, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(base::File::FILE_OK));

  // Read the first chunk.
  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer, /*offset=*/0,
                       /*length=*/1);

  // Expect watcher added for file.
  EXPECT_THAT(
      cloud_file_system->GetWatchers(),
      Pointee(ElementsAre(Pair(
          _, AllOf(Field(&Watcher::entry_path, base::FilePath(kFakeFilePath)),
                   Field(&Watcher::recursive, IsFalse()))))));

  // Read the next chunk.
  EXPECT_CALL(*mock_content_cache, ReadBytes(_, buffer, /*offset=*/1,
                                             /*length=*/1, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(/*bytes_read=*/-1, /*has_more=*/false,
                                   base::File::FILE_ERROR_NOT_FOUND));
  EXPECT_CALL(*mock_content_cache,
              WriteBytes(_, buffer, /*offset=*/1,
                         /*length=*/1, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(base::File::FILE_OK));
  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer, /*offset=*/1,
                       /*length=*/1);

  // Expect no new watcher added for the file.
  EXPECT_THAT(
      cloud_file_system->GetWatchers(),
      Pointee(ElementsAre(Pair(
          _, AllOf(Field(&Watcher::entry_path, base::FilePath(kFakeFilePath)),
                   Field(&Watcher::recursive, IsFalse()))))));

  CloseFileSuccessfully(*cloud_file_system, file_handle);
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       UpToDateItemsInCacheShouldReturnWithoutCallingTheFsp) {
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // Open the `kFakeFilePath` file to stage it in the `FakeProvidedFileSystem`.
  int file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath));

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);

  // Set the first read bytes to return `base::File::FILE_OK`, this indicates
  // that the data is fresh and available in the cache.
  EXPECT_CALL(*mock_content_cache, ReadBytes(_, buffer, /*offset=*/0,
                                             /*length=*/1, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(/*bytes_read=*/1, /*has_more=*/false,
                                   base::File::FILE_OK));

  // Expect that `WriteBytes` should not be called.
  EXPECT_CALL(*mock_content_cache, WriteBytes(_, _, _, _, _)).Times(0);

  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer);
  CloseFileSuccessfully(*cloud_file_system, file_handle);
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       WhenReadingUpToDateItemsFromCacheFailsShouldDeferToFsp) {
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // Open the `kFakeFilePath` file to stage it in the `FakeProvidedFileSystem`.
  int file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath));

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);

  // Make the inner callback to return `base::File::FILE_ERROR_FAILED` to
  // indicate that the actual underlying read of the cached file failed.
  EXPECT_CALL(*mock_content_cache, ReadBytes(_, buffer, /*offset=*/0,
                                             /*length=*/1, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(/*bytes_read=*/-1, /*has_more=*/false,
                                   base::File::FILE_ERROR_FAILED));

  // Ensure that the read still completes successfully, this indicates it
  // deferred the actual read to the underlying FSP instead of the failed cloud
  // file system.
  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer);
  CloseFileSuccessfully(*cloud_file_system, file_handle);
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       ContentCacheFailsWritingBytesShouldStillReturnSuccessfully) {
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // Open the `kFakeFilePath` file to stage it in the `FakeProvidedFileSystem`.
  int file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath));

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);

  // Set the first read bytes to return `base::File::FILE_ERROR_NOT_FOUND`, this
  // indicates that the data is not cached in the content cache.
  EXPECT_CALL(*mock_content_cache, ReadBytes(_, buffer, /*offset=*/0,
                                             /*length=*/1, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(/*bytes_read=*/-1, /*has_more=*/false,
                                   base::File::FILE_ERROR_NOT_FOUND));

  // Set the first write bytes to return `base::File::FILE_ERROR_FAILED`, this
  // simulates a failure whilst streaming to disk. Given the FSP succeeded, we
  // should succeed back to the caller and follow up requests will defer
  // straight to the FSP.
  EXPECT_CALL(*mock_content_cache,
              WriteBytes(_, buffer, /*offset=*/0,
                         /*length=*/1, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(base::File::FILE_ERROR_FAILED));

  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer);
  // Expect no watcher is added.
  EXPECT_THAT(cloud_file_system->GetWatchers(), Pointee(IsEmpty()));

  CloseFileSuccessfully(*cloud_file_system, file_handle);
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       FilesOpenForWriteShouldAlwaysGoToTheFspNotContentCache) {
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // Open the `kFakeFilePath` file to stage it in the `FakeProvidedFileSystem`.
  int file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath), OPEN_FILE_MODE_WRITE);

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);

  // Neither the `ReadBytes` nor the `WriteBytes` should be called.
  EXPECT_CALL(*mock_content_cache, ReadBytes(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*mock_content_cache, WriteBytes(_, _, _, _, _)).Times(0);

  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer);
  // Expect no watcher is added.
  EXPECT_THAT(cloud_file_system->GetWatchers(), Pointee(IsEmpty()));

  CloseFileSuccessfully(*cloud_file_system, file_handle);
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       IfFspReadFailsOnFirstCallContentCacheShouldNotWriteBytes) {
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // Open the `kFakeFilePath` file to stage it in the `FakeProvidedFileSystem`.
  const base::FilePath fake_file_path(kFakeFilePath);
  int file_handle =
      GetFileHandleFromSuccessfulOpenFile(*cloud_file_system, fake_file_path);

  // Remove the entry from the underlying FSP, this should result in a
  // base::File::FILE_ERROR_INVALID_OPERATION on the `ReadFile` request.
  DeleteEntryOnFakeFileSystem(fake_fsp, fake_file_path);

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);

  // Set the first read bytes to return `base::File::FILE_ERROR_NOT_FOUND`, this
  // simulates a cache miss.
  EXPECT_CALL(*mock_content_cache, ReadBytes(_, _, /*offset=*/0,
                                             /*length=*/1, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<4>(/*bytes_read=*/-1, /*has_more=*/false,
                                   base::File::FILE_ERROR_NOT_FOUND));

  // Assert that `WriteBytes` never gets called as the underlying FSP
  // should respond with a `base::File::FILE_ERROR_INVALID_OPERATION` due to the
  // file not existing between the `OpenFile` and the `ReadFile`.
  EXPECT_CALL(*mock_content_cache, WriteBytes(_, _, _, _, _)).Times(0);

  ReadFileFuture read_file_future;
  cloud_file_system->ReadFile(file_handle, buffer.get(), /*offset=*/0,
                              /*length=*/1,
                              read_file_future.GetRepeatingCallback());
  EXPECT_EQ(read_file_future.Get<int>(), 0);
  EXPECT_EQ(read_file_future.Get<base::File::Error>(),
            base::File::FILE_ERROR_INVALID_OPERATION);
  // Expect no watcher is added.
  EXPECT_THAT(cloud_file_system->GetWatchers(), Pointee(IsEmpty()));

  CloseFileSuccessfully(*cloud_file_system, file_handle);
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       WatchersAddedForEachFileAlreadyInTheCacheOnStartUp) {
  base::WeakPtr<MockContentCache> mock_content_cache = CreateMockContentCache();
  std::unique_ptr<MockProvidedFileSystem> provided_file_system =
      std::make_unique<MockProvidedFileSystem>();
  base::WeakPtr<MockProvidedFileSystem> mock_fsp =
      provided_file_system->GetMockWeakPtr();

  // Mock the fake file system info.
  auto file_system_info = GetFileSystemInfo(/*with_mock_cache_manager=*/true);
  EXPECT_CALL(*mock_fsp, GetFileSystemInfo())
      .WillRepeatedly(ReturnRef(file_system_info));

  // Set the content cache to already have files.
  std::vector<base::FilePath> cached_files(
      {base::FilePath("/a.txt"), base::FilePath("/b.txt")});
  EXPECT_CALL(*mock_content_cache, GetCachedFilePaths())
      .WillOnce(Return(cached_files));

  // Expect that AddWatcher is called for each cached file.
  base::RunLoop run_loop1;
  EXPECT_CALL(*mock_fsp,
              AddWatcher(GURL("chrome://content-cache/"),
                         base::FilePath("/a.txt"), /*recursive=*/false,
                         /*persistent=*/false, _, _))
      .WillOnce(RunStatusCallbackAndQuitRunLoopAndReturnAbortCallback<4>(
          run_loop1, base::File::Error::FILE_OK));
  base::RunLoop run_loop2;
  EXPECT_CALL(*mock_fsp,
              AddWatcher(GURL("chrome://content-cache/"),
                         base::FilePath("/b.txt"), /*recursive=*/false,
                         /*persistent=*/false, _, _))
      .WillOnce(RunStatusCallbackAndQuitRunLoopAndReturnAbortCallback<4>(
          run_loop2, base::File::Error::FILE_OK));

  // Initialise the CloudFileSystem.
  std::unique_ptr<CloudFileSystem> cloud_file_system = CreateCloudFileSystem(
      std::move(provided_file_system), /*with_mock_cache_manager=*/true);
  run_loop1.Run();
  run_loop2.Run();
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       RetryToAddWatcherOnSecurityErrorOnStartUp) {
  base::WeakPtr<MockContentCache> mock_content_cache = CreateMockContentCache();
  std::unique_ptr<MockProvidedFileSystem> provided_file_system =
      std::make_unique<MockProvidedFileSystem>();
  base::WeakPtr<MockProvidedFileSystem> mock_fsp =
      provided_file_system->GetMockWeakPtr();

  // Mock the fake file system info.
  auto file_system_info = GetFileSystemInfo(/*with_mock_cache_manager=*/true);
  EXPECT_CALL(*mock_fsp, GetFileSystemInfo())
      .WillRepeatedly(ReturnRef(file_system_info));

  // Set the content cache to already have a file.
  std::vector<base::FilePath> cached_files({base::FilePath("/a.txt")});
  EXPECT_CALL(*mock_content_cache, GetCachedFilePaths())
      .WillOnce(Return(cached_files));

  // Expect that AddWatcher is called for the cached file. Fail the first
  // AddWatcher call with `FILE_ERROR_SECURITY` and pass the follow up call.
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  EXPECT_CALL(*mock_fsp,
              AddWatcher(GURL("chrome://content-cache/"),
                         base::FilePath("/a.txt"), /*recursive=*/false,
                         /*persistent=*/false, IsNotNullCallback(), _))
      .WillOnce(RunStatusCallbackAndQuitRunLoopAndReturnAbortCallback<4>(
          run_loop1, base::File::Error::FILE_ERROR_SECURITY))
      .WillOnce(RunStatusCallbackAndQuitRunLoopAndReturnAbortCallback<4>(
          run_loop2, base::File::Error::FILE_OK));

  // Initialise the CloudFileSystem.
  std::unique_ptr<CloudFileSystem> cloud_file_system = CreateCloudFileSystem(
      std::move(provided_file_system), /*with_mock_cache_manager=*/true);
  run_loop1.Run();
  run_loop2.Run();
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       NoRetryToAddWatcherOnNonSecurityErrorOnStartUp) {
  base::WeakPtr<MockContentCache> mock_content_cache = CreateMockContentCache();
  std::unique_ptr<MockProvidedFileSystem> provided_file_system =
      std::make_unique<MockProvidedFileSystem>();
  base::WeakPtr<MockProvidedFileSystem> mock_fsp =
      provided_file_system->GetMockWeakPtr();

  // Mock the fake file system info.
  auto file_system_info = GetFileSystemInfo(/*with_mock_cache_manager=*/true);
  EXPECT_CALL(*mock_fsp, GetFileSystemInfo())
      .WillRepeatedly(ReturnRef(file_system_info));

  // Set the content cache to already have a file.
  std::vector<base::FilePath> cached_files({base::FilePath("/a.txt")});
  EXPECT_CALL(*mock_content_cache, GetCachedFilePaths())
      .WillOnce(Return(cached_files));

  // Expect that AddWatcher is called for the cached file. Fail the AddWatcher
  // call with `FILE_ERROR_FAILED` and expect that no follow up call is made.
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_fsp,
              AddWatcher(GURL("chrome://content-cache/"),
                         base::FilePath("/a.txt"), /*recursive=*/false,
                         /*persistent=*/false, IsNotNullCallback(), _))
      .WillOnce(RunStatusCallbackAndQuitRunLoopAndReturnAbortCallback<4>(
          run_loop, base::File::Error::FILE_ERROR_FAILED));

  // Initialise the CloudFileSystem.
  std::unique_ptr<CloudFileSystem> cloud_file_system = CreateCloudFileSystem(
      std::move(provided_file_system), /*with_mock_cache_manager=*/true);
  run_loop.Run();
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       WatcherAddedWhenFileAddedAndRemovedWhenFileEvicted) {
  // Underlying FakeProvidedFileSystem is (always) initialised with fake file
  // with kFakeFilePath.
  const base::FilePath fake_file_path(kFakeFilePath);
  auto [mock_content_cache_observer, fake_fsp, cloud_file_system] =
      CreateContentCacheAndObserverAndCloudFileSystemWithFakeFsp();

  // Add file to the cache.
  int file_handle =
      GetFileHandleFromSuccessfulOpenFile(*cloud_file_system, fake_file_path);
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);
  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer, /*offset=*/0,
                       /*length=*/1);
  CloseFileSuccessfully(*cloud_file_system, file_handle);

  // There should be a watcher added for the file.
  EXPECT_THAT(cloud_file_system->GetWatchers(),
              Pointee(UnorderedElementsAre(
                  Pair(_, AllOf(Field(&Watcher::entry_path, fake_file_path),
                                Field(&Watcher::recursive, IsFalse()))))));

  // Remove the entry from the underlying FSP, this should result in a
  // base::File::FILE_ERROR_NOT_FOUND on the `GetMetadata` request.
  DeleteEntryOnFakeFileSystem(fake_fsp, fake_file_path);

  // The file will be evicted after the unsuccessful GetMetadata request.
  EXPECT_CALL(*mock_content_cache_observer, OnItemEvicted(fake_file_path));
  GetMetadataFuture get_metadata_future;
  cloud_file_system->GetMetadata(fake_file_path,
                                 /*fields*/ {},
                                 get_metadata_future.GetRepeatingCallback());
  EXPECT_EQ(get_metadata_future.Get<base::File::Error>(),
            base::File::FILE_ERROR_NOT_FOUND);

  // Expect watcher is removed.
  EXPECT_THAT(cloud_file_system->GetWatchers(), Pointee(IsEmpty()));
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       NotFoundFromGetMetadataEvictsCachedFile) {
  // Underlying FakeProvidedFileSystem is (always) initialised with fake file
  // with kFakeFilePath.
  const base::FilePath fake_file_path(kFakeFilePath);
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // The file won't be evicted after the successful GetMetadata request.
  EXPECT_CALL(*mock_content_cache, Evict(fake_file_path)).Times(0);
  GetMetadataFuture get_metadata_future1;
  cloud_file_system->GetMetadata(fake_file_path,
                                 /*fields*/ {},
                                 get_metadata_future1.GetRepeatingCallback());
  EXPECT_EQ(get_metadata_future1.Get<base::File::Error>(), base::File::FILE_OK);

  // Remove the entry from the underlying FSP, this should result in a
  // base::File::FILE_ERROR_NOT_FOUND on the `GetMetadata` request.
  DeleteEntryOnFakeFileSystem(fake_fsp, fake_file_path);

  // The file will be evicted after the unsuccessful GetMetadata request.
  EXPECT_CALL(*mock_content_cache, Evict(fake_file_path)).Times(1);
  GetMetadataFuture get_metadata_future2;
  cloud_file_system->GetMetadata(fake_file_path,
                                 /*fields*/ {},
                                 get_metadata_future2.GetRepeatingCallback());
  EXPECT_EQ(get_metadata_future2.Get<base::File::Error>(),
            base::File::FILE_ERROR_NOT_FOUND);
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       NotFoundFromOpenFileEvictsCachedFile) {
  // Underlying FakeProvidedFileSystem is (always) initialised with fake file
  // with kFakeFilePath.
  const base::FilePath fake_file_path(kFakeFilePath);
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // The file won't be evicted after the successful `OpenFile` request.
  EXPECT_CALL(*mock_content_cache, Evict(fake_file_path)).Times(0);
  OpenFileFuture open_file_future1;
  cloud_file_system->OpenFile(fake_file_path, OPEN_FILE_MODE_READ,
                              open_file_future1.GetRepeatingCallback());
  EXPECT_EQ(open_file_future1.Get<base::File::Error>(), base::File::FILE_OK);

  // Remove the entry from the underlying FSP, this should result in a
  // base::File::FILE_ERROR_NOT_FOUND on the `OpenFile` request.
  DeleteEntryOnFakeFileSystem(fake_fsp, fake_file_path);

  // The file will be evicted after the unsuccessful `OpenFile` request.
  EXPECT_CALL(*mock_content_cache, Evict(fake_file_path)).Times(1);
  OpenFileFuture open_file_future2;
  cloud_file_system->OpenFile(fake_file_path, OPEN_FILE_MODE_READ,
                              open_file_future2.GetRepeatingCallback());
  EXPECT_EQ(open_file_future2.Get<base::File::Error>(),
            base::File::FILE_ERROR_NOT_FOUND);
}

TEST_F(FileSystemProviderCloudFileSystemTest, OkFromWriteFileEvictsCachedFile) {
  const base::FilePath fake_file_path(kFakeFilePath);
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // Open the `kFakeFilePath` file to stage it in the `FakeProvidedFileSystem`.
  int file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath));

  // The file will be evicted after the successful `WriteFile` request.
  EXPECT_CALL(*mock_content_cache, Evict(fake_file_path)).Times(1);
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);
  WriteFileSuccessfully(*cloud_file_system, file_handle, buffer);
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       OkFromDeleteEntryEvictsCachedFile) {
  const base::FilePath fake_file_path(kFakeFilePath);
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // The file will be evicted after the successful `WriteFile` request.
  EXPECT_CALL(*mock_content_cache, Evict(fake_file_path)).Times(1);
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);
  DeleteFileSuccessfully(*cloud_file_system, fake_file_path);
}

TEST_F(FileSystemProviderCloudFileSystemTest, CurrentReaderCanReadEvictedFile) {
  // Underlying FakeProvidedFileSystem is (always) initialised with fake file
  // with kFakeFilePath.
  const base::FilePath fake_file_path(kFakeFilePath);
  auto [mock_content_cache_observer, fake_fsp, cloud_file_system] =
      CreateContentCacheAndObserverAndCloudFileSystemWithFakeFsp();

  // Read 2 bytes of the `kFakeFilePath` file and insert them into the cache.
  int file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath));
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);

  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer, /*offset=*/0,
                       /*length=*/1);
  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer, /*offset=*/1,
                       /*length=*/1);
  CloseFileSuccessfully(*cloud_file_system, file_handle);

  // Open the file again.
  file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath));

  // Read the first byte from the cache.
  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer, /*offset=*/0,
                       /*length=*/1);

  // Remove the entry from the underlying FSP to ensure that the ReadFile is
  // indeed reading from the cache.
  DeleteEntryOnFakeFileSystem(fake_fsp, fake_file_path);

  // Send a delete notification. Expect that the item gets evicted.
  auto changes = std::make_unique<ProvidedFileSystemObserver::Changes>();
  changes->emplace_back(
      fake_file_path, storage::WatcherManager::ChangeType::DELETED,
      std::make_unique<ash::file_system_provider::CloudFileInfo>("versionA"));
  EXPECT_CALL(*mock_content_cache_observer, OnItemEvicted(fake_file_path));
  cloud_file_system->Notify(fake_file_path, /*recursive=*/true,
                            storage::WatcherManager::ChangeType::DELETED,
                            std::move(changes), /*tag=*/"", base::DoNothing());

  // Read the second byte from the cache.
  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer, /*offset=*/1,
                       /*length=*/1);

  // Fail to read the third byte. Expect that the request is forwarded to the
  // FSP which responds with a `base::File::FILE_ERROR_INVALID_OPERATION` due to
  // the file not existing.
  ReadFileFuture read_file_future;
  cloud_file_system->ReadFile(file_handle, buffer.get(), /*offset=*/2,
                              /*length=*/1,
                              read_file_future.GetRepeatingCallback());
  EXPECT_EQ(read_file_future.Get<int>(), 0);
  EXPECT_EQ(read_file_future.Get<base::File::Error>(),
            base::File::FILE_ERROR_INVALID_OPERATION);

  // Close the file, expect that it now gets removed.
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_content_cache_observer,
              OnItemRemovedFromDisk(fake_file_path, /*bytes_removed=*/2))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  CloseFileSuccessfully(*cloud_file_system, file_handle);
  run_loop.Run();
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       GetMetadataCallsObservedVersionTag) {
  // Underlying FakeProvidedFileSystem is (always) initialised with fake file
  // with kFakeFilePath.
  const base::FilePath fake_file_path(kFakeFilePath);
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // Expect the successful GetMetadata request for a CloudFileInfo triggers a
  // `ObservedVersionTag()` call.
  EXPECT_CALL(*mock_content_cache,
              ObservedVersionTag(fake_file_path, kFakeFileVersionTag))
      .Times(1);
  GetMetadataFuture get_metadata_future;
  cloud_file_system->GetMetadata(
      fake_file_path,
      /*fields*/ ProvidedFileSystemInterface::METADATA_FIELD_CLOUD_FILE_INFO,
      get_metadata_future.GetRepeatingCallback());
  EXPECT_EQ(get_metadata_future.Get<base::File::Error>(), base::File::FILE_OK);
}

TEST_F(FileSystemProviderCloudFileSystemTest, OpenFileCallsObservedVersionTag) {
  // Underlying FakeProvidedFileSystem is (always) initialised with fake file
  // with kFakeFilePath.
  const base::FilePath fake_file_path(kFakeFilePath);
  auto [mock_content_cache, fake_fsp, cloud_file_system] =
      CreateMockContentCacheAndCloudFileSystemWithFakeFsp();

  // Expect the successful OpenFile request triggers a `ObservedVersionTag()`
  // call.
  EXPECT_CALL(*mock_content_cache,
              ObservedVersionTag(fake_file_path, kFakeFileVersionTag))
      .Times(1);
  OpenFileFuture open_file_future1;
  cloud_file_system->OpenFile(fake_file_path, OPEN_FILE_MODE_READ,
                              open_file_future1.GetRepeatingCallback());
  EXPECT_EQ(open_file_future1.Get<base::File::Error>(), base::File::FILE_OK);
}

TEST_F(FileSystemProviderCloudFileSystemTest,
       OpenFileForNewVersionEvictsStaleCachedFile) {
  // Underlying FakeProvidedFileSystem is (always) initialised with fake file
  // with kFakeFilePath.
  const base::FilePath fake_file_path(kFakeFilePath);
  auto [mock_content_cache_observer, fake_fsp, cloud_file_system] =
      CreateContentCacheAndObserverAndCloudFileSystemWithFakeFsp();

  // Read 1 byte of the `kFakeFilePath` file and insert it into the cache.
  int file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath));
  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);

  ReadFileSuccessfully(*cloud_file_system, file_handle, buffer, /*offset=*/0,
                       /*length=*/1);
  CloseFileSuccessfully(*cloud_file_system, file_handle);

  // Update the version tag.
  UpdateEntryWithVersionTagOnFakeFileSystem(fake_fsp, fake_file_path,
                                            "new version");

  // Expect that opening the file causes the stale cache version to be evicted.
  EXPECT_CALL(*mock_content_cache_observer, OnItemEvicted(fake_file_path));
  file_handle = GetFileHandleFromSuccessfulOpenFile(
      *cloud_file_system, base::FilePath(kFakeFilePath));

  // Close the file, expect that it now gets removed.
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_content_cache_observer,
              OnItemRemovedFromDisk(fake_file_path, /*bytes_removed=*/1))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  CloseFileSuccessfully(*cloud_file_system, file_handle);
  run_loop.Run();
}

}  // namespace
}  // namespace ash::file_system_provider
