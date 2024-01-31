// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/copy_from_fd.h"

#include <unistd.h>

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class FakeFileStreamWriter : public storage::FileStreamWriter {
 public:
  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback) override {
    CHECK(::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    data_.append(buf->data(), buf->size());
    return buf->size();
  }

  int Cancel(net::CompletionOnceCallback callback) override {
    CHECK(::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    return net::ERR_NOT_IMPLEMENTED;
  }

  int Flush(storage::FlushMode flush_mode,
            net::CompletionOnceCallback callback) override {
    CHECK(::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    flushed_ = true;
    return net::OK;
  }

  std::string data_;
  bool flushed_ = false;
};

}  // namespace

TEST(CopyFromFileDescriptorTest, Basic) {
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath temp_file_path;
  base::ScopedFD scoped_fd = base::CreateAndOpenFdForTemporaryFileInDir(
      temp_dir.GetPath(), &temp_file_path);
  ASSERT_TRUE(scoped_fd.is_valid());
  ASSERT_EQ(write(scoped_fd.get(), "abcdefghij", 10), 10);
  ASSERT_EQ(lseek(scoped_fd.get(), 3, SEEK_SET), 3);

  auto fs_writer = std::make_unique<FakeFileStreamWriter>();

  auto callback = base::BindOnce(
      [](int expected_fd, FakeFileStreamWriter* expected_fs_writer,
         base::RepeatingClosure quit_closure, base::ScopedFD ret_scoped_fd,
         std::unique_ptr<storage::FileStreamWriter> ret_fs_writer,
         net::Error ret_error) {
        ASSERT_TRUE(
            ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
        ASSERT_EQ(ret_scoped_fd.get(), expected_fd);
        ASSERT_EQ(ret_fs_writer.get(), expected_fs_writer);
        EXPECT_EQ(ret_error, net::OK);
        EXPECT_EQ(expected_fs_writer->data_.size(), 10u - 3u);
        EXPECT_EQ(expected_fs_writer->data_, "defghij");
        EXPECT_TRUE(expected_fs_writer->flushed_);
        quit_closure.Run();
      },
      scoped_fd.get(), fs_writer.get(), task_environment.QuitClosure());

  CopyFromFileDescriptor(std::move(scoped_fd), std::move(fs_writer),
                         storage::FlushPolicy::FLUSH_ON_COMPLETION,
                         std::move(callback));
  task_environment.RunUntilQuit();
}

}  // namespace ash
