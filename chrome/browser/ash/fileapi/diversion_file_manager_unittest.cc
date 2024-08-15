// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/diversion_file_manager.h"

#include "base/files/file_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using FinishDivertingResult = DiversionFileManager::FinishDivertingResult;
using StartDivertingResult = DiversionFileManager::StartDivertingResult;
using StoppedReason = DiversionFileManager::StoppedReason;

namespace {

DiversionFileManager::Callback IncrementCounterCallback(
    StoppedReason expected_stopped_reason,
    const storage::FileSystemURL& expected_url,
    int* counter,
    int delta) {
  return base::BindOnce(
      [](StoppedReason expected_stopped_reason,
         const storage::FileSystemURL& expected_url, int* counter, int delta,
         StoppedReason stopped_reason, const storage::FileSystemURL& url,
         base::ScopedFD scoped_fd, int64_t file_size, base::File::Error error) {
        EXPECT_EQ(expected_stopped_reason, stopped_reason);
        EXPECT_EQ(expected_url, url);
        *counter += delta;
      },
      expected_stopped_reason, expected_url, counter, delta);
}

}  // namespace

class DiversionFileManagerTest : public testing::Test {
 public:
  DiversionFileManagerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
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

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DiversionFileManagerTest, ImplicitExplicitFinish) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  scoped_refptr<DiversionFileManager> dfm =
      base::MakeRefCounted<DiversionFileManager>();
  storage::FileSystemURL bar_url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/p/q/bar"));
  storage::FileSystemURL foo_url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/p/q/foo"));
  EXPECT_FALSE(dfm->IsDiverting(bar_url));
  EXPECT_FALSE(dfm->IsDiverting(foo_url));

  int bar_counter = 0;
  int foo_counter = 0;

  ASSERT_EQ(
      StartDivertingResult::kOK,
      dfm->StartDiverting(bar_url, base::Seconds(28),
                          IncrementCounterCallback(StoppedReason::kImplicitIdle,
                                                   bar_url, &bar_counter, 1)));
  ASSERT_EQ(
      StartDivertingResult::kOK,
      dfm->StartDiverting(foo_url, base::Seconds(32),
                          IncrementCounterCallback(StoppedReason::kImplicitIdle,
                                                   foo_url, &foo_counter, 10)));
  EXPECT_TRUE(dfm->IsDiverting(bar_url));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(dfm->IsDiverting(bar_url));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  ASSERT_EQ(FinishDivertingResult::kWasNotDiverting,
            dfm->FinishDiverting(bar_url, IncrementCounterCallback(
                                              StoppedReason::kExplicitFinish,
                                              bar_url, &bar_counter, 100)));
  ASSERT_EQ(FinishDivertingResult::kOK,
            dfm->FinishDiverting(foo_url, IncrementCounterCallback(
                                              StoppedReason::kExplicitFinish,
                                              foo_url, &foo_counter, 1000)));
  EXPECT_FALSE(dfm->IsDiverting(bar_url));
  EXPECT_FALSE(dfm->IsDiverting(foo_url));

  EXPECT_EQ(bar_counter, 1);
  EXPECT_EQ(foo_counter, 1000);
}

TEST_F(DiversionFileManagerTest, ReaderKeepsDiversionAlive) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  scoped_refptr<DiversionFileManager> dfm =
      base::MakeRefCounted<DiversionFileManager>();
  storage::FileSystemURL foo_url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/p/q/foo"));

  int foo_counter = 0;

  ASSERT_EQ(
      StartDivertingResult::kOK,
      dfm->StartDiverting(foo_url, base::Seconds(15),
                          IncrementCounterCallback(StoppedReason::kImplicitIdle,
                                                   foo_url, &foo_counter, 1)));

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  std::unique_ptr<storage::FileStreamReader> reader =
      dfm->CreateDivertedFileStreamReader(foo_url, 0);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));
  EXPECT_EQ(foo_counter, 0);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));
  EXPECT_EQ(foo_counter, 0);

  reader.reset();

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));
  EXPECT_EQ(foo_counter, 0);

  task_environment_.FastForwardBy(base::Seconds(10));
  EXPECT_FALSE(dfm->IsDiverting(foo_url));
  EXPECT_EQ(foo_counter, 1);
}

TEST_F(DiversionFileManagerTest, Writes) {
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  scoped_refptr<DiversionFileManager> dfm =
      base::MakeRefCounted<DiversionFileManager>();
  storage::FileSystemURL foo_url = storage::FileSystemURL::CreateForTest(
      GURL("filesystem:chrome-extension://abc/external/p/q/foo"));

  base::FilePath temp_dir;
  ASSERT_TRUE(base::GetTempDir(&temp_dir));
  dfm->OverrideTmpfileDirForTesting(temp_dir);

  static constexpr auto on_implicit_idle =
      [](StoppedReason stopped_reason, const storage::FileSystemURL& url,
         base::ScopedFD scoped_fd, int64_t file_size, base::File::Error error) {
        // We shouldn't get here. We should get to on_explicit_finish instead.
        NOTREACHED();
      };
  ASSERT_EQ(StartDivertingResult::kOK,
            dfm->StartDiverting(foo_url, base::Seconds(15),
                                base::BindOnce(on_implicit_idle)));

  dfm->TruncateDivertedFile(foo_url, 0,
                            base::BindOnce([](base::File::Error result) {
                              EXPECT_EQ(base::File::FILE_OK, result);
                            }));

  std::unique_ptr<storage::FileStreamWriter> writer =
      dfm->CreateDivertedFileStreamWriter(foo_url, 0);

  SynchronousWrite(*writer, "hi ");
  dfm->GetDivertedFileInfo(
      foo_url, {storage::FileSystemOperation::GetMetadataField::kSize},
      base::BindOnce(
          [](base::File::Error result, const base::File::Info& file_info) {
            EXPECT_EQ(base::File::FILE_OK, result);
            EXPECT_EQ(3, file_info.size);
          }));

  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  SynchronousWrite(*writer, "there.");
  dfm->GetDivertedFileInfo(
      foo_url, {storage::FileSystemOperation::GetMetadataField::kSize},
      base::BindOnce(
          [](base::File::Error result, const base::File::Info& file_info) {
            EXPECT_EQ(base::File::FILE_OK, result);
            EXPECT_EQ(9, file_info.size);
          }));

  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_TRUE(dfm->IsDiverting(foo_url));

  bool on_explicit_finish_called = false;
  static constexpr auto on_explicit_finish =
      [](bool* called, StoppedReason stopped_reason,
         const storage::FileSystemURL& url, base::ScopedFD scoped_fd,
         int64_t file_size, base::File::Error error) {
        ASSERT_TRUE(scoped_fd.is_valid());
        EXPECT_EQ(file_size, 9u);
        EXPECT_EQ(base::File::FILE_OK, error);
        char buf[9] = {0};
        EXPECT_TRUE(base::ReadFromFD(scoped_fd.get(), buf));
        EXPECT_EQ(buf, base::span_from_cstring("hi there."));
        *called = true;
      };
  ASSERT_EQ(FinishDivertingResult::kOK,
            dfm->FinishDiverting(foo_url,
                                 base::BindOnce(on_explicit_finish,
                                                &on_explicit_finish_called)));

  task_environment_.FastForwardBy(base::Seconds(20));
  EXPECT_FALSE(dfm->IsDiverting(foo_url));
  EXPECT_FALSE(on_explicit_finish_called);

  writer.reset();
  EXPECT_FALSE(dfm->IsDiverting(foo_url));
  EXPECT_TRUE(on_explicit_finish_called);
}

}  // namespace ash
