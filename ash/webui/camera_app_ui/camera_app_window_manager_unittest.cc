// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_window_manager.h"

#include <queue>

#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using testing::_;
using testing::Invoke;
using CameraUsageOwnershipMonitor =
    camera_app::mojom::CameraUsageOwnershipMonitor;
using OnCameraUsageOwnershipChangedCallback = camera_app::mojom::
    CameraUsageOwnershipMonitor::OnCameraUsageOwnershipChangedCallback;

class MockCameraUsageOwnershipMonitor
    : public camera_app::mojom::CameraUsageOwnershipMonitor {
 public:
  MockCameraUsageOwnershipMonitor() : receiver_(this) {}

  void Bind(mojo::PendingReceiver<CameraUsageOwnershipMonitor> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void Reset() { receiver_.reset(); }

  void OnCameraUsageOwnershipChanged(
      bool has_usage,
      OnCameraUsageOwnershipChangedCallback callback) override {
    DoOnCameraUsageOwnershipChanged(has_usage, std::move(callback));
  }
  MOCK_METHOD(void,
              DoOnCameraUsageOwnershipChanged,
              (bool has_usage, OnCameraUsageOwnershipChangedCallback callback));

 private:
  mojo::Receiver<camera_app::mojom::CameraUsageOwnershipMonitor> receiver_;
};

}  // namespace

class CameraAppWindowManagerTest : public views::ViewsTestBase {
 public:
  struct MockApp {
    std::unique_ptr<views::Widget> widget;
    testing::StrictMock<MockCameraUsageOwnershipMonitor> ownership_monitor;
  };

  CameraAppWindowManagerTest()
      : views::ViewsTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                content::BrowserTaskEnvironment::MainThreadType::UI,
                content::BrowserTaskEnvironment::TimeSource::MOCK_TIME))) {}
  CameraAppWindowManagerTest(const CameraAppWindowManagerTest&) = delete;
  CameraAppWindowManagerTest& operator=(const CameraAppWindowManagerTest&) =
      delete;
  ~CameraAppWindowManagerTest() override = default;

  void BindMonitor(MockApp* mock_app) {
    mojo::PendingRemote<CameraUsageOwnershipMonitor> monitor_remote;
    mock_app->ownership_monitor.Bind(
        monitor_remote.InitWithNewPipeAndPassReceiver());
    app_window_manager.SetCameraUsageMonitor(
        mock_app->widget->GetNativeWindow(), std::move(monitor_remote),
        base::DoNothing());
  }

  // The activation observer will not be triggered if the focus hasn't been
  // moved to other widget. It should not happen in the real scenario so for
  // testing, we simply focus on some random target and then focus back.
  void FocusOnOtherWidget() {
    auto some_other_widget = CreateTestWidget();
    some_other_widget->Activate();
  }

  void Ack(bool has_usage, OnCameraUsageOwnershipChangedCallback callback) {
    std::move(callback).Run();
  }

  void Drop(bool has_usage, OnCameraUsageOwnershipChangedCallback callback) {
    dropped_callbacks_.push(std::move(callback));
  }

  void PutCallbackPending(bool has_usage,
                          OnCameraUsageOwnershipChangedCallback callback) {
    pending_callbacks_.push(std::move(callback));
  }

  void ConsumePendingCallbacks() {
    while (!pending_callbacks_.empty()) {
      std::move(pending_callbacks_.front()).Run();
      pending_callbacks_.pop();
    }
  }

 protected:
  CameraAppWindowManager app_window_manager;

 private:
  std::queue<OnCameraUsageOwnershipChangedCallback> pending_callbacks_;

  // Put the callbacks to be dropped in a queue to avoid check failure.
  std::queue<OnCameraUsageOwnershipChangedCallback> dropped_callbacks_;
};

