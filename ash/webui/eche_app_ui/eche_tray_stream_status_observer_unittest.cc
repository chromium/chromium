// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_tray_stream_status_observer.h"

#include "ash/constants/ash_features.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_suite.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/webui/eche_app_ui/eche_stream_status_change_handler.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace eche_app {

namespace {
void GracefulCloseFunction() {}
void GracefulGoBackFunction() {}
}  // namespace

class EcheTrayStreamStatusObserverTest : public AshTestBase {
 protected:
  EcheTrayStreamStatusObserverTest() = default;
  EcheTrayStreamStatusObserverTest(const EcheTrayStreamStatusObserverTest&) =
      delete;
  EcheTrayStreamStatusObserverTest& operator=(
      const EcheTrayStreamStatusObserverTest&) = delete;
  ~EcheTrayStreamStatusObserverTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});
    DCHECK(test_web_view_factory_.get());
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
    eche_tray_ =
        ash::StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();

    stream_status_change_handler_ =
        std::make_unique<EcheStreamStatusChangeHandler>();
    observer_ = std::make_unique<EcheTrayStreamStatusObserver>(
        stream_status_change_handler_.get());
  }

  void TearDown() override {
    observer_.reset();
    stream_status_change_handler_.reset();
    AshTestBase::TearDown();
  }

  void OnStartStreaming() { observer_->OnStartStreaming(); }

  void OnStreamStatusChanged(mojom::StreamStatus status) {
    observer_->OnStreamStatusChanged(status);
  }

  EcheTray* eche_tray() { return eche_tray_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  EcheTray* eche_tray_ = nullptr;
  std::unique_ptr<EcheStreamStatusChangeHandler> stream_status_change_handler_;
  std::unique_ptr<EcheTrayStreamStatusObserver> observer_;
  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

TEST_F(EcheTrayStreamStatusObserverTest, LaunchBubble) {
  LaunchBubble(GURL("http://google.com"), gfx::Image(), u"app 1",
               base::BindOnce(&GracefulCloseFunction),
               base::BindRepeating(&GracefulGoBackFunction));

  // Wait for Eche Tray to load Eche Web to complete.
  base::RunLoop().RunUntilIdle();
  // Bubble widget should be created after launch.
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
}

TEST_F(EcheTrayStreamStatusObserverTest, CloseBubble) {
  CloseBubble();

  // Wait for Eche Web to close.
  base::RunLoop().RunUntilIdle();
  // Eche tray should be visible after close.
  EXPECT_FALSE(eche_tray()->is_active());
}

TEST_F(EcheTrayStreamStatusObserverTest, OnStartStreaming) {
  OnStartStreaming();

  // Wait for Eche Tray to load Eche Web to complete.
  base::RunLoop().RunUntilIdle();
  // The bubble should not be created if LaunchBubble be called before.
  EXPECT_FALSE(eche_tray()->get_bubble_wrapper_for_test());

  LaunchBubble(GURL("http://google.com"), gfx::Image(), u"app 1",
               base::BindOnce(&GracefulCloseFunction),
               base::BindRepeating(&GracefulGoBackFunction));

  // Wait for Eche Tray to load Eche Web to complete.
  base::RunLoop().RunUntilIdle();
  // The bubble widget should be created but not be activated yet.
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_FALSE(eche_tray()->is_active());

  OnStartStreaming();

  // Wait for the bubble to show up.
  base::RunLoop().RunUntilIdle();
  // The bubble widget should be activated.
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());
  EXPECT_TRUE(eche_tray()->is_active());
}

TEST_F(EcheTrayStreamStatusObserverTest, OnStreamStatusChanged) {
  LaunchBubble(GURL("http://google.com"), gfx::Image(), u"app 1",
               base::BindOnce(&GracefulCloseFunction),
               base::BindRepeating(&GracefulGoBackFunction));
  OnStreamStatusChanged(mojom::StreamStatus::kStreamStatusStarted);

  // Wait for Eche Tray to load Eche Web to complete.
  base::RunLoop().RunUntilIdle();
  // Eche tray should be visible when streaming is active
  EXPECT_TRUE(eche_tray()->get_bubble_wrapper_for_test());

  OnStreamStatusChanged(mojom::StreamStatus::kStreamStatusStopped);

  // Wait for Eche Web to close.
  base::RunLoop().RunUntilIdle();
  // Eche tray should not be visible when streaming is finished
  EXPECT_FALSE(eche_tray()->is_active());
}

}  // namespace eche_app
}  // namespace ash
