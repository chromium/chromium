// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/timer/arc_timer_bridge.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ash/components/arc/mojom/timer.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_timer_instance.h"
#include "ash/components/arc/timer/arc_timer_mojom_traits.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/unix_domain_socket.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

// Converts a system file descriptor to a mojo handle that can be sent to the
// host.
mojo::ScopedHandle WrapPlatformFd(base::ScopedFD scoped_fd) {
  mojo::ScopedHandle handle = mojo::WrapPlatformFile(std::move(scoped_fd));
  if (!handle.is_valid()) {
    LOG(ERROR) << "Failed to wrap platform handle";
    return mojo::ScopedHandle();
  }
  return handle;
}

// Callback for D-Bus operations.
void TimerOperationCallback(base::OnceClosure quit_callback,
                            bool* op_result,
                            mojom::ArcTimerResult result) {
  *op_result = (result == mojom::ArcTimerResult::SUCCESS);
  std::move(quit_callback).Run();
}

// Stores clock ids and their corresponding file descriptors. These file
// descriptors indicate when a timer corresponding to the clock has expired on
// a read.
class ArcTimerStore {
 public:
  ArcTimerStore() = default;

  ArcTimerStore(const ArcTimerStore&) = delete;
  ArcTimerStore& operator=(const ArcTimerStore&) = delete;

  bool AddTimer(clockid_t clock_id, base::ScopedFD read_fd) {
    return arc_timers_.emplace(clock_id, std::move(read_fd)).second;
  }

  void ClearTimers() { return arc_timers_.clear(); }

  std::optional<int> GetTimerReadFd(clockid_t clock_id) {
    if (!HasTimer(clock_id))
      return std::nullopt;
    return std::optional<int>(arc_timers_[clock_id].get());
  }

  bool HasTimer(clockid_t clock_id) const {
    auto it = arc_timers_.find(clock_id);
    return it != arc_timers_.end() && it->second.is_valid();
  }

 private:
  // Map of a clock id to read fd that is signalled when the timer corresponding
  // the clock expires.
  std::map<clockid_t, base::ScopedFD> arc_timers_;
};

class ArcTimerTest : public testing::Test {
 public:
  ArcTimerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
    ash::UpstartClient::InitializeFake();
    chromeos::PowerManagerClient::InitializeFake();
    timer_bridge_ = ArcTimerBridge::GetForBrowserContextForTesting(&context_);
    // This results in ArcTimerBridge::OnInstanceReady being called.
    ArcServiceManager::Get()->arc_bridge_service()->timer()->SetInstance(
        &timer_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->timer());
  }

  ArcTimerTest(const ArcTimerTest&) = delete;
  ArcTimerTest& operator=(const ArcTimerTest&) = delete;

  ~ArcTimerTest() override {
    // Destroys the FakeTimerInstance. This results in
    // ArcTimerBridge::OnInstanceClosed being called.
    ArcServiceManager::Get()->arc_bridge_service()->timer()->CloseInstance(
        &timer_instance_);
    timer_bridge_->Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    ash::UpstartClient::Shutdown();
  }

 protected:
  // Returns true iff timer creation of each clock type succeeded.
  bool CreateTimers(const std::vector<clockid_t>& clocks);

  // Returns true iff a timer for |clock_id| is successfully scheduled.
  bool StartTimer(clockid_t clock_id, base::TimeTicks absolute_expiration_time);

  // Returns true iff the read descriptor of a timer is signalled. If the
  // signalling is incorrect returns false. Blocks otherwise.
  bool WaitForExpiration(clockid_t clock_id);

  mojom::TimerHost* GetTimerHost() { return timer_instance_.GetTimerHost(); }

 private:
  // Stores |read_fds| corresponding to clock ids in |clocks| in
  // |arc_timer_store_|.
  bool StoreReadFds(const std::vector<clockid_t>& clocks,
                    std::vector<base::ScopedFD> read_fds);

  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  FakeTimerInstance timer_instance_;

  ArcTimerStore arc_timer_store_;

  raw_ptr<ArcTimerBridge> timer_bridge_;
};

bool ArcTimerTest::StoreReadFds(const std::vector<clockid_t>& clocks,
                                std::vector<base::ScopedFD> read_fds) {
  auto read_fd_iter = read_fds.begin();
  for (auto clock_id : clocks) {
    // This should never fail because at this point timers have been created by
    // powerd and |clocks| doesn't have any duplicate clock ids.
    if (!arc_timer_store_.AddTimer(clock_id, std::move(*read_fd_iter))) {
      LOG(ERROR) << "Error while adding clock=" << clock_id << " to store";
      arc_timer_store_.ClearTimers();
      return false;
    }
    read_fd_iter++;
  }
  return true;
}

