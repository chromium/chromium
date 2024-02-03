// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/diversion_backend_delegate.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "storage/browser/file_system/async_file_util_adapter.h"
#include "storage/browser/file_system/local_file_util.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

base::expected<std::string, net::Error> ReadFromReader(
    storage::FileStreamReader& reader,
    size_t bytes_to_read) {
  std::string result;
  size_t total_bytes_read = 0;
  while (total_bytes_read < bytes_to_read) {
    scoped_refptr<net::IOBufferWithSize> buf(
        base::MakeRefCounted<net::IOBufferWithSize>(bytes_to_read -
                                                    total_bytes_read));
    net::TestCompletionCallback callback;
    int rv = reader.Read(buf.get(), buf->size(), callback.callback());
    if (rv == net::ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    if (rv < 0) {
      return base::unexpected(static_cast<net::Error>(rv));
    } else if (rv == 0) {
      break;
    }
    total_bytes_read += rv;
    result.append(buf->data(), rv);
  }
  return result;
}

static int fake_fsb_delegate_create_file_stream_writer_count = 0;

class FakeFSBDelegate : public FileSystemBackendDelegate {
 public:
  explicit FakeFSBDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)),
        adapter_(std::make_unique<storage::LocalFileUtil>()) {}

  FakeFSBDelegate(const FakeFSBDelegate&) = delete;
  FakeFSBDelegate& operator=(const FakeFSBDelegate&) = delete;

  storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) override {
    CHECK_EQ(storage::kFileSystemTypeLocal, type);
    return &adapter_;
  }

  std::unique_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) override {
    CHECK_EQ(storage::kFileSystemTypeLocal, url.type());
    return storage::FileStreamReader::CreateForLocalFile(
        task_runner_.get(), url.path(), offset, expected_modification_time);
  }

  std::unique_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64_t offset,
      storage::FileSystemContext* context) override {
    CHECK_EQ(storage::kFileSystemTypeLocal, url.type());
    fake_fsb_delegate_create_file_stream_writer_count++;
    return storage::FileStreamWriter::CreateForLocalFile(
        task_runner_.get(), url.path(), offset,
        storage::FileStreamWriter::OPEN_EXISTING_FILE);
  }

  storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) override {
    NOTREACHED_NORETURN();
  }

  void GetRedirectURLForContents(const storage::FileSystemURL& url,
                                 storage::URLCallback callback) override {
    NOTREACHED_NORETURN();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  storage::AsyncFileUtilAdapter adapter_;
};

}  // namespace

