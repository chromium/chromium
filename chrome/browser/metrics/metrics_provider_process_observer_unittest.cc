// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_provider_process_observer.h"

#include <string_view>
#include <vector>

#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

class TestDelegate : public MetricsProviderProcessObserver::Delegate {
 public:
  struct ListenCall {
    content::ChildProcessId content_id;
    base::ProcessId pid;
    std::string_view process_type_suffix;
  };

  void StartListeningToProcess(content::ChildProcessId content_id,
                               base::ProcessId pid,
                               std::string_view process_type_suffix) override {
    start_calls_.push_back({content_id, pid, process_type_suffix});
  }

  void StopListeningToProcess(content::ChildProcessId content_id) override {
    stop_calls_.push_back(content_id);
  }

  const std::vector<ListenCall>& start_calls() const { return start_calls_; }
  const std::vector<content::ChildProcessId>& stop_calls() const {
    return stop_calls_;
  }

 private:
  std::vector<ListenCall> start_calls_;
  std::vector<content::ChildProcessId> stop_calls_;
};

class MetricsProviderProcessObserverTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TestDelegate delegate_;
};

TEST_F(MetricsProviderProcessObserverTest,
       ObservesBrowserProcessOnConstruction) {
  MetricsProviderProcessObserver observer(&delegate_,
                                          /*downsampling_factor=*/1);
  ASSERT_EQ(delegate_.start_calls().size(), 1u);
  EXPECT_EQ(delegate_.start_calls()[0].content_id,
            static_cast<content::ChildProcessId>(
                base::GetUniqueIdForProcess().GetUnsafeValue()));
  EXPECT_EQ(delegate_.start_calls()[0].pid, base::GetCurrentProcId());
  EXPECT_EQ(delegate_.start_calls()[0].process_type_suffix, "Browser");
}

TEST_F(MetricsProviderProcessObserverTest, ObservesBrowserChildProcesses) {
  MetricsProviderProcessObserver observer(&delegate_,
                                          /*downsampling_factor=*/1);
  ASSERT_EQ(delegate_.start_calls().size(), 1u);

  // Simulate GPU process launch.
  content::ChildProcessData gpu_data(content::PROCESS_TYPE_GPU,
                                     content::ChildProcessId(10));
  gpu_data.SetProcess(base::Process::Current());
  observer.BrowserChildProcessLaunchedAndConnected(gpu_data);

  ASSERT_EQ(delegate_.start_calls().size(), 2u);
  EXPECT_EQ(delegate_.start_calls()[1].content_id, content::ChildProcessId(10));
  EXPECT_EQ(delegate_.start_calls()[1].process_type_suffix, "Gpu");

  // Simulate NetworkService utility process launch.
  content::ChildProcessData net_data(content::PROCESS_TYPE_UTILITY,
                                     content::ChildProcessId(11));
  net_data.metrics_name = "network.mojom.NetworkService";
  net_data.SetProcess(base::Process::Current());
  observer.BrowserChildProcessLaunchedAndConnected(net_data);

  ASSERT_EQ(delegate_.start_calls().size(), 3u);
  EXPECT_EQ(delegate_.start_calls()[2].content_id, content::ChildProcessId(11));
  EXPECT_EQ(delegate_.start_calls()[2].process_type_suffix, "NetworkService");

  // Simulate process disconnect.
  observer.BrowserChildProcessHostDisconnected(gpu_data);
  ASSERT_EQ(delegate_.stop_calls().size(), 1u);
  EXPECT_EQ(delegate_.stop_calls()[0], content::ChildProcessId(10));
}

TEST_F(MetricsProviderProcessObserverTest, ObservesRenderProcesses) {
  MetricsProviderProcessObserver observer(&delegate_,
                                          /*downsampling_factor=*/1);

  content::MockRenderProcessHost rph(&profile_);
  rph.SetProcess(base::Process::Current());

  observer.OnRenderProcessHostCreated(&rph);
  rph.SimulateReady();

  ASSERT_EQ(delegate_.start_calls().size(), 2u);
  EXPECT_EQ(delegate_.start_calls()[1].content_id, rph.GetID());
  EXPECT_EQ(delegate_.start_calls()[1].process_type_suffix, "Renderer");

  content::ChildProcessTerminationInfo info;
  observer.RenderProcessExited(&rph, info);
  ASSERT_EQ(delegate_.stop_calls().size(), 1u);
  EXPECT_EQ(delegate_.stop_calls()[0], rph.GetID());
}

}  // namespace
}  // namespace metrics