bool ArcTimerTest::CreateTimers(const std::vector<clockid_t>& clocks) {
  // Create requests to create a timer for each clock.
  std::vector<mojom::CreateTimerRequestPtr> arc_timer_requests;
  std::vector<base::ScopedFD> read_fds;
  for (auto clock_id : clocks) {
    mojom::CreateTimerRequestPtr request = mojom::CreateTimerRequest::New();
    // Create a socket pair for each clock. One socket will be part of the
    // mojo argument and will be used by the host to indicate when the timer
    // expires. The other socket will be used to detect the expiration of the
    // timer by epolling and reading.
    base::ScopedFD read_fd;
    base::ScopedFD write_fd;
    if (!base::CreateSocketPair(&read_fd, &write_fd)) {
      LOG(ERROR) << "Failed to create socket pair for ARC timers";
      return false;
    }
    request->clock_id = clock_id;
    request->expiration_fd = WrapPlatformFd(std::move(write_fd));
    arc_timer_requests.emplace_back(std::move(request));

    read_fds.emplace_back(std::move(read_fd));
  }

  // Clear local test state before creating timers.
  arc_timer_store_.ClearTimers();

  // Call the host to create timers. Safe to use base::Unretained(this) as the
  // class is guaranteed to exist for the duration of the test.
  bool result;
  base::RunLoop loop;

  timer_instance_.GetTimerHost()->CreateTimers(
      std::move(arc_timer_requests),
      base::BindOnce(&TimerOperationCallback, loop.QuitClosure(), &result));
  loop.Run();
  if (!result)
    return false;

  // If timer creation succeeded, store the read fds associated with each clock
  // in the store. The read fd will be used to wait on for a timer expiration.
  if (!StoreReadFds(clocks, std::move(read_fds))) {
    return false;
  }

  return true;
}

bool ArcTimerTest::StartTimer(clockid_t clock_id,
                              base::TimeTicks absolute_expiration_time) {
  // Call the host to start a timer corresponding to |clock_id|. Safe to use
  // base::Unretained(this) as the class is guaranteed to exist for the
  // duration of the test.
  base::RunLoop loop;
  bool result;
  timer_instance_.GetTimerHost()->StartTimer(
      clock_id, absolute_expiration_time,
      base::BindOnce(&TimerOperationCallback, loop.QuitClosure(), &result));
  loop.Run();
  return result;
}

bool ArcTimerTest::WaitForExpiration(clockid_t clock_id) {
  if (!arc_timer_store_.HasTimer(clock_id)) {
    LOG(ERROR) << "Timer of clock=" << clock_id << " not present";
    return false;
  }

  // Wait for the host to indicate expiration by watching the read end of the
  // socket pair.
  std::optional<int> timer_read_fd_opt =
      arc_timer_store_.GetTimerReadFd(clock_id);
  // This should never happen if the timer was present in the store.
  if (!timer_read_fd_opt.has_value()) {
    ADD_FAILURE() << "Clock=" << clock_id << " read fd not found";
    return false;
  }
  int timer_read_fd = timer_read_fd_opt.value();
  base::RunLoop loop;
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      watch_readable_controller = base::FileDescriptorWatcher::WatchReadable(
          timer_read_fd, loop.QuitClosure());
  loop.Run();

  // The timer expects 8 bytes to be written from the host upon expiration and
  // the number of expirations to be 1.
  uint64_t num_expirations;
  std::vector<base::ScopedFD> fds;
  ssize_t bytes_read = base::UnixDomainSocket::RecvMsg(
      timer_read_fd, &num_expirations, sizeof(num_expirations), &fds);
  if (bytes_read < static_cast<ssize_t>(sizeof(num_expirations))) {
    LOG(ERROR) << "Incorrect timer wake up bytes_read=" << bytes_read;
    return false;
  }
  EXPECT_EQ(num_expirations, 1ULL);

  // TODO(fdoray): Remove this hack once WatchReadable fixes crbug.com/74118.
  // This is required for |watch_readable_controller| to clean up properly.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  return true;
}

