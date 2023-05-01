// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/patch/content/patch_service.h"
#include "components/services/patch/public/cpp/patch.h"
#include "components/update_client/component_patcher_operation.h"
#include "components/update_client/test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

namespace {

constexpr base::TaskTraits kThreadPoolTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

}  // namespace

class PatchTest : public InProcessBrowserTest {
 public:
  PatchTest() {
    EXPECT_TRUE(installed_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(input_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(unpack_dir_.CreateUniqueTempDir());
  }

  PatchTest(const PatchTest&) = delete;
  PatchTest& operator=(const PatchTest&) = delete;

  base::FilePath InputFilePath(const char* name) {
    base::FilePath path = installed_dir_.GetPath().AppendASCII(name);

    base::RunLoop run_loop;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, kThreadPoolTaskTraits,
        base::BindOnce(&PatchTest::CopyFile,
                       update_client::GetTestFilePath(name), path),
        run_loop.QuitClosure());

    run_loop.Run();
    return path;
  }

  base::FilePath PatchFilePath(const char* name) {
    base::FilePath path = input_dir_.GetPath().AppendASCII(name);

    base::RunLoop run_loop;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, kThreadPoolTaskTraits,
        base::BindOnce(&PatchTest::CopyFile,
                       update_client::GetTestFilePath(name), path),
        run_loop.QuitClosure());

    run_loop.Run();
    return path;
  }

  base::FilePath OutputFilePath(const char* name) {
    return unpack_dir_.GetPath().AppendASCII(name);
  }

  base::FilePath InvalidPath(const char* name) {
    return input_dir_.GetPath().AppendASCII("nonexistent").AppendASCII(name);
  }

  void RunPatchTest(const std::string& operation,
                    const base::FilePath& input,
                    const base::FilePath& patch,
                    const base::FilePath& output,
                    int expected_result) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    done_called_ = false;

    base::ThreadPool::CreateSequencedTaskRunner(kThreadPoolTaskTraits)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&PatchTest::PatchAsyncSequencedTaskRunner,
                                  base::Unretained(this), operation, input,
                                  patch, output, expected_result));
    run_loop.Run();
    EXPECT_TRUE(done_called_);
  }

 private:
  void PatchAsyncSequencedTaskRunner(
      const std::string& operation,
      const base::FilePath& input,
      const base::FilePath& patch,
      const base::FilePath& output,
      int expected_result) {
    patch::Patch(patch::LaunchFilePatcher(), operation, input, patch, output,
                 base::BindOnce(&PatchTest::PatchDone, base::Unretained(this),
                                expected_result));
  }

  void PatchDone(int expected, int result) {
    EXPECT_EQ(expected, result);
    done_called_ = true;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(quit_closure_));
  }

  static void CopyFile(const base::FilePath& source,
                       const base::FilePath& target) {
    EXPECT_TRUE(base::CopyFile(source, target));
  }

  base::ScopedTempDir installed_dir_;
  base::ScopedTempDir input_dir_;
  base::ScopedTempDir unpack_dir_;
  base::OnceClosure quit_closure_;
  bool done_called_;
};

IN_PROC_BROWSER_TEST_F(PatchTest, CheckBsdiffOperation) {
  constexpr int kExpectedResult = bsdiff::OK;

  base::FilePath input_file = InputFilePath("binary_input.bin");
  base::FilePath patch_file = PatchFilePath("binary_bsdiff_patch.bin");
  base::FilePath output_file = OutputFilePath("output.bin");

  RunPatchTest(update_client::kBsdiff, input_file, patch_file, output_file,
               kExpectedResult);

  EXPECT_TRUE(base::ContentsEqual(
      update_client::GetTestFilePath("binary_output.bin"), output_file));
}

IN_PROC_BROWSER_TEST_F(PatchTest, CheckCourgetteOperation) {
  constexpr int kExpectedResult = courgette::C_OK;

  base::FilePath input_file = InputFilePath("binary_input.bin");
  base::FilePath patch_file = PatchFilePath("binary_courgette_patch.bin");
  base::FilePath output_file = OutputFilePath("output.bin");

  RunPatchTest(update_client::kCourgette, input_file, patch_file, output_file,
               kExpectedResult);

  EXPECT_TRUE(base::ContentsEqual(
      update_client::GetTestFilePath("binary_output.bin"), output_file));
}

IN_PROC_BROWSER_TEST_F(PatchTest, InvalidInputFile) {
  constexpr int kInvalidInputFile = -1;

  base::FilePath invalid = InvalidPath("binary_input.bin");
  base::FilePath patch_file = PatchFilePath("binary_courgette_patch.bin");
  base::FilePath output_file = OutputFilePath("output.bin");

  RunPatchTest(update_client::kCourgette, invalid, patch_file, output_file,
               kInvalidInputFile);
}

IN_PROC_BROWSER_TEST_F(PatchTest, InvalidPatchFile) {
  constexpr int kInvalidPatchFile = -1;

  base::FilePath input_file = InputFilePath("binary_input.bin");
  base::FilePath invalid = InvalidPath("binary_courgette_patch.bin");
  base::FilePath output_file = OutputFilePath("output.bin");

  RunPatchTest(update_client::kCourgette, input_file, invalid, output_file,
               kInvalidPatchFile);
}

IN_PROC_BROWSER_TEST_F(PatchTest, InvalidOutputFile) {
  constexpr int kInvalidOutputFile = -1;

  base::FilePath input_file = InputFilePath("binary_input.bin");
  base::FilePath patch_file = PatchFilePath("binary_courgette_patch.bin");
  base::FilePath invalid = InvalidPath("output.bin");

  RunPatchTest(update_client::kCourgette, input_file, patch_file, invalid,
               kInvalidOutputFile);
}
