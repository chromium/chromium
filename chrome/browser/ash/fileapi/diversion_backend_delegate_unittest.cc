// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/diversion_backend_delegate.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/strings/strcat.h"
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

// A simple list of 0 or 1 FileSystemURLs that CHECK-fails when CheckAllowed is
// passed that FileSystemURL.
class DenyList {
 public:
  void CheckAllowed(const storage::FileSystemURL& fs_url) const {
    if (!disabled_ && fs_url_.is_valid()) {
      CHECK(fs_url_ != fs_url);
    }
  }

  void Deny(const storage::FileSystemURL& fs_url) {
    CHECK(fs_url.is_valid());
    CHECK(!fs_url_.is_valid());
    fs_url_ = fs_url;
  }

  void set_disabled(bool disabled) { disabled_ = disabled; }

 private:
  bool disabled_ = false;
  storage::FileSystemURL fs_url_;
};

// Wraps a LocalFileUtil (which implements the FileSystemFileUtil interface)
// with a DenyList. Unless disabled, it checks that various FileSystemFileUtil
// methods are not passed any FileSystemURLs on the deny list.
class DeniableFileUtil : public storage::LocalFileUtil {
 public:
  explicit DeniableFileUtil(const DenyList& deny_list)
      : deny_list_(deny_list) {}

  base::File::Error EnsureFileExists(
      storage::FileSystemOperationContext* context,
      const storage::FileSystemURL& url,
      bool* created) override {
    deny_list_->CheckAllowed(url);
    return storage::LocalFileUtil::EnsureFileExists(context, url, created);
  }

  base::File::Error GetFileInfo(storage::FileSystemOperationContext* context,
                                const storage::FileSystemURL& url,
                                base::File::Info* file_info,
                                base::FilePath* platform_file) override {
    deny_list_->CheckAllowed(url);
    return storage::LocalFileUtil::GetFileInfo(context, url, file_info,
                                               platform_file);
  }

  base::File::Error Truncate(storage::FileSystemOperationContext* context,
                             const storage::FileSystemURL& url,
                             int64_t length) override {
    deny_list_->CheckAllowed(url);
    return storage::LocalFileUtil::Truncate(context, url, length);
  }

  base::File::Error DeleteFile(storage::FileSystemOperationContext* context,
                               const storage::FileSystemURL& url) override {
    deny_list_->CheckAllowed(url);
    return storage::LocalFileUtil::DeleteFile(context, url);
  }

 private:
  raw_ref<const DenyList> deny_list_;
};

static int fake_fsb_delegate_create_file_stream_writer_count = 0;

class FakeFSBDelegate : public FileSystemBackendDelegate {
 public:
  explicit FakeFSBDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const DenyList& deny_list)
      : task_runner_(std::move(task_runner)),
        adapter_(std::make_unique<DeniableFileUtil>(deny_list)) {}

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
    NOTREACHED();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  storage::AsyncFileUtilAdapter adapter_;
};

}  // namespace

class DiversionBackendDelegateTest : public testing::Test,
                                     public testing::WithParamInterface<int> {
 public:
  DiversionBackendDelegateTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // The int returned by GetParam() ranges from 0 to (1 << kNumParamBits).
  static constexpr int kNumParamBits = 2;
  bool ShouldCopy() const { return (1 << 0) & GetParam(); }
  bool ShouldDestExists() const { return (1 << 1) & GetParam(); }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return base::StrCat({
        "Should",
        (((1 << 0) & info.param) ? "CopyAnd" : "MoveAnd"),
        (((1 << 1) & info.param) ? "DestExists" : "NotDestExists"),
    });
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

  void AssertEnsureFileExistsReturns(storage::AsyncFileUtil* async_file_util,
                                     const storage::FileSystemURL& fs_url,
                                     base::File::Error expected_error) {
    async_file_util->EnsureFileExists(
        CreateFSOContext(), fs_url,
        base::BindOnce(
            [](base::File::Error expected_error,
               base::RepeatingClosure quit_closure, base::File::Error error,
               bool created) {
              ASSERT_EQ(expected_error, error);
              ASSERT_TRUE(created);
              quit_closure.Run();
            },
            expected_error, task_environment_.QuitClosure()));
    task_environment_.RunUntilQuit();
  }

  void AssertGetFileInfoReturns(storage::AsyncFileUtil* async_file_util,
                                const storage::FileSystemURL& fs_url,
                                base::File::Error expected_error) {
    async_file_util->GetFileInfo(
        CreateFSOContext(), fs_url,
        {storage::FileSystemOperation::GetMetadataField::kIsDirectory},
        base::BindOnce(
            [](base::File::Error expected_error,
               base::RepeatingClosure quit_closure, base::File::Error error,
               const base::File::Info& file_info) {
              ASSERT_EQ(expected_error, error);
              quit_closure.Run();
            },
            expected_error, task_environment_.QuitClosure()));
    task_environment_.RunUntilQuit();
  }

  void RunBasic(DiversionBackendDelegate::Policy policy);

  static const char* kExpectedContents;
  static const char* kFragments[];

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir fs_context_temp_dir_;
  base::ScopedTempDir general_temp_dir_;
  scoped_refptr<storage::FileSystemContext> fs_context_;
};

