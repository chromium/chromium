// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/screen_ai/screen_ai_library_provider.h"
#include "chrome/browser/screen_ai/screen_ai_service_handler_base.h"
#include "net/base/network_change_notifier.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace screen_ai {

constexpr char kIsSuspendedMetric[] = "Accessibility.OCR.Service.IsSuspended";
constexpr char kCrashCountBeforeResumeMetric[] =
    "Accessibility.OCR.Service.CrashCountBeforeResume";
constexpr char kComponentAvailableMetric[] =
    "Accessibility.ScreenAI.Component.Available2";

// The values of `ComponentAvailability` in `enums.xml`.
constexpr int kAvailable = 0;
constexpr int kUnavailableWithNetwork = 1;
constexpr int kUnavailableWithoutNetwork = 2;

// A library provider without a library, unless the test sets a path. It
// answers the requests for the library when the test tells it to.
class FakeScreenAILibraryProvider : public ScreenAILibraryProvider {
 public:
  // ScreenAILibraryProvider:
  base::FilePath GetBinaryPath() const override { return binary_path_; }
  void RecordUsage() override { ++record_usage_count_; }
  base::CallbackListSubscription EnsureLibraryAvailable(
      base::OnceCallback<void(bool)> callback) override {
    ++library_request_count_;
    if (answer_for_next_request_) {
      // Forget the answer first, as `callback` can ask for the library again.
      bool available = *std::exchange(answer_for_next_request_, std::nullopt);
      std::move(callback).Run(available);
      return {};
    }
    return pending_callbacks_->Add(std::move(callback));
  }

  // Answers the pending requests for the library. Like the real provider, it
  // does not tell this answer to a request that one of the callbacks makes.
  void NotifyLibraryAvailability(bool available) {
    auto callbacks =
        std::exchange(pending_callbacks_, std::make_unique<PendingCallbacks>());
    callbacks->Notify(available);
  }

  // Makes the next request for the library get `available` as its answer
  // before `EnsureLibraryAvailable()` returns. The real provider does this
  // when the library is already on the device, and when a download cannot
  // start.
  void AnswerNextRequestAtOnce(bool available) {
    answer_for_next_request_ = available;
  }

  int record_usage_count() const { return record_usage_count_; }
  int library_request_count() const { return library_request_count_; }
  bool has_pending_callbacks() const { return !pending_callbacks_->empty(); }
  void set_binary_path(const base::FilePath& binary_path) {
    binary_path_ = binary_path;
  }

 private:
  using PendingCallbacks = base::OnceCallbackList<void(bool)>;