// Test that when a window lost visibility / activated, the camera usage is
// suspended/resumed.
TEST_F(CameraAppWindowManagerTest, LostVisibilityAndActivated) {
  MockApp mock_app = {CreateTestWidget()};
  auto* widget = mock_app.widget.get();
  auto& monitor = mock_app.ownership_monitor;

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(true, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Ack));
  widget->Show();
  BindMonitor(&mock_app);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(false, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Ack));
  widget->Hide();
  base::RunLoop().RunUntilIdle();

  FocusOnOtherWidget();

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(true, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Ack));
  widget->Show();
  base::RunLoop().RunUntilIdle();
}

// Test that when a window lost visibility when the camera usage is resuming,
// the camera usage should be suspended in the end.
TEST_F(CameraAppWindowManagerTest, LostVisibilityWhenResuming) {
  MockApp mock_app = {CreateTestWidget()};
  auto* widget = mock_app.widget.get();
  auto& monitor = mock_app.ownership_monitor;

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(true, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::PutCallbackPending));
  widget->Show();
  BindMonitor(&mock_app);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(false, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Ack));
  widget->Hide();
  ConsumePendingCallbacks();
  base::RunLoop().RunUntilIdle();
}

// Test that when a window is activated when the camera usage is suspending, the
// camera usage should be resumed in the end.
TEST_F(CameraAppWindowManagerTest, ActivateWhenSuspending) {
  MockApp mock_app = {CreateTestWidget()};
  auto* widget = mock_app.widget.get();
  auto& monitor = mock_app.ownership_monitor;

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(true, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Ack));
  widget->Show();
  BindMonitor(&mock_app);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(false, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::PutCallbackPending));
  widget->Hide();
  base::RunLoop().RunUntilIdle();

  FocusOnOtherWidget();

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(true, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Ack));
  widget->Show();
  ConsumePendingCallbacks();
  base::RunLoop().RunUntilIdle();
}

// Test that when a new window is launched, it can get the camera usage.
TEST_F(CameraAppWindowManagerTest, MultipleWindows) {
  MockApp mock_app1 = {CreateTestWidget()};
  auto* widget = mock_app1.widget.get();
  auto& monitor = mock_app1.ownership_monitor;

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(true, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Ack));
  widget->Show();
  BindMonitor(&mock_app1);
  base::RunLoop().RunUntilIdle();

  // The original owner should suspend first.
  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(false, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::PutCallbackPending));
  MockApp mock_app2 = {CreateTestWidget()};
  mock_app2.widget->Show();
  BindMonitor(&mock_app2);
  base::RunLoop().RunUntilIdle();

  // And the new owner should get the ownership afterwards.
  EXPECT_CALL(mock_app2.ownership_monitor,
              DoOnCameraUsageOwnershipChanged(true, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Ack));
  ConsumePendingCallbacks();
  base::RunLoop().RunUntilIdle();
}

// Test that when a window is unexpectedly closed during transferring camera
// usage, the next window is still able to grant the camera usage.
TEST_F(CameraAppWindowManagerTest, UnexpectedlyClosed) {
  MockApp mock_app1 = {CreateTestWidget()};
  auto* widget = mock_app1.widget.get();
  auto& monitor = mock_app1.ownership_monitor;

  EXPECT_CALL(monitor, DoOnCameraUsageOwnershipChanged(true, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Drop));
  widget->Show();
  BindMonitor(&mock_app1);
  base::RunLoop().RunUntilIdle();

  // Close the app without sending ack that the camera usage is resumed.
  monitor.Reset();

  // Check that newly instantiated app is still able to get the camera
  // ownership although the previous one is unexpectedly closed.
  MockApp mock_app2 = {CreateTestWidget()};
  EXPECT_CALL(mock_app2.ownership_monitor,
              DoOnCameraUsageOwnershipChanged(true, _))
      .WillOnce(Invoke(this, &CameraAppWindowManagerTest::Ack));
  mock_app2.widget->Show();
  BindMonitor(&mock_app2);
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