const char* DiversionBackendDelegateTest::kExpectedContents =
    "Lorem ipsum.Dolor sit.Amet.";

const char* DiversionBackendDelegateTest::kFragments[] = {
    "Lorem ipsum.",
    "Dolor sit.",
    "Amet.",
};

void DiversionBackendDelegateTest::RunBasic(
    DiversionBackendDelegate::Policy policy) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  const bool should_divert =
      policy != DiversionBackendDelegate::Policy::kDoNotDivert;
  const bool should_isolate =
      policy == DiversionBackendDelegate::Policy::kDivertIsolated;

  const char* basename = "";
  switch (policy) {
    case DiversionBackendDelegate::Policy::kDoNotDivert:
      basename = "diversion.dat.some_other_extension";
      break;
    case DiversionBackendDelegate::Policy::kDivertIsolated:
      basename = "diversion.dat.crdownload";
      break;
    case DiversionBackendDelegate::Policy::kDivertMingled:
      basename = "diversion.dat.cros_divert_mingled_test";
      break;
  }
  storage::FileSystemURL fs_url0 = CreateFSURL(basename);

  DenyList deny_list;
  if (should_isolate) {
    deny_list.Deny(fs_url0);
  }

  fake_fsb_delegate_create_file_stream_writer_count = 0;
  DiversionBackendDelegate delegate(std::make_unique<FakeFSBDelegate>(
      task_environment_.GetMainThreadTaskRunner(), deny_list));

  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  delegate.OverrideTmpfileDirForTesting(temp_dir);

  // Simulate the "download a file" workflow that first writes to a temporary
  // file (fs_url0) before moving that over the ulimate destination (fs_url1).
  //
  // The final state should be the same, regardless of should_divert, in that
  // the fs_url0 file does not exist but the fs_url1 file does exist (and its
  // contents match the kExpectedContents).
  //
  // We also test copying (instead of moving) fs_url0 to fs_url1, depending on
  // ShouldCopy(). The "download a file" workflow always moves, but copying
  // should work too (where the final state is that both fs_url0 and fs_url1
  // files exist and have the kExpectedContents).
  storage::FileSystemURL fs_url1 = CreateFSURL("diversion.dat");
  ASSERT_EQ(policy, delegate.ShouldDivertForTesting(fs_url0));

  // The final state should be indifferent to whether or not fs_url1 already
  // exists and, if it does, whether it's longer than kExpectedContents.
  if (ShouldDestExists()) {
    ASSERT_TRUE(base::WriteFile(fs_url1.path(), std::string(100, 'x')));
  }

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
    AssertGetFileInfoReturns(async_file_util, fs_url0,
                             base::File::FILE_ERROR_NOT_FOUND);
    AssertEnsureFileExistsReturns(async_file_util, fs_url0,
                                  base::File::FILE_OK);
    AssertGetFileInfoReturns(async_file_util, fs_url0, base::File::FILE_OK);
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
  // depending on should_divert;
  {
    int64_t offset = 0;
    for (const char* fragment : kFragments) {
      std::unique_ptr<storage::FileStreamWriter> writer =
          delegate.CreateFileStreamWriter(fs_url0, offset, fs_context_.get());
      SynchronousWrite(*writer, fragment);
      offset += strlen(fragment);
    }
    EXPECT_EQ(should_divert ? 0 : 3,
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
    EXPECT_EQ(kExpectedContents, read_from_reader_contents.value());
  }

  // Even though, in our unit test, our DiversionBackendDelegate is ultimately
  // backed by a local file system, whether or not fs_url0 exists on that local
  // file system depends on whether the DiversionBackendDelegate diverted
  // (instead of passing through) that FileSystemURL.
  {
    EXPECT_NE(should_divert, base::PathExists(fs_url0.path()));
    EXPECT_EQ(ShouldDestExists(), base::PathExists(fs_url1.path()));
  }

  // Copying or moving that fs_url0 file to fs_url1 should materialize it (if
  // diverted) on the DiversionBackendDelegate's wrappee backend.
  //
  // Moving materializes fs_url1. Copying materializes both fs_url0 and fs_url1
  // and materializing fs_url0 (which is on the deny_list, if should_isolate)
  // requires temporarily disabling that deny_list.
  if (ShouldCopy()) {
    deny_list.set_disabled(true);
    async_file_util->CopyFileLocal(
        CreateFSOContext(), fs_url0, fs_url1, {},
        base::BindRepeating(
            [](int64_t size_arg_for_copy_file_progress_callback) {
              // No-op.
            }),
        base::BindOnce(
            [](base::RepeatingClosure quit_closure, base::File::Error error) {
              ASSERT_EQ(base::File::FILE_OK, error);
              quit_closure.Run();
            },
            task_environment_.QuitClosure()));
    task_environment_.RunUntilQuit();
    deny_list.set_disabled(false);
  } else {
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
    EXPECT_EQ(ShouldCopy(), base::PathExists(fs_url0.path()));
    EXPECT_TRUE(base::PathExists(fs_url1.path()));

    if (ShouldCopy()) {
      std::string contents0;
      ASSERT_TRUE(base::ReadFileToString(fs_url0.path(), &contents0));
      EXPECT_EQ(kExpectedContents, contents0);
    }

    std::string contents1;
    ASSERT_TRUE(base::ReadFileToString(fs_url1.path(), &contents1));
    EXPECT_EQ(kExpectedContents, contents1);
  }
}

