// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/files/file_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class FileProxyTest : public testing::Test {
 public:
  FileProxyTest()
      : task_environment_(test::TaskEnvironment::MainThreadType::IO),
        file_thread_("FileProxyTestFileThread"),
        error_(File::FILE_OK),
        bytes_written_(-1) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_thread_.Start());
  }

  void DidFinish(base::RepeatingClosure continuation, File::Error error) {
    error_ = error;
    continuation.Run();
  }

  void DidCreateOrOpen(base::RepeatingClosure continuation, File::Error error) {
    error_ = error;
    continuation.Run();
  }

  void DidCreateTemporary(base::RepeatingClosure continuation,
                          File::Error error,
                          const FilePath& path) {
    error_ = error;
    path_ = path;
    continuation.Run();
  }

  void DidGetFileInfo(base::RepeatingClosure continuation,
                      File::Error error,
                      const File::Info& file_info) {
    error_ = error;
    file_info_ = file_info;
    continuation.Run();
  }

  void DidRead(base::RepeatingClosure continuation,
               File::Error error,
               base::span<const char> data) {
    error_ = error;
    buffer_ = base::HeapArray<char>::CopiedFrom(data);
    continuation.Run();
  }

  void DidWrite(base::RepeatingClosure continuation,
                File::Error error,
                int bytes_written) {
    error_ = error;
    bytes_written_ = bytes_written;
    continuation.Run();
  }

 protected:
  void CreateProxy(uint32_t flags, FileProxy* proxy) {
    RunLoop run_loop;
    proxy->CreateOrOpen(
        TestPath(), flags,
        BindOnce(&FileProxyTest::DidCreateOrOpen, weak_factory_.GetWeakPtr(),
                 run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
    EXPECT_TRUE(proxy->IsValid());
  }

  TaskRunner* file_task_runner() const {
    return file_thread_.task_runner().get();
  }
  const FilePath& TestDirPath() const { return dir_.GetPath(); }
  const FilePath TestPath() const { return dir_.GetPath().AppendASCII("test"); }

  ScopedTempDir dir_;
  test::TaskEnvironment task_environment_;
  Thread file_thread_;

  File::Error error_;
  FilePath path_;
  File::Info file_info_;
  base::HeapArray<char> buffer_;
  int bytes_written_;
  WeakPtrFactory<FileProxyTest> weak_factory_{this};
};

TEST_F(FileProxyTest, CreateOrOpen_Create) {
  FileProxy proxy(file_task_runner());
  RunLoop run_loop;
  proxy.CreateOrOpen(
      TestPath(), File::FLAG_CREATE | File::FLAG_READ,
      BindOnce(&FileProxyTest::DidCreateOrOpen, weak_factory_.GetWeakPtr(),
               run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  EXPECT_EQ(File::FILE_OK, error_);
  EXPECT_TRUE(proxy.IsValid());
  EXPECT_TRUE(proxy.created());
  EXPECT_TRUE(PathExists(TestPath()));
}

TEST_F(FileProxyTest, CreateOrOpen_Open) {
  // Creates a file.
  base::WriteFile(TestPath(), std::string_view());
  ASSERT_TRUE(PathExists(TestPath()));

  // Opens the created file.
  FileProxy proxy(file_task_runner());
  RunLoop run_loop;
  proxy.CreateOrOpen(
      TestPath(), File::FLAG_OPEN | File::FLAG_READ,
      BindOnce(&FileProxyTest::DidCreateOrOpen, weak_factory_.GetWeakPtr(),
               run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  EXPECT_EQ(File::FILE_OK, error_);
  EXPECT_TRUE(proxy.IsValid());
  EXPECT_FALSE(proxy.created());
}

TEST_F(FileProxyTest, CreateOrOpen_OpenNonExistent) {
  FileProxy proxy(file_task_runner());
  RunLoop run_loop;
  proxy.CreateOrOpen(
      TestPath(), File::FLAG_OPEN | File::FLAG_READ,
      BindOnce(&FileProxyTest::DidCreateOrOpen, weak_factory_.GetWeakPtr(),
               run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
  EXPECT_EQ(File::FILE_ERROR_NOT_FOUND, error_);
  EXPECT_FALSE(proxy.IsValid());
  EXPECT_FALSE(proxy.created());
  EXPECT_FALSE(PathExists(TestPath()));
}

TEST_F(FileProxyTest, CreateOrOpen_AbandonedCreate) {
  {
    base::ScopedDisallowBlocking disallow_blocking;
    RunLoop run_loop;
    {
      FileProxy proxy(file_task_runner());
      proxy.CreateOrOpen(
          TestPath(), File::FLAG_CREATE | File::FLAG_READ,
          BindOnce(&FileProxyTest::DidCreateOrOpen, weak_factory_.GetWeakPtr(),
                   run_loop.QuitWhenIdleClosure()));
    }
    run_loop.Run();
  }

  EXPECT_TRUE(PathExists(TestPath()));
}

TEST_F(FileProxyTest, Close) {
  // Creates a file.
  FileProxy proxy(file_task_runner());
  CreateProxy(File::FLAG_CREATE | File::FLAG_WRITE, &proxy);

#if BUILDFLAG(IS_WIN)
  // This fails on Windows if the file is not closed.
  EXPECT_FALSE(base::Move(TestPath(), TestDirPath().AppendASCII("new")));
#endif

  RunLoop run_loop;
  proxy.Close(BindOnce(&FileProxyTest::DidFinish, weak_factory_.GetWeakPtr(),
                       run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
  EXPECT_EQ(File::FILE_OK, error_);
  EXPECT_FALSE(proxy.IsValid());

  // Now it should pass on all platforms.
  EXPECT_TRUE(base::Move(TestPath(), TestDirPath().AppendASCII("new")));
}

TEST_F(FileProxyTest, CreateTemporary) {
  {
    FileProxy proxy(file_task_runner());
    {
      RunLoop run_loop;
      proxy.CreateTemporary(
          0 /* additional_file_flags */,
          BindOnce(&FileProxyTest::DidCreateTemporary,
                   weak_factory_.GetWeakPtr(), run_loop.QuitWhenIdleClosure()));
      run_loop.Run();
    }

    EXPECT_TRUE(proxy.IsValid());
    EXPECT_EQ(File::FILE_OK, error_);
    EXPECT_TRUE(PathExists(path_));

    // The file should be writable.
    {
      RunLoop run_loop;
      proxy.Write(0, base::as_byte_span(std::string_view("test")),
                  BindOnce(&FileProxyTest::DidWrite, weak_factory_.GetWeakPtr(),
                           run_loop.QuitWhenIdleClosure()));
      run_loop.Run();
    }
    EXPECT_EQ(File::FILE_OK, error_);
    EXPECT_EQ(4, bytes_written_);
  }

  // Make sure the written data can be read from the returned path.
  std::string data;
  EXPECT_TRUE(ReadFileToString(path_, &data));
  EXPECT_EQ("test", data);

  // Make sure we can & do delete the created file to prevent leaks on the bots.
  // Try a few times because files may be locked by anti-virus or other.
  bool deleted_temp_file = false;
  for (int i = 0; !deleted_temp_file && i < 3; ++i) {
    if (base::DeleteFile(path_))
      deleted_temp_file = true;
    else
      // Wait one second and then try again
      PlatformThread::Sleep(Seconds(1));
  }
  EXPECT_TRUE(deleted_temp_file);
}

TEST_F(FileProxyTest, SetAndTake) {
  File file(TestPath(), File::FLAG_CREATE | File::FLAG_READ);
  ASSERT_TRUE(file.IsValid());
  FileProxy proxy(file_task_runner());
  EXPECT_FALSE(proxy.IsValid());
  proxy.SetFile(std::move(file));
  EXPECT_TRUE(proxy.IsValid());
  EXPECT_FALSE(file.IsValid());

  file = proxy.TakeFile();
  EXPECT_FALSE(proxy.IsValid());
  EXPECT_TRUE(file.IsValid());
}

TEST_F(FileProxyTest, DuplicateFile) {
  FileProxy proxy(file_task_runner());
  CreateProxy(File::FLAG_CREATE | File::FLAG_WRITE, &proxy);
  ASSERT_TRUE(proxy.IsValid());

  base::File duplicate = proxy.DuplicateFile();
  EXPECT_TRUE(proxy.IsValid());
  EXPECT_TRUE(duplicate.IsValid());

  FileProxy invalid_proxy(file_task_runner());
  ASSERT_FALSE(invalid_proxy.IsValid());

  base::File invalid_duplicate = invalid_proxy.DuplicateFile();
  EXPECT_FALSE(invalid_proxy.IsValid());
  EXPECT_FALSE(invalid_duplicate.IsValid());
}

TEST_F(FileProxyTest, GetInfo) {
  // Setup.
  ASSERT_TRUE(base::WriteFile(TestPath(), "test"));
  File::Info expected_info;
  GetFileInfo(TestPath(), &expected_info);

  // Run.
  FileProxy proxy(file_task_runner());
  CreateProxy(File::FLAG_OPEN | File::FLAG_READ, &proxy);
  RunLoop run_loop;
  proxy.GetInfo(BindOnce(&FileProxyTest::DidGetFileInfo,
                         weak_factory_.GetWeakPtr(),
                         run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  // Verify.
  EXPECT_EQ(File::FILE_OK, error_);
  EXPECT_EQ(expected_info.size, file_info_.size);
  EXPECT_EQ(expected_info.is_directory, file_info_.is_directory);
  EXPECT_EQ(expected_info.is_symbolic_link, file_info_.is_symbolic_link);
  EXPECT_EQ(expected_info.last_modified, file_info_.last_modified);
  EXPECT_EQ(expected_info.creation_time, file_info_.creation_time);
}

TEST_F(FileProxyTest, Read) {
  // Setup.
  constexpr std::string_view expected_data = "bleh";
  ASSERT_TRUE(base::WriteFile(TestPath(), expected_data));

  // Run.
  FileProxy proxy(file_task_runner());
  CreateProxy(File::FLAG_OPEN | File::FLAG_READ, &proxy);

  RunLoop run_loop;
  proxy.Read(0, 128,
             BindOnce(&FileProxyTest::DidRead, weak_factory_.GetWeakPtr(),
                      run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  // Verify.
  EXPECT_EQ(File::FILE_OK, error_);
  EXPECT_EQ(expected_data, std::string_view(buffer_.data(), buffer_.size()));
}

TEST_F(FileProxyTest, WriteAndFlush) {
  FileProxy proxy(file_task_runner());
  CreateProxy(File::FLAG_CREATE | File::FLAG_WRITE, &proxy);

  auto write_span = base::as_byte_span("foo!");
  EXPECT_EQ(write_span.size(), 5u);  // Includes the NUL, too.
  {
    RunLoop run_loop;
    proxy.Write(0, write_span,
                BindOnce(&FileProxyTest::DidWrite, weak_factory_.GetWeakPtr(),
                         run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(File::FILE_OK, error_);
  EXPECT_EQ(write_span.size(), static_cast<size_t>(bytes_written_));

  // Flush the written data.  (So that the following read should always
  // succeed.  On some platforms it may work with or without this flush.)
  {
    RunLoop run_loop;
    proxy.Flush(BindOnce(&FileProxyTest::DidFinish, weak_factory_.GetWeakPtr(),
                         run_loop.QuitWhenIdleClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(File::FILE_OK, error_);

  // Verify the written data.
  char read_buffer[10];
  EXPECT_GE(std::size(read_buffer), write_span.size());
  EXPECT_EQ(write_span.size(), base::ReadFile(TestPath(), read_buffer));
  for (size_t i = 0; i < write_span.size(); ++i) {
    EXPECT_EQ(write_span[i], read_buffer[i]);
  }
}

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android, see http://crbug.com/489602
#define MAYBE_SetTimes DISABLED_SetTimes
#else
#define MAYBE_SetTimes SetTimes
#endif
TEST_F(FileProxyTest, MAYBE_SetTimes) {
  FileProxy proxy(file_task_runner());
  CreateProxy(
      File::FLAG_CREATE | File::FLAG_WRITE | File::FLAG_WRITE_ATTRIBUTES,
      &proxy);

  Time last_accessed_time = Time::Now() - Days(12345);
  Time last_modified_time = Time::Now() - Hours(98765);

  RunLoop run_loop;
  proxy.SetTimes(last_accessed_time, last_modified_time,
                 BindOnce(&FileProxyTest::DidFinish, weak_factory_.GetWeakPtr(),
                          run_loop.QuitWhenIdleClosure()));
  run_loop.Run();
  EXPECT_EQ(File::FILE_OK, error_);

  File::Info info;
  GetFileInfo(TestPath(), &info);

  // The returned values may only have the seconds precision, so we cast
  // the double values to int here.
  EXPECT_EQ(static_cast<int>(last_modified_time.InSecondsFSinceUnixEpoch()),
            static_cast<int>(info.last_modified.InSecondsFSinceUnixEpoch()));

#if !BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia, /tmp is noatime
  EXPECT_EQ(static_cast<int>(last_accessed_time.InSecondsFSinceUnixEpoch()),
            static_cast<int>(info.last_accessed.InSecondsFSinceUnixEpoch()));
#endif  // BUILDFLAG(IS_FUCHSIA)
}

TEST_F(FileProxyTest, SetLength_Shrink) {
  // Setup.
  const char kTestData[] = "0123456789";
  ASSERT_TRUE(base::WriteFile(TestPath(), kTestData));
  File::Info info;
  GetFileInfo(TestPath(), &info);
  ASSERT_EQ(10, info.size);

  // Run.
  FileProxy proxy(file_task_runner());
  CreateProxy(File::FLAG_OPEN | File::FLAG_WRITE, &proxy);
  RunLoop run_loop;
  proxy.SetLength(
      7, BindOnce(&FileProxyTest::DidFinish, weak_factory_.GetWeakPtr(),
                  run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  // Verify.
  GetFileInfo(TestPath(), &info);
  ASSERT_EQ(7, info.size);

  char buffer[7];
  EXPECT_EQ(7, base::ReadFile(TestPath(), buffer));
  int i = 0;
  for (; i < 7; ++i)
    EXPECT_EQ(kTestData[i], buffer[i]);
}

TEST_F(FileProxyTest, SetLength_Expand) {
  // Setup.
  const char kTestData[] = "9876543210";
  ASSERT_TRUE(base::WriteFile(TestPath(), kTestData));
  File::Info info;
  GetFileInfo(TestPath(), &info);
  ASSERT_EQ(10, info.size);

  // Run.
  FileProxy proxy(file_task_runner());
  CreateProxy(File::FLAG_OPEN | File::FLAG_WRITE, &proxy);
  RunLoop run_loop;
  proxy.SetLength(
      53, BindOnce(&FileProxyTest::DidFinish, weak_factory_.GetWeakPtr(),
                   run_loop.QuitWhenIdleClosure()));
  run_loop.Run();

  // Verify.
  GetFileInfo(TestPath(), &info);
  ASSERT_EQ(53, info.size);

  char buffer[53];
  EXPECT_EQ(53, base::ReadFile(TestPath(), buffer));
  int i = 0;
  for (; i < 10; ++i)
    EXPECT_EQ(kTestData[i], buffer[i]);
  for (; i < 53; ++i)
    EXPECT_EQ(0, buffer[i]);
}

}  // namespace base
