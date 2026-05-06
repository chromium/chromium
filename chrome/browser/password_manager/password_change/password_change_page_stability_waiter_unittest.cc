// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_page_stability_waiter.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

namespace {

using ::testing::_;

using page_content_annotations::mojom::PageStabilityMonitor;

class MockPageStabilityMonitor : public PageStabilityMonitor {
 public:
  MockPageStabilityMonitor() = default;
  ~MockPageStabilityMonitor() override = default;

  MOCK_METHOD(void,
              NotifyWhenStable,
              (base::TimeDelta, NotifyWhenStableCallback),
              (override));

  void Bind(mojo::PendingReceiver<PageStabilityMonitor> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void Close() { receiver_.reset(); }

 private:
  mojo::Receiver<PageStabilityMonitor> receiver_{this};
};

class MockChromeRenderFrame : public chrome::mojom::ChromeRenderFrame {
 public:
  MockChromeRenderFrame() = default;
  ~MockChromeRenderFrame() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(
        this, mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>(
                  std::move(handle)));
  }

  MOCK_METHOD(void,
              SetWindowFeatures,
              (blink::mojom::WindowFeaturesPtr),
              (override));
  MOCK_METHOD(void, RequestReloadImageForContextNode, (), (override));
  MOCK_METHOD(void,
              RequestBitmapForContextNode,
              (RequestBitmapForContextNodeCallback),
              (override));
  MOCK_METHOD(void,
              RequestBitmapForContextNodeWithBoundsHint,
              (RequestBitmapForContextNodeWithBoundsHintCallback),
              (override));
  MOCK_METHOD(void,
              RequestBoundsHintForAllImages,
              (RequestBoundsHintForAllImagesCallback),
              (override));
  MOCK_METHOD(void,
              RequestImageForContextNode,
              (int32_t,
               const gfx::Size&,
               chrome::mojom::ImageFormat,
               int32_t,
               RequestImageForContextNodeCallback),
              (override));
  MOCK_METHOD(void,
              ExecuteWebUIJavaScript,
              (const std::u16string&),
              (override));
  MOCK_METHOD(void, GetMediaFeedURL, (GetMediaFeedURLCallback), (override));
  MOCK_METHOD(void, LoadBlockedPlugins, (const std::string&), (override));
  MOCK_METHOD(void, SetShouldDeferMediaLoad, (bool), (override));
  MOCK_METHOD(void,
              InitializeTool,
              (actor::mojom::ToolInvocationPtr, InitializeToolCallback),
              (override));
  MOCK_METHOD(void,
              ExecuteTool,
              (const actor::TaskId&, ExecuteToolCallback),
              (override));
  MOCK_METHOD(void,
              InvokeTool,
              (actor::mojom::ToolInvocationPtr, InvokeToolCallback),
              (override));
  MOCK_METHOD(void, CancelTool, (const actor::TaskId&), (override));
  MOCK_METHOD(void,
              StartActorJournal,
              (mojo::PendingAssociatedRemote<actor::mojom::JournalClient>),
              (override));
  MOCK_METHOD(void,
              CreatePageStabilityMonitor,
              (mojo::PendingReceiver<PageStabilityMonitor>,
               const actor::TaskId&,
               bool),
              (override));
  MOCK_METHOD(void,
              GetCrossDocumentScriptToolResult,
              (const base::UnguessableToken&,
               GetCrossDocumentScriptToolResultCallback),
              (override));

 private:
  mojo::AssociatedReceiverSet<chrome::mojom::ChromeRenderFrame> receivers_;
};

}  // namespace

class PasswordChangePageStabilityWaiterTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordChangePageStabilityWaiterTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndDisableFeature(
        password_manager::features::kUseDetachedWidget);
  }
  ~PasswordChangePageStabilityWaiterTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com"));

    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        chrome::mojom::ChromeRenderFrame::Name_,
        base::BindRepeating(&MockChromeRenderFrame::BindPendingReceiver,
                            base::Unretained(&mock_chrome_render_frame_)));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockChromeRenderFrame mock_chrome_render_frame_;
  MockPageStabilityMonitor mock_page_stability_monitor_;
  password_manager::StubPasswordManagerClient stub_client_;
};

TEST_F(PasswordChangePageStabilityWaiterTest, PageBecomesStable) {
  base::test::TestFuture<void> future;

  EXPECT_CALL(mock_chrome_render_frame_, CreatePageStabilityMonitor)
      .WillOnce([&](mojo::PendingReceiver<PageStabilityMonitor> receiver,
                    const actor::TaskId&, bool) {
        mock_page_stability_monitor_.Bind(std::move(receiver));
      });

  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback;
  EXPECT_CALL(mock_page_stability_monitor_, NotifyWhenStable(_, _))
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback callback) {
        monitor_callback = std::move(callback);
      });

  PasswordChangePageStabilityWaiter waiter(web_contents(), &stub_client_,
                                           future.GetCallback());
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback; }));
  std::move(monitor_callback).Run();
  EXPECT_TRUE(future.Wait());
}

TEST_F(PasswordChangePageStabilityWaiterTest, RestartsOnNavigation) {
  base::test::TestFuture<void> future;

  EXPECT_CALL(mock_chrome_render_frame_, CreatePageStabilityMonitor)
      .Times(2)
      .WillRepeatedly([&](mojo::PendingReceiver<PageStabilityMonitor> receiver,
                          const actor::TaskId&, bool) {
        mock_page_stability_monitor_.Close();
        mock_page_stability_monitor_.Bind(std::move(receiver));
      });

  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback1;
  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback2;

  EXPECT_CALL(mock_page_stability_monitor_, NotifyWhenStable)
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback cb) {
        monitor_callback1 = std::move(cb);
      })
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback cb) {
        monitor_callback2 = std::move(cb);
      });

  PasswordChangePageStabilityWaiter waiter(web_contents(), &stub_client_,
                                           future.GetCallback());
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback1; }));

  // Emulate navigation event.
  waiter.DidFinishNavigation(nullptr);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback2; }));
  std::move(monitor_callback2).Run();
  EXPECT_TRUE(future.Wait());
}

TEST_F(PasswordChangePageStabilityWaiterTest, MonitorDisconnects) {
  base::test::TestFuture<void> future;

  EXPECT_CALL(mock_chrome_render_frame_, CreatePageStabilityMonitor)
      .WillOnce([&](mojo::PendingReceiver<PageStabilityMonitor> receiver,
                    const actor::TaskId&, bool) {
        mock_page_stability_monitor_.Bind(std::move(receiver));
      });

  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback;
  EXPECT_CALL(mock_page_stability_monitor_, NotifyWhenStable)
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback callback) {
        monitor_callback = std::move(callback);
      });

  PasswordChangePageStabilityWaiter waiter(web_contents(), &stub_client_,
                                           future.GetCallback());
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback; }));

  mock_page_stability_monitor_.Close();
  EXPECT_TRUE(future.Wait());
}
