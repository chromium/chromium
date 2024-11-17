// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/metrics/perf/perf_output.h"

#include <stdio.h>
#include <unistd.h>

#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {
// Returns an example PerfDataProto. The contents don't have to make sense. They
// just need to constitute a semantically valid protobuf.
// |proto| is an output parameter that will contain the created protobuf.
PerfDataProto GetExamplePerfDataProto() {
  PerfDataProto proto;
  proto.set_timestamp_sec(1435604013);  // Time since epoch in seconds.

  PerfDataProto_PerfFileAttr* file_attr = proto.add_file_attrs();
  file_attr->add_ids(61);
  file_attr->add_ids(62);
  file_attr->add_ids(63);

  PerfDataProto_PerfEventAttr* attr = file_attr->mutable_attr();
  attr->set_type(1);
  attr->set_size(2);
  attr->set_config(3);
  attr->set_sample_period(4);
  attr->set_sample_freq(5);

  PerfDataProto_PerfEventStats* stats = proto.mutable_stats();
  stats->set_num_events_read(100);
  stats->set_num_sample_events(200);
  stats->set_num_mmap_events(300);
  stats->set_num_fork_events(400);
  stats->set_num_exit_events(500);

  return proto;
}

// Perf session ID returned by the GetPerfOutputV2 DBus method call.
const uint64_t kFakePerfSssionId = 101;
// Quipper command line arguments for running perf.
const std::vector<std::string> kQuipperArgs{
    "--duration", "4",      "--", "perf", "record", "-a",
    "-e",         "cycles", "-g", "-c",   "4000037"};

// This fakes DebugDaemonClient by serving example perf data when the profiling
// duration elapses.
class FakeDebugDaemonClient : public ash::FakeDebugDaemonClient {
 public:
  FakeDebugDaemonClient()
      : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  FakeDebugDaemonClient(const FakeDebugDaemonClient&) = delete;
  FakeDebugDaemonClient& operator=(const FakeDebugDaemonClient&) = delete;

  ~FakeDebugDaemonClient() override {
    EXPECT_FALSE(perf_output_file_.IsValid());
  }

  void GetPerfOutput(const std::vector<std::string>& quipper_args,
                     bool disable_cpu_idle,
                     int file_descriptor,
                     chromeos::DBusMethodCallback<uint64_t> callback) override {
    // We will write perf output to this pipe FD. dup() |file_descriptor|
    // because it is closed after this method returns.
    base::ScopedPlatformFile perf_output_fd(HANDLE_EINTR(dup(file_descriptor)));
    ASSERT_NE(perf_output_fd, -1);

    perf_output_file_ = base::File(std::move(perf_output_fd));
    ASSERT_TRUE(perf_output_file_.IsValid());

    // Returns a fake perf session ID after calling GetPerfOutputFd.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](chromeos::DBusMethodCallback<uint64_t> callback) {
                         std::move(callback).Run(kFakePerfSssionId);
                       },
                       std::move(callback)));
  }

  bool stop_called() const { return stop_called_; }

  void StopPerf(uint64_t session_id,
                chromeos::VoidDBusMethodCallback callback) override {
    // Simulates stopping the perf session by writing perf data right away.
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    EXPECT_EQ(session_id, kFakePerfSssionId);
    stop_called_ = true;
    OnFakePerfOutputComplete();
  }

  // This simulates that profile collection is done and quipper writes perf data
  // over the pipe.
  void OnFakePerfOutputComplete() {
    base::ScopedAllowBlockingForTesting allow_block;

    auto perf_data = GetExamplePerfDataProto().SerializeAsString();
    EXPECT_TRUE(perf_output_file_.WriteAtCurrentPosAndCheck(
        base::as_byte_span(perf_data)));

    // Need to close the pipe to unblock the pipe reader.
    perf_output_file_.Close();
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::File perf_output_file_;
  chromeos::DBusMethodCallback<uint64_t> get_perf_outjput_callback_;
  bool stop_called_ = false;
};

}  // namespace

class PerfOutputCallTest : public testing::Test {
 public:
  PerfOutputCallTest() = default;

  PerfOutputCallTest(const PerfOutputCallTest&) = delete;
  PerfOutputCallTest& operator=(const PerfOutputCallTest&) = delete;

  void SetUp() override {
    debug_daemon_client_ = std::make_unique<FakeDebugDaemonClient>();
  }

  void TearDown() override { perf_output_call_.reset(); }

  void OnPerfOutputComplete(std::string perf_output) {
    perf_output_ = std::move(perf_output);
  }

 protected:
  // |task_environment_| must be the first member (or at least before
  // any member that cares about tasks) to be initialized first and
  // destroyed last.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<FakeDebugDaemonClient> debug_daemon_client_;
  std::unique_ptr<PerfOutputCall> perf_output_call_;

  std::string perf_output_;
};

// Test getting perf output after profile duration elapses.
TEST_F(PerfOutputCallTest, GetPerfOutput) {
  perf_output_call_ = std::make_unique<PerfOutputCall>(
      debug_daemon_client_.get(), kQuipperArgs, false,
      base::BindOnce(&PerfOutputCallTest::OnPerfOutputComplete,
                     base::Unretained(this)));
  // Not yet collected.
  EXPECT_EQ(perf_output_, "");

  // Perf data is available.
  debug_daemon_client_->OnFakePerfOutputComplete();

  // Note that we can call RunUntilIdle() only after fake perf data is written
  // over the pipe, or RunUntilIdle() will block forever on the reading end.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(debug_daemon_client_->stop_called());
  EXPECT_EQ(perf_output_, GetExamplePerfDataProto().SerializeAsString());
}

// Test stopping the perf session and get perf output right away.
TEST_F(PerfOutputCallTest, Stop) {
  perf_output_call_ = std::make_unique<PerfOutputCall>(
      debug_daemon_client_.get(), kQuipperArgs, false,
      base::BindOnce(&PerfOutputCallTest::OnPerfOutputComplete,
                     base::Unretained(this)));
  // Not yet collected.
  EXPECT_EQ(perf_output_, "");

  // Perf data is available after calling Stop().
  perf_output_call_->Stop();

  // Note that we can call RunUntilIdle() only after fake perf data is written
  // over the pipe, or RunUntilIdle() will block forever on the reading end.
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(debug_daemon_client_->stop_called());
  EXPECT_EQ(perf_output_, GetExamplePerfDataProto().SerializeAsString());
}

}  // namespace metrics
