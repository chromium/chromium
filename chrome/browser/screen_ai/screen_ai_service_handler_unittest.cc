// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/screen_ai/screen_ai_library_provider.h"
#include "chrome/browser/screen_ai/screen_ai_service_handler_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screen_ai {

constexpr char kIsSuspendedMetric[] = "Accessibility.OCR.Service.IsSuspended";
constexpr char kCrashCountBeforeResumeMetric[] =
    "Accessibility.OCR.Service.CrashCountBeforeResume";

// A library provider without a library, unless the test sets a path.
class FakeScreenAILibraryProvider : public ScreenAILibraryProvider {
 public:
  // ScreenAILibraryProvider:
  base::FilePath GetBinaryPath() const override { return binary_path_; }
  void SetLastUsageTime() override { ++set_last_usage_time_count_; }
  void Download() override { ++download_count_; }

  int set_last_usage_time_count() const { return set_last_usage_time_count_; }
  int download_count() const { return download_count_; }
  void set_binary_path(const base::FilePath& binary_path) {
    binary_path_ = binary_path;
  }

 private:
  base::FilePath binary_path_;
  int set_last_usage_time_count_ = 0;
  int download_count_ = 0;
};

class TestScreenAIServiceHandler : public ScreenAIServiceHandlerBase {
 public:
  explicit TestScreenAIServiceHandler(ScreenAILibraryProvider* library_provider)
      : ScreenAIServiceHandlerBase(library_provider) {}

  std::string GetServiceName() const override { return "OCR"; }

  void LoadModelFilesAndInitialize(
      const base::FilePath& library_binary_path,
      base::TimeTicks request_start_time) override {
    service_connected_ = true;
  }

  bool IsConnectionBound() const override { return service_connected_; }
  bool IsServiceEnabled() const override { return true; }
  void ResetConnection() override { service_connected_ = false; }

 private:
  bool service_connected_ = false;
};

class ScreenAIServiceShutdownHandlerTest : public ::testing::Test {
 public:
  ScreenAIServiceShutdownHandlerTest() : handler(&library_provider_) {}

  bool IsSuspended() { return handler.GetAndRecordSuspendedState(); }
  void DisconnectService() { handler.OnScreenAIServiceDisconnected(); }
  void SendShuttingdownMessage() { handler.ShuttingDownOnIdle(); }
  bool IsServiceAvailable() {
    std::optional<bool> state = handler.GetServiceState();
    if (state.has_value()) {
      // A true value means that the service is already running which is not
      // possible in unittest.
      EXPECT_FALSE(state.value());
      return false;
    } else {
      // An empty result means that the service is not banned, and can be used.
      return true;
    }
  }

  void InitializeService() { handler.InitializeServiceIfNeeded(); }
  std::optional<bool> GetServiceStateAsync(ServiceStateCallback callback) {
    return handler.GetServiceStateAsync(std::move(callback));
  }

 protected:
  // Outlives `handler`, which keeps a pointer to it.
  FakeScreenAILibraryProvider library_provider_;
  TestScreenAIServiceHandler handler;
  base::HistogramTester histogram_tester_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ScreenAIServiceShutdownHandlerTest, SuspendedAfterCrash) {
  EXPECT_FALSE(IsSuspended());
  DisconnectService();
  EXPECT_TRUE(IsSuspended());
  EXPECT_FALSE(IsServiceAvailable());
  histogram_tester_.ExpectBucketCount(kIsSuspendedMetric, false, 1);
  // Expect two since it's queried twice in `IsSuspended` and
  // `IsServiceAvailable`.
  histogram_tester_.ExpectBucketCount(kIsSuspendedMetric, true, 2);
}

TEST_F(ScreenAIServiceShutdownHandlerTest, NotSuspendedAfterShutdown) {
  EXPECT_FALSE(IsSuspended());
  SendShuttingdownMessage();
  DisconnectService();
  EXPECT_FALSE(IsSuspended());
  EXPECT_TRUE(IsServiceAvailable());
  // Expect 3 since it's queried twice in `IsSuspended` and once in
  // `IsServiceAvailable`.
  histogram_tester_.ExpectUniqueSample(kIsSuspendedMetric, false, 3);
}