class DiversionBackendDelegateTest : public testing::Test,
                                     public testing::WithParamInterface<bool> {
 public:
  DiversionBackendDelegateTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  bool ShouldDivert() const { return GetParam(); }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "ShouldDivert" : "ShouldNotDivert";
  }

 protected:
  void SetUp() override {
    ASSERT_TRUE(fs_context_temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(general_temp_dir_.CreateUniqueTempDir());

    static constexpr bool is_incognito = false;
    scoped_refptr<storage::MockQuotaManager> quota_manager =
        base::MakeRefCounted<storage::MockQuotaManager>(
            is_incognito, fs_context_temp_dir_.GetPath(),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>());

    scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy =
        base::MakeRefCounted<storage::MockQuotaManagerProxy>(
            quota_manager.get(),
            base::SingleThreadTaskRunner::GetCurrentDefault());

    fs_context_ = CreateFileSystemContextForTesting(
        quota_manager_proxy, fs_context_temp_dir_.GetPath());
  }

  void SynchronousWrite(storage::FileStreamWriter& writer, std::string s) {
    scoped_refptr<net::StringIOBuffer> buffer =
        base::MakeRefCounted<net::StringIOBuffer>(s);
    writer.Write(
        buffer.get(), buffer->size(),
        base::BindOnce([](base::RepeatingClosure quit_closure,
                          int byte_count_or_error_code) { quit_closure.Run(); },
                       task_environment_.QuitClosure()));
    task_environment_.RunUntilQuit();
  }

  std::unique_ptr<storage::FileSystemOperationContext> CreateFSOContext() {
    return std::make_unique<storage::FileSystemOperationContext>(
        fs_context_.get());
  }

  storage::FileSystemURL CreateFSURL(const char* basename) {
    return storage::FileSystemURL::CreateForTest(
        blink::StorageKey::CreateFromStringForTesting("chrome-extension://xxx"),
        storage::kFileSystemTypeExternal, base::FilePath(basename),
        "fake_mount_filesystem_id", storage::kFileSystemTypeLocal,
        general_temp_dir_.GetPath().Append(base::FilePath(basename)),
        "fake_filesystem_id", storage::FileSystemMountOption());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir fs_context_temp_dir_;
  base::ScopedTempDir general_temp_dir_;
  scoped_refptr<storage::FileSystemContext> fs_context_;
};

TEST_P(DiversionBackendDelegateTest, Basic) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  fake_fsb_delegate_create_file_stream_writer_count = 0;
  DiversionBackendDelegate delegate(std::make_unique<FakeFSBDelegate>(
      task_environment_.GetMainThreadTaskRunner()));

  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  delegate.OverrideTmpfileDirForTesting(temp_dir);

  const char* expected_contents = "Lorem ipsum.Dolor sit.Amet.";
  const char* fragments[] = {"Lorem ipsum.", "Dolor sit.", "Amet."};

  // Simulate the "download a file" workflow that first writes to a temporary
  // file (fs_url0) before moving that over the ulimate destination (fs_url1).
  //
  // The final state should be the same, regardless of ShouldDivert(), in that
  // the fs_url0 file does not exist but the fs_url1 file does exist (and its
  // contents match the expected_contents).
  storage::FileSystemURL fs_url0 =
      CreateFSURL(ShouldDivert() ? "diversion.dat.crdownload"
                                 : "diversion.dat.some_other_extension");
  storage::FileSystemURL fs_url1 = CreateFSURL("diversion.dat");
  ASSERT_EQ(ShouldDivert(), delegate.ShouldDivertForTesting(fs_url0));

  // The storage backends are generally happier, when calling
  // CreateFileStreamWriter, if the file already 'exists'. This doesn't
  // necessarily mean existence from the kernel's point of view, just from the
  // //storage/browser/file_system virtual file system's point of view.
  //
  // We therefore call EnsureFileExists, here, before CreateFileStreamWriter,
  // further below.
  storage::AsyncFileUtil* async_file_util =
      delegate.GetAsyncFileUtil(fs_url0.type());
  {
    async_file_util->EnsureFileExists(
        CreateFSOContext(), fs_url0,
        base::BindOnce(
            [](base::RepeatingClosure quit_closure, base::File::Error error,
               bool created) {
              ASSERT_EQ(base::File::FILE_OK, error);
              ASSERT_TRUE(created);
              quit_closure.Run();
            },
            task_environment_.QuitClosure()));
    task_environment_.RunUntilQuit();
  }

  // Make multiple incremental writes, from multiple FileStreamWriter objects,
  // similar to what happens when web apps use the FSA (File System Access)
  // JavaScript API. In this unit test, the DiversionBackendDelegate is
  // ultimately backed by a local file system. In production, though, FSA calls
  // connected to the ODFS (One Drive File System) FSP (File System Provider)
  // backend can be problematic (quadratic complexity in performance), when
  // using multiple FileStreamWriter objects, because of ODFS's "each
  // FileStreamWriter corresponds to a complete, one-shot upload" model.
  //
  // The whole point of a DiversionBackendDelegate is to buffer FSA's multiple
  // FileStreamWriters so that ODFS only sees a single FileStreamWriter. The
  // number of FakeFSBDelegate FileStreamWriter's created should be 0 or 3
  // depending on ShouldDivert().
  {
    int64_t offset = 0;
    for (const char* fragment : fragments) {
      std::unique_ptr<storage::FileStreamWriter> writer =
          delegate.CreateFileStreamWriter(fs_url0, offset, fs_context_.get());
      SynchronousWrite(*writer, fragment);
      offset += strlen(fragment);
    }
    EXPECT_EQ(ShouldDivert() ? 0 : 3,
              fake_fsb_delegate_create_file_stream_writer_count);
  }

  // We have just written to fs_url0, so we should be able to read the contents
  // back immediately (e.g. to compute a hash value for malware detection).
  {
    std::unique_ptr<storage::FileStreamReader> reader =
        delegate.CreateFileStreamReader(fs_url0, 0,
                                        std::numeric_limits<int64_t>::max(),
                                        base::Time(), fs_context_.get());
    base::expected<std::string, net::Error> read_from_reader_contents =
        ReadFromReader(*reader, 100);
    reader.reset();
    ASSERT_TRUE(read_from_reader_contents.has_value());
    EXPECT_EQ(expected_contents, read_from_reader_contents.value());
  }

  // Even though, in our unit test, our DiversionBackendDelegate is ultimately
  // backed by a local file system, whether or not fs_url0 exists on that local
  // file system depends on whether the DiversionBackendDelegate diverted
  // (instead of passing through) that FileSystemURL.
  {
    EXPECT_NE(ShouldDivert(), base::PathExists(fs_url0.path()));
    EXPECT_FALSE(base::PathExists(fs_url1.path()));
  }

  // Moving (renaming) that fs_url0 file to fs_url1 should materialize it (if
  // diverted) on the DiversionBackendDelegate's wrappee backend.
  {
    async_file_util->MoveFileLocal(
        CreateFSOContext(), fs_url0, fs_url1, {},
        base::BindOnce(
            [](base::RepeatingClosure quit_closure, base::File::Error error) {
              ASSERT_EQ(base::File::FILE_OK, error);
              quit_closure.Run();
            },
            task_environment_.QuitClosure()));
    task_environment_.RunUntilQuit();
  }

  // Check the final state, per "The final state should be the same" above.
  {
    EXPECT_FALSE(base::PathExists(fs_url0.path()));
    EXPECT_TRUE(base::PathExists(fs_url1.path()));

    std::string read_file_to_string_contents;
    ASSERT_TRUE(
        base::ReadFileToString(fs_url1.path(), &read_file_to_string_contents));
    EXPECT_EQ(expected_contents, read_file_to_string_contents);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         DiversionBackendDelegateTest,
                         testing::Bool(),
                         &DiversionBackendDelegateTest::DescribeParams);

}  // namespace ash
