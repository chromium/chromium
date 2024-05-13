// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_throttler/frame_throttling_controller.h"

#include <utility>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/test/test_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class FakeFrameSinkManagerImpl : public viz::TestFrameSinkManagerImpl {
 public:
  FakeFrameSinkManagerImpl() = default;
  FakeFrameSinkManagerImpl(const FakeFrameSinkManagerImpl&) = delete;
  FakeFrameSinkManagerImpl& operator=(const FakeFrameSinkManagerImpl&) = delete;
  ~FakeFrameSinkManagerImpl() override = default;

  // mojom::FrameSinkManager implementation:
  void Throttle(const std::vector<viz::FrameSinkId>& ids,
                base::TimeDelta interval) override {
    throttled_interval_ = interval;
    throttled_frame_sink_ids_ = ids;
  }

  base::TimeDelta throttled_interval() const { return throttled_interval_; }
  const std::vector<viz::FrameSinkId>& throttled_frame_sink_ids() const {
    return throttled_frame_sink_ids_;
  }

 private:
  base::TimeDelta throttled_interval_;
  std::vector<viz::FrameSinkId> throttled_frame_sink_ids_;
};

class FrameThrottlingControllerTest : public AshTestBase {
 protected:
  FrameThrottlingControllerTest() : controller_(&host_frame_sink_manager_) {
    mojo::PendingRemote<viz::mojom::FrameSinkManager> frame_sink_manager;
    mojo::PendingReceiver<viz::mojom::FrameSinkManager>
        frame_sink_manager_receiver =
            frame_sink_manager.InitWithNewPipeAndPassReceiver();
    mojo::PendingRemote<viz::mojom::FrameSinkManagerClient>
        frame_sink_manager_client;
    mojo::PendingReceiver<viz::mojom::FrameSinkManagerClient>
        frame_sink_manager_client_receiver =
            frame_sink_manager_client.InitWithNewPipeAndPassReceiver();

    host_frame_sink_manager_.BindAndSetManager(
        std::move(frame_sink_manager_client_receiver),
        task_environment()->GetMainThreadTaskRunner(),
        std::move(frame_sink_manager));
    frame_sink_manager_impl_.BindReceiver(
        std::move(frame_sink_manager_receiver),
        std::move(frame_sink_manager_client));
  }

  void SetUp() override {
    AshTestBase::SetUp();
    controller_.OnWindowTreeHostCreated(ash_test_helper()->GetHost());
  }

  std::unique_ptr<aura::Window> CreateTestBrowserWindow(
      const viz::FrameSinkId frame_sink_id) {
    std::unique_ptr<aura::Window> browser_window =
        CreateAppWindow(gfx::Rect(100, 100), chromeos::AppType::BROWSER);
    browser_window->SetEmbedFrameSinkId(frame_sink_id);
    return browser_window;
  }

  FakeFrameSinkManagerImpl frame_sink_manager_impl_;
  viz::HostFrameSinkManager host_frame_sink_manager_;
  FrameThrottlingController controller_;
};

TEST_F(FrameThrottlingControllerTest, ManualThrottling) {
  std::unique_ptr<aura::Window> window_1 = CreateTestWindow();
  window_1->SetEmbedFrameSinkId({1, 1});
  std::unique_ptr<aura::Window> window_2 = CreateTestWindow();
  window_2->SetEmbedFrameSinkId({2, 2});

  // 10 Hz is lower than the default, but there are no other windows actively
  // being throttled, so 10 Hz should be used.
  controller_.StartThrottling({window_1.get(), window_2.get()},
                              base::Hertz(10));
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(frame_sink_manager_impl_.throttled_interval(),
              Eq(base::Hertz(10)));
  EXPECT_THAT(
      frame_sink_manager_impl_.throttled_frame_sink_ids(),
      UnorderedElementsAre(viz::FrameSinkId({1, 1}), viz::FrameSinkId({2, 2})));

  controller_.StartThrottling({window_1.get()});
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(frame_sink_manager_impl_.throttled_interval(),
              Eq(base::Hertz(kDefaultThrottleFps)));
  EXPECT_THAT(frame_sink_manager_impl_.throttled_frame_sink_ids(),
              UnorderedElementsAre(viz::FrameSinkId({1, 1})));

  controller_.EndThrottling();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(frame_sink_manager_impl_.throttled_frame_sink_ids(), IsEmpty());
}

TEST_F(FrameThrottlingControllerTest, CompositingBasedThrottling) {
  constexpr viz::FrameSinkId kBrowserWindowFrameSinkId = {99, 99};

  std::unique_ptr<aura::Window> browser_window =
      CreateTestBrowserWindow(kBrowserWindowFrameSinkId);

  controller_.OnCompositingFrameSinksToThrottleUpdated(
      ash_test_helper()->GetHost(), {kBrowserWindowFrameSinkId});
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(frame_sink_manager_impl_.throttled_interval(),
              Eq(base::Hertz(kDefaultThrottleFps)));
  EXPECT_THAT(frame_sink_manager_impl_.throttled_frame_sink_ids(),
              UnorderedElementsAre(kBrowserWindowFrameSinkId));
}

TEST_F(FrameThrottlingControllerTest, ManualAndCompositingBasedThrottling) {
  constexpr viz::FrameSinkId kBrowserWindowFrameSinkId = {99, 99};
  constexpr viz::FrameSinkId kManualWindowFrameSinkId = {100, 100};

  std::unique_ptr<aura::Window> browser_window =
      CreateTestBrowserWindow(kBrowserWindowFrameSinkId);

  controller_.OnCompositingFrameSinksToThrottleUpdated(
      ash_test_helper()->GetHost(), {kBrowserWindowFrameSinkId});
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<aura::Window> manual_window = CreateTestWindow();
  manual_window->SetEmbedFrameSinkId(kManualWindowFrameSinkId);

  // 10 Hz is lower than the default frame rate, so it should be rejected.
  controller_.StartThrottling({manual_window.get()}, base::Hertz(10));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(frame_sink_manager_impl_.throttled_interval(),
              Eq(base::Hertz(kDefaultThrottleFps)));
  EXPECT_THAT(frame_sink_manager_impl_.throttled_frame_sink_ids(),
              UnorderedElementsAre(kBrowserWindowFrameSinkId,
                                   kManualWindowFrameSinkId));

  // 30 Hz is higher than the default frame rate, so it should be respected.
  controller_.StartThrottling({manual_window.get()}, base::Hertz(30));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(frame_sink_manager_impl_.throttled_interval(),
              Eq(base::Hertz(30)));
  EXPECT_THAT(frame_sink_manager_impl_.throttled_frame_sink_ids(),
              UnorderedElementsAre(kBrowserWindowFrameSinkId,
                                   kManualWindowFrameSinkId));

  // The frame rate should return to default after manual window is removed.
  controller_.EndThrottling();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(frame_sink_manager_impl_.throttled_interval(),
              Eq(base::Hertz(kDefaultThrottleFps)));
  EXPECT_THAT(frame_sink_manager_impl_.throttled_frame_sink_ids(),
              UnorderedElementsAre(kBrowserWindowFrameSinkId));
}

}  // namespace
}  // namespace ash
