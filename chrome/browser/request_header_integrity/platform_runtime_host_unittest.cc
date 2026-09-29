// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/request_header_integrity/platform_runtime_host.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/common/request_header_integrity/platform_runtime.mojom.h"
#include "chrome/common/request_header_integrity/platform_runtime_headers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace request_header_integrity {
namespace {

net::HttpRequestHeaders MakeHeadersA() {
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Test-Header", "aaa");
  return headers;
}

net::HttpRequestHeaders MakeHeadersB() {
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Test-Header", "bbb");
  return headers;
}

// Stands in for the utility process. Every launch creates a new instance, so
// tests can tell a reused service from a relaunched one by counting instances
// rather than calls.
class FakeService : public mojom::PlatformRuntimeService,
                    public mojom::PlatformRuntime {
 public:
  explicit FakeService(
      mojo::PendingReceiver<mojom::PlatformRuntimeService> receiver)
      : receiver_(this, std::move(receiver)), output_(MakeHeadersA()) {}

  ~FakeService() override = default;

  void LoadLibrary(
      const base::FilePath& library_path,
      mojo::PendingReceiver<mojom::PlatformRuntime> runtime) override {
    last_library_path_ = library_path;
    runtime_receiver_.Bind(std::move(runtime));
  }

  void ProcessHeaders(const net::HttpRequestHeaders& input_headers,
                      ProcessHeadersCallback callback) override {
    ++call_count_;
    last_input_headers_ = input_headers;
    if (hang_) {
      // Hold the callback without ever running it, which is what a wedged
      // utility process looks like from the browser's side: the connection
      // stays up, so nothing tells the host the call is never coming back.
      hung_callback_ = std::move(callback);
      return;
    }
    if (status_ == mojom::PlatformRuntimeStatus::kSuccess) {
      std::move(callback).Run(base::ok(output_));
    } else {
      std::move(callback).Run(base::unexpected(status_));
    }
  }

  void SetReply(mojom::PlatformRuntimeStatus status,
                net::HttpRequestHeaders output) {
    status_ = status;
    output_ = std::move(output);
  }
  void set_hang(bool hang) { hang_ = hang; }

  int call_count() const { return call_count_; }
  const base::FilePath& last_library_path() const { return last_library_path_; }
  const net::HttpRequestHeaders& last_input_headers() const {
    return last_input_headers_;
  }

  void CloseConnection() {
    runtime_receiver_.reset();
    receiver_.reset();
  }

 private:
  mojo::Receiver<mojom::PlatformRuntimeService> receiver_;
  mojo::Receiver<mojom::PlatformRuntime> runtime_receiver_{this};
  mojom::PlatformRuntimeStatus status_ = mojom::PlatformRuntimeStatus::kSuccess;
  net::HttpRequestHeaders output_;
  bool hang_ = false;
  ProcessHeadersCallback hung_callback_;
  int call_count_ = 0;
  base::FilePath last_library_path_;
  net::HttpRequestHeaders last_input_headers_;
};

class PlatformRuntimeHostTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    library_path_ = temp_dir_.GetPath().AppendASCII("library.dll");
    ASSERT_TRUE(base::WriteFile(library_path_, "not a real library"));

