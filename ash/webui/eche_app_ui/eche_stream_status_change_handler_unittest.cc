// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_stream_status_change_handler.h"
#include <memory>

#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {
namespace {

class TaskRunner {
 public:
  TaskRunner() = default;
  ~TaskRunner() = default;

  void WaitForResult() { run_loop_.Run(); }

  void Finish() { run_loop_.Quit(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
};

class FakeObserver : public EcheStreamStatusChangeHandler::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_start_streaming_calls() const {
    return num_start_streaming_calls_;
  }
  mojom::StreamStatus last_notified_stream_status() const {
    return last_notified_stream_status_;
  }

  // EcheStreamStatusChangeHandler::Observer:
  void OnStartStreaming() override { ++num_start_streaming_calls_; }
  void OnStreamStatusChanged(mojom::StreamStatus status) override {
    last_notified_stream_status_ = status;
  }

 private:
  size_t num_start_streaming_calls_ = 0;
  mojom::StreamStatus last_notified_stream_status_ =
      mojom::StreamStatus::kStreamStatusUnknown;
};

class FakeStreamActionObserver : public mojom::StreamActionObserver {
 public:
  FakeStreamActionObserver(
      mojo::PendingRemote<mojom::StreamActionObserver>* remote,
      TaskRunner* task_runner)
      : receiver_(this, remote->InitWithNewPipeAndPassReceiver()) {
    task_runner_ = task_runner;
  }
  ~FakeStreamActionObserver() override = default;

  size_t num_stream_action_state_calls() const {
    return num_stream_action_state_calls_;
  }

  // mojom::StreamActionObserver:
  void OnStreamAction(mojom::StreamAction action) override {
    ++num_stream_action_state_calls_;
    task_runner_->Finish();
  }

 private:
  size_t num_stream_action_state_calls_ = 0;
  raw_ptr<TaskRunner> task_runner_;
  mojo::Receiver<mojom::StreamActionObserver> receiver_;
};

}  // namespace

class EcheStreamStatusChangeHandlerTest : public testing::Test {
 protected:
  EcheStreamStatusChangeHandlerTest() = default;
  EcheStreamStatusChangeHandlerTest(const EcheStreamStatusChangeHandlerTest&) =
      delete;
  EcheStreamStatusChangeHandlerTest& operator=(
      const EcheStreamStatusChangeHandlerTest&) = delete;
  ~EcheStreamStatusChangeHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    connection_status_handler_ =
        std::make_unique<eche_app::EcheConnectionStatusHandler>();
    apps_launch_info_provider_ = std::make_unique<AppsLaunchInfoProvider>(
        connection_status_handler_.get());
    apps_launch_info_provider_->SetAppLaunchInfo(
        mojom::AppStreamLaunchEntryPoint::APPS_LIST);
    handler_ = std::make_unique<EcheStreamStatusChangeHandler>(
        apps_launch_info_provider_.get(), connection_status_handler_.get());
    handler_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    apps_launch_info_provider_.reset();
    handler_->RemoveObserver(&fake_observer_);
    handler_.reset();
    connection_status_handler_.reset();
  }

  void StartStreaming() { handler_->StartStreaming(); }
  void CloseStream() { handler_->CloseStream(); }
  void NotifyStreamStatus(mojom::StreamStatus status) {
    handler_->OnStreamStatusChanged(status);
  }

  void SetStreamActionObserver(
      mojo::PendingRemote<mojom::StreamActionObserver> observer) {
    handler_->SetStreamActionObserver(std::move(observer));
  }

  size_t GetNumObserverStartStreamingCalls() const {
    return fake_observer_.num_start_streaming_calls();
  }
  mojom::StreamStatus GetObservedStreamStatus() const {
    return fake_observer_.last_notified_stream_status();
  }

  TaskRunner task_runner_;

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<eche_app::EcheConnectionStatusHandler>
      connection_status_handler_;
  std::unique_ptr<AppsLaunchInfoProvider> apps_launch_info_provider_;
  std::unique_ptr<EcheStreamStatusChangeHandler> handler_;
};

TEST_F(EcheStreamStatusChangeHandlerTest, StartStreaming) {
  StartStreaming();
  EXPECT_EQ(1u, GetNumObserverStartStreamingCalls());
}

TEST_F(EcheStreamStatusChangeHandlerTest, CloseStream) {
  mojo::PendingRemote<mojom::StreamActionObserver> observer;
  FakeStreamActionObserver fake_stream_action_observer(&observer,
                                                       &task_runner_);
  SetStreamActionObserver(std::move(observer));

  CloseStream();
  task_runner_.WaitForResult();

  EXPECT_EQ(1u, fake_stream_action_observer.num_stream_action_state_calls());
}

TEST_F(EcheStreamStatusChangeHandlerTest, OnStreamStatusChanged) {
  NotifyStreamStatus(mojom::StreamStatus::kStreamStatusInitializing);
  EXPECT_EQ(mojom::StreamStatus::kStreamStatusInitializing,
            GetObservedStreamStatus());

  NotifyStreamStatus(mojom::StreamStatus::kStreamStatusStarted);
  EXPECT_EQ(mojom::StreamStatus::kStreamStatusStarted,
            GetObservedStreamStatus());

  NotifyStreamStatus(mojom::StreamStatus::kStreamStatusStopped);
  EXPECT_EQ(mojom::StreamStatus::kStreamStatusStopped,
            GetObservedStreamStatus());
}

}  // namespace eche_app
}  // namespace ash