  std::unique_ptr<PendingCallbacks> pending_callbacks_ =
      std::make_unique<PendingCallbacks>();
  std::optional<bool> answer_for_next_request_;
  base::FilePath binary_path_;
  int record_usage_count_ = 0;
  int library_request_count_ = 0;
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
  void GetServiceStateAsync(ServiceStateCallback callback) {
    handler.GetServiceStateAsync(std::move(callback));
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
  GetServiceStateAsync(service_state.GetCallback());
  EXPECT_FALSE(service_state.IsReady());

  // Normally the request stays in the queue until the download finishes and
  // the handler is told about it. Here the service is initialized while the
  // library is still not on the device, which is what happens when a caller
  // binds the service before the download completes. The handler asks for the
  // download and tells the pending request that the service is not ready.
  InitializeService();
  EXPECT_EQ(library_provider_.record_usage_count(), 1);
  EXPECT_FALSE(service_state.Get());

  // The handler has asked for the library twice: for the request, and when it
  // found no library to launch the service with.
  EXPECT_EQ(library_provider_.library_request_count(), 2);
}

TEST_F(ScreenAIServiceShutdownHandlerTest,
       DoesNotWaitForDownloadThatLaunchStarts) {
  // A caller binds the service while the library is not on the device.
  InitializeService();
  EXPECT_EQ(library_provider_.library_request_count(), 1);

  // The handler only starts the download. It does not wait for it.
  EXPECT_FALSE(library_provider_.has_pending_callbacks());
}

TEST_F(ScreenAIServiceShutdownHandlerTest, DoesNotLaunchWithInvalidPath) {
  for (const auto* path :
       {FILE_PATH_LITERAL("library.so"), FILE_PATH_LITERAL("../library.so")}) {
    library_provider_.set_binary_path(base::FilePath(path));
    base::test::TestFuture<bool> service_state;
    GetServiceStateAsync(service_state.GetCallback());
    EXPECT_FALSE(service_state.IsReady());

    library_provider_.NotifyLibraryAvailability(true);
    EXPECT_FALSE(service_state.Get());
    EXPECT_FALSE(handler.IsProcessRunningForTesting());
  }

  // The handler has asked for the library once for each request, and not
  // again when it did not launch the service.
  EXPECT_EQ(library_provider_.library_request_count(), 2);
}

TEST_F(ScreenAIServiceShutdownHandlerTest, AsksForLibraryWhenStateIsNotKnown) {
  base::test::TestFuture<bool> service_state;
  GetServiceStateAsync(service_state.GetCallback());

  // The request waits until the library provider tells if the library is
  // available.
  EXPECT_EQ(library_provider_.library_request_count(), 1);
  EXPECT_FALSE(service_state.IsReady());

  library_provider_.NotifyLibraryAvailability(false);
  EXPECT_FALSE(service_state.Get());
}

TEST_F(ScreenAIServiceShutdownHandlerTest, AsksForLibraryOnceWhileWaiting) {
  base::test::TestFuture<bool> first_service_state;
  base::test::TestFuture<bool> second_service_state;
  GetServiceStateAsync(first_service_state.GetCallback());
  GetServiceStateAsync(second_service_state.GetCallback());
  EXPECT_EQ(library_provider_.library_request_count(), 1);

  // The answer of the library provider is for both requests.
  library_provider_.NotifyLibraryAvailability(false);
  EXPECT_FALSE(first_service_state.Get());
  EXPECT_FALSE(second_service_state.Get());

  // A request that comes after the answer asks for the library again.
  base::test::TestFuture<bool> third_service_state;
  GetServiceStateAsync(third_service_state.GetCallback());
  EXPECT_EQ(library_provider_.library_request_count(), 2);
}

TEST_F(ScreenAIServiceShutdownHandlerTest,
       AnswersWhenLibraryIsNotAvailableAtOnce) {
  // Normally the library provider answers later. Here it tells that the
  // library is not available before the request for it returns.
  library_provider_.AnswerNextRequestAtOnce(false);
  base::test::TestFuture<bool> service_state;
  GetServiceStateAsync(service_state.GetCallback());
  ASSERT_TRUE(service_state.IsReady());
  EXPECT_FALSE(service_state.Get());
  histogram_tester_.ExpectUniqueSample(kComponentAvailableMetric,
                                       kUnavailableWithNetwork, 1);

  // The handler does not wait for the library anymore, so the next request
  // asks for it again.
  base::test::TestFuture<bool> next_service_state;
  GetServiceStateAsync(next_service_state.GetCallback());
  EXPECT_EQ(library_provider_.library_request_count(), 2);
  EXPECT_TRUE(library_provider_.has_pending_callbacks());
}

TEST_F(ScreenAIServiceShutdownHandlerTest, UsesLibraryThatIsAvailableAtOnce) {
  // The handler does not launch the service with this path, so the test can
  // see the end of the request.
  library_provider_.set_binary_path(
      base::FilePath(FILE_PATH_LITERAL("library.so")));

  // Normally the library provider answers later. Here it tells that the
  // library is available before the request for it returns.
  library_provider_.AnswerNextRequestAtOnce(true);
  base::test::TestFuture<bool> service_state;
  GetServiceStateAsync(service_state.GetCallback());
  ASSERT_TRUE(service_state.IsReady());
  EXPECT_FALSE(service_state.Get());
  histogram_tester_.ExpectUniqueSample(kComponentAvailableMetric, kAvailable,
                                       1);

  // The handler does not wait for the library anymore, so the next request
  // asks for it again.
  base::test::TestFuture<bool> next_service_state;
  GetServiceStateAsync(next_service_state.GetCallback());
  EXPECT_EQ(library_provider_.library_request_count(), 2);
  EXPECT_TRUE(library_provider_.has_pending_callbacks());
}

TEST_F(ScreenAIServiceShutdownHandlerTest,
       RequestFromCallbackOfRequestThatFailsAtOnce) {
  // Normally a request is answered after `GetServiceStateAsync()` has
  // returned. Here the library provider answers at once, so the callback of
  // the first request runs inside that call. It asks for the service again,
  // like a caller that tries once more.
  library_provider_.AnswerNextRequestAtOnce(false);
  base::test::TestFuture<bool> second_service_state;
  GetServiceStateAsync(base::BindLambdaForTesting([&](bool successful) {
    EXPECT_FALSE(successful);
    GetServiceStateAsync(second_service_state.GetCallback());
  }));

  // The second request has asked for the library, and still waits for it
  // after the first one has returned.
  EXPECT_EQ(library_provider_.library_request_count(), 2);
  EXPECT_TRUE(library_provider_.has_pending_callbacks());
  EXPECT_FALSE(second_service_state.IsReady());

  library_provider_.NotifyLibraryAvailability(false);
  EXPECT_FALSE(second_service_state.Get());
}

TEST_F(ScreenAIServiceShutdownHandlerTest, RequestFromCallbackOfFailedRequest) {
  // The callback of the first request asks for the service again when the
  // library provider tells that the library is not available.
  base::test::TestFuture<bool> second_service_state;
  GetServiceStateAsync(base::BindLambdaForTesting([&](bool successful) {
    EXPECT_FALSE(successful);
    GetServiceStateAsync(second_service_state.GetCallback());
  }));
  library_provider_.NotifyLibraryAvailability(false);

  // The second request waits for a new answer of the library provider.
  EXPECT_EQ(library_provider_.library_request_count(), 2);
  EXPECT_TRUE(library_provider_.has_pending_callbacks());
  EXPECT_FALSE(second_service_state.IsReady());

  library_provider_.NotifyLibraryAvailability(false);
  EXPECT_FALSE(second_service_state.Get());
}

TEST_F(ScreenAIServiceShutdownHandlerTest, DoesNotAskForLibraryWhenSuspended) {
  DisconnectService();

  // The request is answered at once, as the service is suspended.
  base::test::TestFuture<bool> service_state;
  GetServiceStateAsync(service_state.GetCallback());
  ASSERT_TRUE(service_state.IsReady());
  EXPECT_FALSE(service_state.Get());
  EXPECT_EQ(library_provider_.library_request_count(), 0);
}

TEST_F(ScreenAIServiceShutdownHandlerTest, StopsWaitingWhenDestroyed) {
  {
    TestScreenAIServiceHandler other_handler(&library_provider_);
    other_handler.GetServiceStateAsync(base::DoNothing());
    EXPECT_TRUE(library_provider_.has_pending_callbacks());
  }

  // The library provider does not keep the request of a destroyed handler.
  EXPECT_FALSE(library_provider_.has_pending_callbacks());
}

TEST_F(ScreenAIServiceShutdownHandlerTest,
       RecordsLibraryAsAvailableWhenStateIsKnown) {
  DisconnectService();

  // The service is suspended, so its state is known and the library is not
  // asked for.
  base::test::TestFuture<bool> service_state;
  GetServiceStateAsync(service_state.GetCallback());
  histogram_tester_.ExpectUniqueSample(kComponentAvailableMetric, kAvailable,
                                       1);
}

TEST_F(ScreenAIServiceShutdownHandlerTest,
       RecordsLibraryAvailabilityAfterAskingForIt) {
  base::test::TestFuture<bool> service_state;
  GetServiceStateAsync(service_state.GetCallback());

  // Nothing is recorded while the request waits for the library.
  histogram_tester_.ExpectTotalCount(kComponentAvailableMetric, 0);

  library_provider_.NotifyLibraryAvailability(true);
  histogram_tester_.ExpectUniqueSample(kComponentAvailableMetric, kAvailable,
                                       1);
}

TEST_F(ScreenAIServiceShutdownHandlerTest,
       RecordsNetworkStateWhenLibraryIsNotAvailable) {
  base::test::TestFuture<bool> first_service_state;
  GetServiceStateAsync(first_service_state.GetCallback());
  library_provider_.NotifyLibraryAvailability(false);
  histogram_tester_.ExpectUniqueSample(kComponentAvailableMetric,
                                       kUnavailableWithNetwork, 1);

  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  base::test::TestFuture<bool> second_service_state;
  GetServiceStateAsync(second_service_state.GetCallback());
  library_provider_.NotifyLibraryAvailability(false);
  histogram_tester_.ExpectBucketCount(kComponentAvailableMetric,
                                      kUnavailableWithoutNetwork, 1);
  histogram_tester_.ExpectTotalCount(kComponentAvailableMetric, 2);
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