    host_ = std::make_unique<PlatformRuntimeHost>();
    host_->SetServiceLauncherForTesting(base::BindLambdaForTesting(
        [this](mojo::PendingReceiver<mojom::PlatformRuntimeService> r) {
          services_.push_back(std::make_unique<FakeService>(std::move(r)));
          if (configure_new_service_) {
            configure_new_service_.Run(services_.back().get());
          }
        }));
  }

  void TearDown() override {
    host_.reset();
    services_.clear();
    PlatformRuntimeHeaders::GetInstance().ResetForTesting();
  }

 protected:
  // Drains the mojo round trip. base::test::RunUntil() is unusable here
  // because it polls on a real-time timer, which never fires under MOCK_TIME.
  void FlushTasks() { task_environment_.FastForwardBy(base::TimeDelta()); }

  // Publishes a first value so tests that care about later refreshes can start
  // from a known good state.
  void PrimeWithInitialHeaders() {
    host_->OnComponentReady(library_path_);
    FlushTasks();
    ASSERT_EQ(1u, services_.size());
    ASSERT_EQ(host_->headers().GetHeaderVector(),
              MakeHeadersA().GetHeaderVector());
  }

  FakeService* service(size_t index) { return services_[index].get(); }
  size_t service_count() const { return services_.size(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  base::FilePath library_path_;
  std::vector<std::unique_ptr<FakeService>> services_;
  base::RepeatingCallback<void(FakeService*)> configure_new_service_;
  std::unique_ptr<PlatformRuntimeHost> host_;
  base::HistogramTester histograms_;
};

TEST_F(PlatformRuntimeHostTest, PublishesHeadersAfterComponentReady) {
  bool notified = false;
  auto subscription = host_->RegisterHeadersChangedCallback(
      base::BindLambdaForTesting([&]() { notified = true; }));

  EXPECT_TRUE(host_->headers().IsEmpty());

  host_->OnComponentReady(library_path_);
  FlushTasks();

  EXPECT_EQ(host_->headers().GetHeaderVector(),
            MakeHeadersA().GetHeaderVector());
  EXPECT_TRUE(notified);
  ASSERT_EQ(1u, service_count());
  EXPECT_EQ(library_path_, service(0)->last_library_path());

  // The block is published process-locally too, not just held by the host.
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Test-Header", "placeholder");
  EXPECT_EQ(PlatformRuntimeApplyResult::kApplied,
            PlatformRuntimeHeaders::GetInstance().Apply(&headers));
  EXPECT_EQ("aaa", headers.GetHeader("X-Test-Header"));
}

// The service is invoked over headers the browser mints itself, so it never
// has to be told what any of them mean.
TEST_F(PlatformRuntimeHostTest, SuppliesBrowserMintedInputHeaders) {
  ASSERT_NO_FATAL_FAILURE(PrimeWithInitialHeaders());

  EXPECT_FALSE(service(0)->last_input_headers().IsEmpty());
}

// A relative path can never be handed to WithPreloadedLibraries(), which
// CHECKs on it inside the child.
TEST_F(PlatformRuntimeHostTest, RelativePathIsRejected) {
  host_->OnComponentReady(base::FilePath(FILE_PATH_LITERAL("library.dll")));
  FlushTasks();

  EXPECT_EQ(0u, service_count());
}

// The service is long-lived: refreshes reuse the process rather than
// relaunching it, so the library is loaded from disk exactly once.
TEST_F(PlatformRuntimeHostTest, RefreshReusesTheSameServiceProcess) {
  ASSERT_NO_FATAL_FAILURE(PrimeWithInitialHeaders());

  for (int i = 0; i < 3; ++i) {
    task_environment_.FastForwardBy(base::Minutes(3));
  }

  EXPECT_EQ(1u, service_count()) << "service process was relaunched";
  EXPECT_EQ(4, service(0)->call_count());
}

TEST_F(PlatformRuntimeHostTest, NewValueIsRepublishedToObservers) {
  int notifications = 0;
  auto subscription = host_->RegisterHeadersChangedCallback(
      base::BindLambdaForTesting([&]() { ++notifications; }));

  ASSERT_NO_FATAL_FAILURE(PrimeWithInitialHeaders());
  EXPECT_EQ(1, notifications);

  service(0)->SetReply(mojom::PlatformRuntimeStatus::kSuccess, MakeHeadersB());
  task_environment_.FastForwardBy(base::Minutes(3));

  EXPECT_EQ(host_->headers().GetHeaderVector(),
            MakeHeadersB().GetHeaderVector());
  EXPECT_EQ(2, notifications);
}

// Republishing an unchanged value would wake every renderer for nothing.
TEST_F(PlatformRuntimeHostTest, IdenticalValueDoesNotNotify) {
  int notifications = 0;
  auto subscription = host_->RegisterHeadersChangedCallback(
      base::BindLambdaForTesting([&]() { ++notifications; }));

  ASSERT_NO_FATAL_FAILURE(PrimeWithInitialHeaders());
  ASSERT_EQ(1, notifications);

  task_environment_.FastForwardBy(base::Minutes(3));
  ASSERT_EQ(2, service(0)->call_count());

  EXPECT_EQ(1, notifications);
}

TEST_F(PlatformRuntimeHostTest, FailedRefreshKeepsPreviousValue) {
  ASSERT_NO_FATAL_FAILURE(PrimeWithInitialHeaders());

  service(0)->SetReply(mojom::PlatformRuntimeStatus::kFailure, {});
  task_environment_.FastForwardBy(base::Minutes(3));

  EXPECT_EQ(host_->headers().GetHeaderVector(),
            MakeHeadersA().GetHeaderVector());
  histograms_.ExpectBucketCount(
      "ComponentUpdater.PlatformRuntime.ProcessHeadersStatus",
      mojom::PlatformRuntimeStatus::kFailure, 1);
}

// An empty output is reported as kNoOutput rather than published as an empty
// block, which would wipe out a perfectly good previous value.
TEST_F(PlatformRuntimeHostTest, NoOutputKeepsPreviousValue) {
  ASSERT_NO_FATAL_FAILURE(PrimeWithInitialHeaders());

  service(0)->SetReply(mojom::PlatformRuntimeStatus::kNoOutput, {});
  task_environment_.FastForwardBy(base::Minutes(3));

  EXPECT_EQ(host_->headers().GetHeaderVector(),
            MakeHeadersA().GetHeaderVector());
  histograms_.ExpectBucketCount(
      "ComponentUpdater.PlatformRuntime.ProcessHeadersStatus",
      mojom::PlatformRuntimeStatus::kNoOutput, 1);
}

TEST_F(PlatformRuntimeHostTest,
       LibraryUnavailableStopsRetryingUntilComponentUpdate) {
  configure_new_service_ = base::BindLambdaForTesting([](FakeService* s) {
    s->SetReply(mojom::PlatformRuntimeStatus::kLibraryUnavailable, {});
  });

  host_->OnComponentReady(library_path_);
  FlushTasks();
  ASSERT_EQ(1u, service_count());
  EXPECT_EQ(1, service(0)->call_count());
  histograms_.ExpectBucketCount(
      "ComponentUpdater.PlatformRuntime.ProcessHeadersStatus",
      mojom::PlatformRuntimeStatus::kLibraryUnavailable, 1);

  // Advancing far past the refresh and backoff intervals must neither poll the
  // existing service nor launch a new one while the component is unchanged.
  task_environment_.FastForwardBy(base::Minutes(30));
  EXPECT_EQ(1u, service_count());
  EXPECT_EQ(1, service(0)->call_count());

  // Once a new component arrives via OnComponentReady(), the host launches a
  // fresh service and resumes refreshing.
  configure_new_service_ = base::DoNothing();
  const base::FilePath updated_path =
      temp_dir_.GetPath().AppendASCII("library_v2.dll");
  ASSERT_TRUE(base::WriteFile(updated_path, "updated"));
  host_->OnComponentReady(updated_path);
  FlushTasks();

  ASSERT_EQ(2u, service_count());
  EXPECT_EQ(host_->headers().GetHeaderVector(),
            MakeHeadersA().GetHeaderVector());
}

// A service that hangs without disconnecting must not stall refreshes forever.
TEST_F(PlatformRuntimeHostTest, HungCallIsAbandonedAndServiceRelaunched) {
  configure_new_service_ =
      base::BindLambdaForTesting([](FakeService* s) { s->set_hang(true); });

  host_->OnComponentReady(library_path_);
  FlushTasks();
  ASSERT_EQ(1u, service_count());
  ASSERT_EQ(1, service(0)->call_count());

  // Still inside the deadline: no reply, no teardown, nothing published.
  task_environment_.FastForwardBy(base::Seconds(29));
  EXPECT_EQ(1u, service_count());
  EXPECT_TRUE(host_->headers().IsEmpty());

  // Deadline passes. The wedged process is dropped and a relaunch scheduled,
  // this time landing on a service that answers.
  configure_new_service_ = base::DoNothing();
  task_environment_.FastForwardBy(base::Seconds(2));
  histograms_.ExpectBucketCount(
      "ComponentUpdater.PlatformRuntime.RefreshTimedOut", true, 1);

  task_environment_.FastForwardBy(base::Minutes(1));
  ASSERT_EQ(2u, service_count());
  EXPECT_EQ(host_->headers().GetHeaderVector(),
            MakeHeadersA().GetHeaderVector());
}

TEST_F(PlatformRuntimeHostTest, CrashedServiceIsRelaunched) {
  ASSERT_NO_FATAL_FAILURE(PrimeWithInitialHeaders());

  service(0)->CloseConnection();
  task_environment_.FastForwardBy(base::Minutes(1));
  ASSERT_EQ(2u, service_count());

  service(1)->SetReply(mojom::PlatformRuntimeStatus::kSuccess, MakeHeadersB());
  task_environment_.FastForwardBy(base::Minutes(3));
  EXPECT_EQ(host_->headers().GetHeaderVector(),
            MakeHeadersB().GetHeaderVector());
}

TEST_F(PlatformRuntimeHostTest, BackoffNeverExceedsUsefulLifetime) {
  // Every service drops its connection immediately, standing in for a library
  // that always fails to load in the child.
  configure_new_service_ =
      base::BindLambdaForTesting([](FakeService* s) { s->CloseConnection(); });

  host_->OnComponentReady(library_path_);
  FlushTasks();
  ASSERT_EQ(1u, service_count());

  size_t attempts_at_last_check = service_count();
  // Far past the point where a doubling backoff starting at 30s would have
  // escaped a five minute cap.
  for (int window = 0; window < 60; ++window) {
    task_environment_.FastForwardBy(base::Minutes(6));
    ASSERT_GT(service_count(), attempts_at_last_check)
        << "no relaunch attempt during the six minute window ending at "
        << (window + 1) * 6
        << " minutes; the backoff has grown past the point where the "
           "published headers are still useful";
    attempts_at_last_check = service_count();
  }
}

// A component update must take effect promptly even if a call against the old
// library is still outstanding. Dropping the old service discards that call's
// reply callback, so the host has to notice the call is gone rather than wait
// out the deadline.
TEST_F(PlatformRuntimeHostTest, ComponentUpdateSupersedesInFlightCall) {
  configure_new_service_ =
      base::BindLambdaForTesting([](FakeService* s) { s->set_hang(true); });

  host_->OnComponentReady(library_path_);
  FlushTasks();
  ASSERT_EQ(1u, service_count());
  ASSERT_EQ(1, service(0)->call_count());

  const base::FilePath updated_path =
      temp_dir_.GetPath().AppendASCII("library_v2.dll");
  ASSERT_TRUE(base::WriteFile(updated_path, "not a real library either"));

  configure_new_service_ = base::DoNothing();
  host_->OnComponentReady(updated_path);
  FlushTasks();

  ASSERT_EQ(2u, service_count());
  EXPECT_EQ(host_->headers().GetHeaderVector(),
            MakeHeadersA().GetHeaderVector());
  EXPECT_EQ(updated_path, service(1)->last_library_path());
}

}  // namespace
}  // namespace request_header_integrity