TEST_F(ScreenAIServiceShutdownHandlerTest, CrashCountBeforeResume) {
  EXPECT_FALSE(IsSuspended());
  DisconnectService();
  EXPECT_TRUE(IsSuspended());
  EXPECT_FALSE(IsServiceAvailable());
  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(IsSuspended());
  EXPECT_TRUE(IsServiceAvailable());

  // Revive after crash is only recorded after shutdown message is received and
  // service disconnects.
  SendShuttingdownMessage();
  DisconnectService();
  histogram_tester_.ExpectUniqueSample(kCrashCountBeforeResumeMetric, 1, 1);
}

TEST_F(ScreenAIServiceShutdownHandlerTest, SecondCrashLongerSuspend) {
  EXPECT_FALSE(IsSuspended());
  DisconnectService();
  EXPECT_TRUE(IsSuspended());
  task_environment_.FastForwardBy(base::Minutes(1));
  EXPECT_FALSE(IsSuspended());
  // Crash again.
  DisconnectService();
  EXPECT_TRUE(IsSuspended());
  task_environment_.FastForwardBy(base::Minutes(1));
  // Still suspended as the second crash results in longer suspend.
  EXPECT_TRUE(IsSuspended());
  task_environment_.FastForwardBy(base::Minutes(3));
  EXPECT_FALSE(IsSuspended());

  SendShuttingdownMessage();
  DisconnectService();
  histogram_tester_.ExpectUniqueSample(kCrashCountBeforeResumeMetric, 2, 1);
}

TEST_F(ScreenAIServiceShutdownHandlerTest, DownloadsLibraryWhenNotAvailable) {
  base::test::TestFuture<bool> service_state;
  EXPECT_FALSE(GetServiceStateAsync(service_state.GetCallback()).has_value());
  EXPECT_FALSE(service_state.IsReady());

  // Normally the request stays in the queue until the download finishes and
  // the handler is told about it. Here the service is initialized while the
  // library is still not on the device, which is what happens when a caller
  // binds the service before the download completes. The handler asks for the
  // download and tells the pending request that the service is not ready.
  InitializeService();
  EXPECT_EQ(library_provider_.set_last_usage_time_count(), 1);
  EXPECT_EQ(library_provider_.download_count(), 1);
  EXPECT_FALSE(service_state.Get());
}

TEST_F(ScreenAIServiceShutdownHandlerTest, DoesNotLaunchWithInvalidPath) {
  for (const auto* path :
       {FILE_PATH_LITERAL("library.so"), FILE_PATH_LITERAL("../library.so")}) {
    library_provider_.set_binary_path(base::FilePath(path));
    base::test::TestFuture<bool> service_state;
    GetServiceStateAsync(service_state.GetCallback());
    EXPECT_FALSE(service_state.IsReady());

    InitializeService();
    EXPECT_FALSE(service_state.Get());
    EXPECT_FALSE(handler.IsProcessRunningForTesting());
  }
  EXPECT_EQ(library_provider_.download_count(), 0);
}

TEST(ScreenAIComponentFilesTest, RejectsPathsOutsideOfComponentFolder) {
  const base::FilePath::CharType kFilesList[] =
      FILE_PATH_LITERAL("files_list.txt");
  // ComponentFiles closes its files on the thread pool. `temp_dir` is declared
  // first, so that the task environment runs these tasks before the folder is
  // deleted.
  base::ScopedTempDir temp_dir;
  base::test::TaskEnvironment task_environment;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath component_folder =
      temp_dir.GetPath().AppendASCII("component");
  ASSERT_TRUE(base::CreateDirectory(component_folder));
  ASSERT_TRUE(base::WriteFile(component_folder.AppendASCII("model"), "data"));
  ASSERT_TRUE(base::WriteFile(temp_dir.GetPath().AppendASCII("other"), "data"));
  const base::FilePath binary_path = component_folder.AppendASCII("library");

  ASSERT_TRUE(base::WriteFile(component_folder.Append(kFilesList), "model\n"));
  EXPECT_EQ(ComponentFiles(binary_path, kFilesList).model_files_.size(), 1u);

  ASSERT_TRUE(base::WriteFile(component_folder.Append(kFilesList),
                              "model\n../other\n"));
  EXPECT_TRUE(ComponentFiles(binary_path, kFilesList).model_files_.empty());

  ASSERT_TRUE(
      base::WriteFile(component_folder.Append(kFilesList),
                      temp_dir.GetPath().AppendASCII("other").AsUTF8Unsafe()));
  EXPECT_TRUE(ComponentFiles(binary_path, kFilesList).model_files_.empty());
}

}  // namespace screen_ai
