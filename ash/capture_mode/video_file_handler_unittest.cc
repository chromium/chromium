// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/video_file_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class VideoFileHandlerTest : public ::testing::Test {
 public:
  VideoFileHandlerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}
  VideoFileHandlerTest(const VideoFileHandlerTest&) = delete;
  VideoFileHandlerTest& operator=(const VideoFileHandlerTest&) = delete;
  ~VideoFileHandlerTest() override = default;

  scoped_refptr<base::SequencedTaskRunner> task_runner() const {
    return task_runner_;
  }
  const base::FilePath& temp_file() const { return temp_file_; }

  // ::testing::Test:
  void SetUp() override {
    EXPECT_TRUE(ScheduleFileOpTaskAndWait(
        base::BindOnce(&base::CreateTemporaryFile, &temp_file_)));
    EXPECT_FALSE(temp_file_.empty());
  }

  // Creates and returns an initialized VideoFileHandler instance.
  base::SequenceBound<VideoFileHandler> CreateAndInitHandler(size_t capacity) {
    base::SequenceBound<VideoFileHandler> handler =
        VideoFileHandler::Create(task_runner(), temp_file(), capacity);
    const bool success =
        RunOnHandlerAndWait(&handler, &VideoFileHandler::Initialize);
    EXPECT_TRUE(success);
    return handler;
  }

  // Schedules and waits for a file IO |task|, and returns its result.
  using FileOpTask = base::OnceCallback<bool()>;
  bool ScheduleFileOpTaskAndWait(FileOpTask task) {
    bool result = false;
    base::RunLoop run_loop;
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, std::move(task),
        base::BindOnce(
            [](base::RunLoop* loop, bool* result, bool success) {
              *result = success;
              loop->Quit();
            },
            &run_loop, &result));
    run_loop.Run();
    return result;
  }

  // Reads and returns the contents of the |temp_file_|.
  std::string ReadTempFileContent() {
    std::string file_content;
    EXPECT_TRUE(ScheduleFileOpTaskAndWait(
        base::BindOnce(&base::ReadFileToString, temp_file_, &file_content)));
    return file_content;
  }

  // Runs an async |method| on the VideoFileHandler instance |handler| and waits
  // for it complete and returns its result.
  template <typename Method>
  bool RunOnHandlerAndWait(base::SequenceBound<VideoFileHandler>* handler,
                           Method method) const {
    base::RunLoop run_loop;
    bool result = false;
    handler->AsyncCall(method).Then(base::BindOnce(
        [](base::RunLoop* loop, bool* result, bool success) {
          *result = success;
          loop->Quit();
        },
        &run_loop, &result));
    run_loop.Run();
    return result;
  }

  // Returns the success status of all IO operations done so far by the given
  // |handler|.
  bool GetSuccessStatusOnUi(
      base::SequenceBound<VideoFileHandler>* handler) const {
    return RunOnHandlerAndWait(handler, &VideoFileHandler::GetSuccessStatus);
  }

  // |base::SequenceBound| does not allow passing a null callback to its
  // |Then()| operations. This function is a convenience for getting a callback
  // that does nothing.
  base::OnceCallback<void(bool)> GetIgnoreResultCallback() const {
    return base::BindOnce([](bool) {});
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::FilePath temp_file_;
};