TEST_F(ArcTimerTest, StartTimerTest) {
  std::vector<clockid_t> clocks = {CLOCK_REALTIME_ALARM, CLOCK_BOOTTIME_ALARM};
  // Create timers before starting it.
  EXPECT_TRUE(CreateTimers(clocks));
  // Start timer and check if timer expired.
  base::TimeDelta delay = base::Milliseconds(20);
  EXPECT_TRUE(StartTimer(CLOCK_BOOTTIME_ALARM, base::TimeTicks::Now() + delay));
  EXPECT_TRUE(WaitForExpiration(CLOCK_BOOTTIME_ALARM));
}

TEST_F(ArcTimerTest, InvalidCreateTimersArgsTest) {
  std::vector<clockid_t> clocks = {CLOCK_REALTIME_ALARM, CLOCK_BOOTTIME_ALARM,
                                   CLOCK_BOOTTIME_ALARM};
  // Timers with duplicate clock ids shouldn't succeed.
  EXPECT_FALSE(CreateTimers(clocks));
}

TEST_F(ArcTimerTest, InvalidStartTimerArgsTest) {
  std::vector<clockid_t> clocks = {CLOCK_REALTIME_ALARM};
  EXPECT_TRUE(CreateTimers(clocks));
  // Start timer should fail due to un-registered clock id.
  base::TimeDelta delay = base::Milliseconds(20);
  EXPECT_FALSE(
      StartTimer(CLOCK_BOOTTIME_ALARM, base::TimeTicks::Now() + delay));
}

TEST_F(ArcTimerTest, CheckMultipleCreateTimersTest) {
  std::vector<clockid_t> clocks = {CLOCK_REALTIME_ALARM};
  EXPECT_TRUE(CreateTimers(clocks));
  // The power manager implicitly deletes old timers associated with a tag
  // during a create call. Thus, consecutive create calls should succeed.
  EXPECT_TRUE(CreateTimers(clocks));
}

TEST_F(ArcTimerTest, SetTimeTest_RequestedTimeIsInvalid) {
  // Time::Now() + 25 hours should be rejected.
  base::Time time_to_set =
      base::Time::Now() + kArcSetTimeMaxTimeDelta + base::Hours(1);
  base::test::TestFuture<mojom::ArcTimerResult> future;
  GetTimerHost()->SetTime(time_to_set, future.GetCallback());
  EXPECT_EQ(future.Get(), mojom::ArcTimerResult::FAILURE);

  // Time::Now() - 25 hours should be rejected.
  time_to_set = base::Time::Now() - kArcSetTimeMaxTimeDelta - base::Hours(1);
  base::test::TestFuture<mojom::ArcTimerResult> future2;
  GetTimerHost()->SetTime(time_to_set, future2.GetCallback());
  EXPECT_EQ(future2.Get(), mojom::ArcTimerResult::FAILURE);
}

TEST_F(ArcTimerTest, SetTimeTest_RequestedTimeIsValid) {
  // Time::Now() + 23 hours should be accepted.
  const base::Time time_to_set =
      base::Time::Now() + kArcSetTimeMaxTimeDelta - base::Hours(1);

  ash::FakeUpstartClient::Get()->set_start_job_cb(base::BindLambdaForTesting(
      [time_to_set](const std::string& job_name,
                    const std::vector<std::string>& env) {
        EXPECT_EQ(job_name, kArcSetTimeJobName);
        EXPECT_EQ(env.size(), 1U);  // Can't use ASSERT_EQ inside the closure.
        if (env.size() >= 1) {
          EXPECT_EQ(env[0], base::StringPrintf("UNIXTIME_TO_SET=%ld",
                                               time_to_set.ToTimeT()));
        }
        return ash::FakeUpstartClient::StartJobResult(true /* success */);
      }));

  base::test::TestFuture<mojom::ArcTimerResult> future;
  GetTimerHost()->SetTime(time_to_set, future.GetCallback());
  EXPECT_EQ(future.Get(), mojom::ArcTimerResult::SUCCESS);
}

TEST_F(ArcTimerTest, SetTimeTest_UpstartJobFails) {
  const base::Time time_to_set =
      base::Time::Now() + kArcSetTimeMaxTimeDelta - base::Hours(1);

  ash::FakeUpstartClient::Get()->set_start_job_cb(base::BindRepeating(
      [](const std::string& job_name, const std::vector<std::string>& env) {
        // Upstart job fails.
        return ash::FakeUpstartClient::StartJobResult(false /* success */);
      }));

  base::test::TestFuture<mojom::ArcTimerResult> future;
  GetTimerHost()->SetTime(time_to_set, future.GetCallback());
  EXPECT_EQ(future.Get(), mojom::ArcTimerResult::FAILURE);
}

}  // namespace

}  // namespace arc
