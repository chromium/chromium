// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/common/mac/app_shim_param_traits.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestingAppShim : public chrome::mojom::AppShim {
 public:
  TestingAppShim() {}

  chrome::mojom::AppShimHostBootstrap::LaunchAppCallback
  GetLaunchAppCallback() {
    return base::BindOnce(&TestingAppShim::LaunchAppDone,
                          base::Unretained(this));
  }
  chrome::mojom::AppShimHostBootstrapRequest GetHostBootstrapRequest() {
    return mojo::MakeRequest(&host_bootstrap_ptr_);
  }

  apps::AppShimLaunchResult GetLaunchResult() const {
    EXPECT_TRUE(received_launch_done_result_);
    return launch_done_result_;
  }

 private:
  void LaunchAppDone(apps::AppShimLaunchResult result,
                     chrome::mojom::AppShimRequest app_shim_request) {
    received_launch_done_result_ = true;
    launch_done_result_ = result;
  }

  // chrome::mojom::AppShim implementation.
  void CreateViewsBridgeFactory(
      views_bridge_mac::mojom::BridgeFactoryAssociatedRequest request)
      override {}
  void CreateContentNSViewBridgeFactory(
      content::mojom::NSViewBridgeFactoryAssociatedRequest request) override {}
  void Hide() override {}
  void UnhideWithoutActivation() override {}
  void SetUserAttention(apps::AppShimAttentionType attention_type) override {}

  bool received_launch_done_result_ = false;
  apps::AppShimLaunchResult launch_done_result_ = apps::APP_SHIM_LAUNCH_SUCCESS;

  chrome::mojom::AppShimHostBootstrapPtr host_bootstrap_ptr_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShim);
};

class TestingAppShimHostBootstrap : public AppShimHostBootstrap {
 public:
  explicit TestingAppShimHostBootstrap(
      chrome::mojom::AppShimHostBootstrapRequest host_request)
      : test_weak_factory_(this) {
    // AppShimHost will bind to the request from ServeChannel. For testing
    // purposes, have this request passed in at creation.
    host_bootstrap_binding_.Bind(std::move(host_request));
  }

  base::WeakPtr<TestingAppShimHostBootstrap> GetWeakPtr() {
    return test_weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestingAppShimHostBootstrap> test_weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShimHostBootstrap);
};

const char kTestAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestProfileDir[] = "Profile 1";

class AppShimHostTest : public testing::Test,
                        public apps::AppShimHandler {
 public:
  AppShimHostTest() { task_runner_ = base::ThreadTaskRunnerHandle::Get(); }

  ~AppShimHostTest() override {
    if (host_)
      delete host_.get();
    DCHECK(!host_);
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }
  TestingAppShimHostBootstrap* host() { return host_.get(); }
  chrome::mojom::AppShimHostBootstrap* GetBootstrapMojoHost() {
    return host_.get();
  }
  chrome::mojom::AppShimHost* GetMojoHost() { return host_ptr_.get(); }

  void LaunchApp(apps::AppShimLaunchType launch_type) {
    GetBootstrapMojoHost()->LaunchApp(
        mojo::MakeRequest(&host_ptr_), base::FilePath(kTestProfileDir),
        kTestAppId, launch_type, std::vector<base::FilePath>(),
        shim_->GetLaunchAppCallback());
  }

  apps::AppShimLaunchResult GetLaunchResult() {
    RunUntilIdle();
    return shim_->GetLaunchResult();
  }

  void SimulateDisconnect() { host_ptr_.reset(); }

 protected:
  void OnShimLaunch(Host* host,
                    apps::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& file) override {
    ++launch_count_;
    if (launch_type == apps::APP_SHIM_LAUNCH_NORMAL)
      ++launch_now_count_;
    host->OnAppLaunchComplete(launch_result_);
  }

  void OnShimClose(Host* host) override { ++close_count_; }

  void OnShimFocus(Host* host,
                   apps::AppShimFocusType focus_type,
                   const std::vector<base::FilePath>& file) override {
    ++focus_count_;
  }

  void OnShimSetHidden(Host* host, bool hidden) override {}

  void OnShimQuit(Host* host) override { ++quit_count_; }

  apps::AppShimLaunchResult launch_result_ = apps::APP_SHIM_LAUNCH_SUCCESS;
  int launch_count_ = 0;
  int launch_now_count_ = 0;
  int close_count_ = 0;
  int focus_count_ = 0;
  int quit_count_ = 0;

 private:
  void SetUp() override {
    testing::Test::SetUp();
    shim_.reset(new TestingAppShim());
    TestingAppShimHostBootstrap* host =
        new TestingAppShimHostBootstrap(shim_->GetHostBootstrapRequest());
    host_ = host->GetWeakPtr();
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<TestingAppShim> shim_;

  // AppShimHost will destroy itself in AppShimHost::Close, so use a weak
  // pointer here to avoid lifetime issues.
  base::WeakPtr<TestingAppShimHostBootstrap> host_;
  chrome::mojom::AppShimHostPtr host_ptr_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostTest);
};


}  // namespace

TEST_F(AppShimHostTest, TestLaunchAppWithHandler) {
  apps::AppShimHandler::RegisterHandler(kTestAppId, this);
  LaunchApp(apps::APP_SHIM_LAUNCH_NORMAL);
  EXPECT_EQ(kTestAppId, host()->GetHostForTesting()->GetAppId());
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_SUCCESS, GetLaunchResult());
  EXPECT_EQ(1, launch_count_);
  EXPECT_EQ(1, launch_now_count_);
  EXPECT_EQ(0, focus_count_);
  EXPECT_EQ(0, close_count_);

  // A second OnAppLaunchComplete is ignored.
  host()->GetHostForTesting()->OnAppLaunchComplete(
      apps::APP_SHIM_LAUNCH_APP_NOT_FOUND);
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_SUCCESS, GetLaunchResult());

  GetMojoHost()->FocusApp(apps::APP_SHIM_FOCUS_NORMAL,
                          std::vector<base::FilePath>());
  RunUntilIdle();
  EXPECT_EQ(1, focus_count_);

  GetMojoHost()->QuitApp();
  RunUntilIdle();
  EXPECT_EQ(1, quit_count_);

  SimulateDisconnect();
  RunUntilIdle();
  EXPECT_EQ(1, close_count_);
  EXPECT_EQ(nullptr, host());
  apps::AppShimHandler::RemoveHandler(kTestAppId);
}

TEST_F(AppShimHostTest, TestNoLaunchNow) {
  apps::AppShimHandler::RegisterHandler(kTestAppId, this);
  LaunchApp(apps::APP_SHIM_LAUNCH_REGISTER_ONLY);
  EXPECT_EQ(kTestAppId, host()->GetHostForTesting()->GetAppId());
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_SUCCESS, GetLaunchResult());
  EXPECT_EQ(1, launch_count_);
  EXPECT_EQ(0, launch_now_count_);
  EXPECT_EQ(0, focus_count_);
  EXPECT_EQ(0, close_count_);
  apps::AppShimHandler::RemoveHandler(kTestAppId);
}

TEST_F(AppShimHostTest, TestFailLaunch) {
  apps::AppShimHandler::RegisterHandler(kTestAppId, this);
  launch_result_ = apps::APP_SHIM_LAUNCH_APP_NOT_FOUND;
  LaunchApp(apps::APP_SHIM_LAUNCH_NORMAL);
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_APP_NOT_FOUND, GetLaunchResult());
  apps::AppShimHandler::RemoveHandler(kTestAppId);
}