TEST_F(VideoFileHandlerTest, ChunksHandling) {
  constexpr size_t kCapacity = 10;
  base::SequenceBound<VideoFileHandler> handler =
      CreateAndInitHandler(kCapacity);
  ASSERT_TRUE(handler);

  // Append a chunk smaller than the capacity. Nothing will be written to the
  // file yet.
  std::string chunk_1 = "12345";
  handler.AsyncCall(&VideoFileHandler::AppendChunk)
      .WithArgs(chunk_1)
      .Then(GetIgnoreResultCallback());
  std::string file_content = ReadTempFileContent();
  EXPECT_TRUE(file_content.empty());
  EXPECT_TRUE(GetSuccessStatusOnUi(&handler));

  // Append another chunk which together with what is cached in the handler
  // buffer would exceed the capacity. Only what is in the buffer will be
  // written now.
  std::string chunk_2 = "1234567";
  handler.AsyncCall(&VideoFileHandler::AppendChunk)
      .WithArgs(chunk_2)
      .Then(GetIgnoreResultCallback());
  file_content = ReadTempFileContent();
  EXPECT_EQ(file_content, chunk_1);
  EXPECT_TRUE(GetSuccessStatusOnUi(&handler));

  // Now chunk_2 is cached with a size equals to 7. Appending another chunk with
  // size equals to 3 would still be within the buffer capacity. Nothing will be
  // flushed yet.
  std::string chunk_3 = "89A";
  handler.AsyncCall(&VideoFileHandler::AppendChunk)
      .WithArgs(chunk_3)
      .Then(GetIgnoreResultCallback());
  file_content = ReadTempFileContent();
  EXPECT_EQ(file_content, chunk_1);
  EXPECT_TRUE(GetSuccessStatusOnUi(&handler));

  // Appending another chunk will cause a flush.
  std::string chunk_4 = "BCDEFG";
  handler.AsyncCall(&VideoFileHandler::AppendChunk)
      .WithArgs(chunk_4)
      .Then(GetIgnoreResultCallback());
  file_content = ReadTempFileContent();
  EXPECT_EQ(file_content, chunk_1 + chunk_2 + chunk_3);
  EXPECT_TRUE(GetSuccessStatusOnUi(&handler));

  // Destroying the handler will schedule its destruction on the task runner,
  // and will cause a flush of the remaining chunk_4 in the cache. After that,
  // the file content will be complete.
  handler.Reset();
  file_content = ReadTempFileContent();
  EXPECT_EQ(file_content, chunk_1 + chunk_2 + chunk_3 + chunk_4);
}

TEST_F(VideoFileHandlerTest, BigChunks) {
  constexpr size_t kCapacity = 10;
  base::SequenceBound<VideoFileHandler> handler =
      CreateAndInitHandler(kCapacity);
  ASSERT_TRUE(handler);

  // Append a chunk smaller than the capacity. Nothing will be written to the
  // file yet.
  std::string chunk_1 = "12345";
  handler.AsyncCall(&VideoFileHandler::AppendChunk)
      .WithArgs(chunk_1)
      .Then(GetIgnoreResultCallback());
  std::string file_content = ReadTempFileContent();
  EXPECT_TRUE(file_content.empty());
  EXPECT_TRUE(GetSuccessStatusOnUi(&handler));

  // Appending a big chunk that is bigger than the buffer capacity will cause a
  // flush of what's currently cached, followed by an immediate write of that
  // big chunk, such that the file content will be complete.
  std::string chunk_2 = "123456789ABCDEF";
  handler.AsyncCall(&VideoFileHandler::AppendChunk)
      .WithArgs(chunk_2)
      .Then(GetIgnoreResultCallback());
  file_content = ReadTempFileContent();
  EXPECT_EQ(file_content, chunk_1 + chunk_2);
  EXPECT_TRUE(GetSuccessStatusOnUi(&handler));
}

TEST_F(VideoFileHandlerTest, ManualFlush) {
  constexpr size_t kCapacity = 10;
  base::SequenceBound<VideoFileHandler> handler =
      CreateAndInitHandler(kCapacity);
  ASSERT_TRUE(handler);

  // Append a chunk smaller than the capacity. Nothing will be written to the
  // file yet.
  std::string chunk_1 = "12345";
  handler.AsyncCall(&VideoFileHandler::AppendChunk)
      .WithArgs(chunk_1)
      .Then(GetIgnoreResultCallback());
  std::string file_content = ReadTempFileContent();
  EXPECT_TRUE(file_content.empty());
  EXPECT_TRUE(GetSuccessStatusOnUi(&handler));

  // It's possible to flush the buffer manually.
  base::RunLoop run_loop;
  handler.AsyncCall(&VideoFileHandler::FlushBufferedChunks)
      .Then(base::BindOnce(
          [](base::RunLoop* loop, bool success) {
            EXPECT_TRUE(success);
            loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  file_content = ReadTempFileContent();
  EXPECT_EQ(file_content, chunk_1);
  EXPECT_TRUE(GetSuccessStatusOnUi(&handler));
}

}  // namespace ash
