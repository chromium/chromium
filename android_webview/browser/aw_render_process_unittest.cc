// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_render_process.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {

class ChannelBackedRenderProcessHost : public content::MockRenderProcessHost {
 public:
  ChannelBackedRenderProcessHost(content::BrowserContext* browser_context,
                                 IPC::ChannelProxy* channel)
      : content::MockRenderProcessHost(browser_context), channel_(channel) {}

  IPC::ChannelProxy* GetChannel() override { return channel_; }

 private:
  raw_ptr<IPC::ChannelProxy> channel_;
};

// TODO(crbug.com/588343902): Remove or update once Java AwRenderProcess handle
// lifetime is decoupled from RenderProcessHost UserData ownership.
class AwRenderProcessTest : public testing::Test {
 protected:
  void SetUp() override {
    server_channel_ = IPC::ChannelProxy::Create(
        std::move(pipe_.handle0), IPC::Channel::MODE_SERVER,
        /*listener=*/nullptr, content::GetIOThreadTaskRunner({}),
        content::GetUIThreadTaskRunner({}));
    render_process_host_ = std::make_unique<ChannelBackedRenderProcessHost>(
        &browser_context_, server_channel_.get());
    ASSERT_TRUE(render_process_host_->Init());
  }

  void TearDown() override {
    render_process_host_.reset();
    server_channel_->Close();
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  mojo::MessagePipe pipe_;
  std::unique_ptr<IPC::ChannelProxy> server_channel_;
  std::unique_ptr<ChannelBackedRenderProcessHost> render_process_host_;
};

TEST_F(AwRenderProcessTest,
       UnusedProcessSurvivesProcessExitAndCleansUpOnHostDestroyed) {
  ASSERT_TRUE(AwRenderProcess::IsUnused(render_process_host_.get()));

  AwRenderProcess* aw_render_process =
      AwRenderProcess::GetInstanceForRenderProcessHost(
          render_process_host_.get());
  ASSERT_NE(aw_render_process, nullptr);
  render_process_host_->SimulateReady();

  render_process_host_->SimulateRenderProcessExit(
      base::TERMINATION_STATUS_NORMAL_TERMINATION, 0);

  // Destroying the RenderProcessHost after an unused process exit must
  // unregister the observer cleanly without tripping
  // DCHECK(!IsInObserverList()) in ~RenderProcessHostObserver().
  render_process_host_->Cleanup();
  render_process_host_.reset();
}

TEST_F(AwRenderProcessTest, HostDestroyedWithoutProcessExitCleansUpObserver) {
  AwRenderProcess* aw_render_process =
      AwRenderProcess::GetInstanceForRenderProcessHost(
          render_process_host_.get());
  ASSERT_NE(aw_render_process, nullptr);
  render_process_host_->SimulateReady();

  // Directly destroying the RenderProcessHost (e.g. --single-process shutdown
  // or MockRenderProcessHost teardown without RenderProcessExited) must remove
  // the observer cleanly before ClearAllUserData() runs.
  render_process_host_.reset();
}

}  // namespace
}  // namespace android_webview