TEST_P(DiversionBackendDelegateTest, BasicDoNotDivert) {
  RunBasic(DiversionBackendDelegate::Policy::kDoNotDivert);
}

TEST_P(DiversionBackendDelegateTest, BasicDivertIsolated) {
  RunBasic(DiversionBackendDelegate::Policy::kDivertIsolated);
}

TEST_P(DiversionBackendDelegateTest, BasicDivertMingled) {
  RunBasic(DiversionBackendDelegate::Policy::kDivertMingled);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DiversionBackendDelegateTest,
    testing::Range(0, 1 << DiversionBackendDelegateTest::kNumParamBits),
    &DiversionBackendDelegateTest::DescribeParams);

TEST_F(DiversionBackendDelegateTest, Timeout) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  DenyList deny_list;
  fake_fsb_delegate_create_file_stream_writer_count = 0;
  DiversionBackendDelegate delegate(std::make_unique<FakeFSBDelegate>(
      task_environment_.GetMainThreadTaskRunner(), deny_list));

  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  delegate.OverrideTmpfileDirForTesting(temp_dir);

  storage::FileSystemURL fs_url0 = CreateFSURL("diversion.dat.crdownload");
  ASSERT_EQ(DiversionBackendDelegate::Policy::kDivertIsolated,
            delegate.ShouldDivertForTesting(fs_url0));

  storage::AsyncFileUtil* async_file_util =
      delegate.GetAsyncFileUtil(fs_url0.type());
  {
    AssertGetFileInfoReturns(async_file_util, fs_url0,
                             base::File::FILE_ERROR_NOT_FOUND);
    AssertEnsureFileExistsReturns(async_file_util, fs_url0,
                                  base::File::FILE_OK);
    AssertGetFileInfoReturns(async_file_util, fs_url0, base::File::FILE_OK);
  }

  {
    int64_t offset = 0;
    for (const char* fragment : kFragments) {
      std::unique_ptr<storage::FileStreamWriter> writer =
          delegate.CreateFileStreamWriter(fs_url0, offset, fs_context_.get());
      SynchronousWrite(*writer, fragment);
      offset += strlen(fragment);
    }
  }

  // The code above is similar to the DiversionBackendDelegateTest.Basic test,
  // although it stops before the CopyFileLocal or MoveFileLocal call and there
  // is no fs_url1 variable, only fs_url0. The code below is different.

  {
    ASSERT_FALSE(base::PathExists(fs_url0.path()));
    task_environment_.FastForwardBy(delegate.IdleTimeoutForTesting());
    ASSERT_TRUE(base::PathExists(fs_url0.path()));

    std::string contents0;
    ASSERT_TRUE(base::ReadFileToString(fs_url0.path(), &contents0));
    EXPECT_EQ(kExpectedContents, contents0);
  }
}

}  // namespace ash
