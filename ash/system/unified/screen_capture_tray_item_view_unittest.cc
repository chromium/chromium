// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/screen_capture_tray_item_view.h"

#include <memory>

#include "ash/shelf/shelf.h"
#include "ash/test/ash_test_base.h"
#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace {
constexpr base::TimeDelta minimal_tray_item_presence_time = base::Seconds(6);
}  // namespace

namespace ash {
class ScreenCaptureTrayItemViewMock : public ScreenCaptureTrayItemView {
 public:
  explicit ScreenCaptureTrayItemViewMock(Shelf* shelf)
      : ScreenCaptureTrayItemView(shelf) {}
  ScreenCaptureTrayItemViewMock& operator=(
      const ScreenCaptureTrayItemViewMock&) = delete;
  ~ScreenCaptureTrayItemViewMock() override = default;

  MOCK_METHOD(void, SetVisible, (bool), (override));
};

class ScreenCaptureTrayItemViewTest : public AshTestBase {
 public:
  ScreenCaptureTrayItemViewTest()
      : AshTestBase(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}
  ScreenCaptureTrayItemViewTest(const ScreenCaptureTrayItemViewTest&) = delete;
  ScreenCaptureTrayItemViewTest& operator=(
      const ScreenCaptureTrayItemViewTest&) = delete;
  ~ScreenCaptureTrayItemViewTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    screen_capture_tray_item_view_ =
        std::make_unique<ScreenCaptureTrayItemViewMock>(GetPrimaryShelf());
  }

 protected:
  std::unique_ptr<ScreenCaptureTrayItemViewMock> screen_capture_tray_item_view_;
};

TEST_F(ScreenCaptureTrayItemViewTest, SingleOriginCaptureStartedAndStopped) {
  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(true));
  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_1", /*origin=*/
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example.com", /*port=*/443));
  EXPECT_EQ(1u, screen_capture_tray_item_view_->requests_.size());

  task_environment()->FastForwardBy(minimal_tray_item_presence_time +
                                    base::Milliseconds(1));
  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(false));
  screen_capture_tray_item_view_->MultiCaptureStopped(/*label=*/"label_1");
  EXPECT_EQ(0u, screen_capture_tray_item_view_->requests_.size());
}

TEST_F(ScreenCaptureTrayItemViewTest, MultiOriginCaptureStartedAndStopped) {
  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(true)).Times(3);
  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_1", /*origin=*/
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example.com", /*port=*/443));
  EXPECT_EQ(1u, screen_capture_tray_item_view_->requests_.size());

  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_2", /*origin=*/
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example2.com", /*port=*/443));
  EXPECT_EQ(2u, screen_capture_tray_item_view_->requests_.size());

  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_1", /*origin=*/
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example2.com", /*port=*/443));
  EXPECT_EQ(2u, screen_capture_tray_item_view_->requests_.size());

  task_environment()->FastForwardBy(minimal_tray_item_presence_time +
                                    base::Milliseconds(1));
  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(true));
  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(false));
  screen_capture_tray_item_view_->MultiCaptureStopped(/*label=*/"label_1");
  EXPECT_EQ(1u, screen_capture_tray_item_view_->requests_.size());

  screen_capture_tray_item_view_->MultiCaptureStopped(/*label=*/"label_2");
  EXPECT_EQ(0u, screen_capture_tray_item_view_->requests_.size());
}

TEST_F(ScreenCaptureTrayItemViewTest,
       MultiOriginCaptureStartedAndEarlyStoppedExpectedDelayedStoppedCallback) {
  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(true)).Times(3);
  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_1", /*origin=*/
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example.com", /*port=*/443));
  EXPECT_EQ(1u, screen_capture_tray_item_view_->requests_.size());

  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_2", /*origin=*/
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example2.com", /*port=*/443));
  EXPECT_EQ(2u, screen_capture_tray_item_view_->requests_.size());

  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_1", /*origin=*/
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example2.com", /*port=*/443));
  EXPECT_EQ(2u, screen_capture_tray_item_view_->requests_.size());

  task_environment()->FastForwardBy(minimal_tray_item_presence_time -
                                    base::Milliseconds(1));
  EXPECT_EQ(2u, screen_capture_tray_item_view_->requests_.size());

  screen_capture_tray_item_view_->MultiCaptureStopped(/*label=*/"label_1");
  screen_capture_tray_item_view_->MultiCaptureStopped(/*label=*/"label_2");

  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(true));
  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(false));
  task_environment()->FastForwardBy(base::Milliseconds(2));
  EXPECT_EQ(0u, screen_capture_tray_item_view_->requests_.size());
}

// TODO(crbug.com/417560454): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MultiOriginCaptureStartedNotificationSkipAllowlistedOneOrigin \
  DISABLED_MultiOriginCaptureStartedNotificationSkipAllowlistedOneOrigin
#else
#define MAYBE_MultiOriginCaptureStartedNotificationSkipAllowlistedOneOrigin \
  MultiOriginCaptureStartedNotificationSkipAllowlistedOneOrigin
#endif
TEST_F(ScreenCaptureTrayItemViewTest,
       MAYBE_MultiOriginCaptureStartedNotificationSkipAllowlistedOneOrigin) {
  const url::Origin origin_with_allowlisted_exception =
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"isolated-app",
          /*host=*/"aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic",
          /*port=*/0);
  CHECK_DEREF(web_app::IwaKeyDistributionInfoProvider::GetInstance())
      .SetComponentDataForTesting(
          web_app::IwaKeyDistributionInfoProvider::ComponentData(
              /*version=*/base::Version("1.0.0"),
              /*key_rotations=*/{},
              /*version=*/
              {{origin_with_allowlisted_exception.host(),
                {.skip_capture_started_notification = true}}},
              /*managed_allowlist=*/{},
              /*special_app_permissions=*/true));
  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(true)).Times(0);

  std::vector<url::Origin> skip_notification_origins = {
      origin_with_allowlisted_exception};

  // This origin is exempt from showing the indicator and therefore doesn't
  // add another entry.
  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_1", /*origin=*/
      origin_with_allowlisted_exception);
  EXPECT_EQ(0u, screen_capture_tray_item_view_->requests_.size());

  task_environment()->FastForwardBy(minimal_tray_item_presence_time -
                                    base::Milliseconds(1));
  EXPECT_EQ(0u, screen_capture_tray_item_view_->requests_.size());

  screen_capture_tray_item_view_->MultiCaptureStopped(/*label=*/"label_1");
  EXPECT_EQ(0u, screen_capture_tray_item_view_->requests_.size());

  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(false)).Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(2));
  EXPECT_EQ(0u, screen_capture_tray_item_view_->requests_.size());
}

// TODO(crbug.com/417560454): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_MultiOriginCaptureStartedNotificationSkipAllowlistedMixedOrigins \
  DISABLED_MultiOriginCaptureStartedNotificationSkipAllowlistedMixedOrigins
#else
#define MAYBE_MultiOriginCaptureStartedNotificationSkipAllowlistedMixedOrigins \
  MultiOriginCaptureStartedNotificationSkipAllowlistedMixedOrigins
#endif
TEST_F(ScreenCaptureTrayItemViewTest,
       MAYBE_MultiOriginCaptureStartedNotificationSkipAllowlistedMixedOrigins) {
  const url::Origin origin_with_allowlisted_exception =
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"isolated-app",
          /*host=*/"aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic",
          /*port=*/0);
  CHECK_DEREF(web_app::IwaKeyDistributionInfoProvider::GetInstance())
      .SetComponentDataForTesting(
          web_app::IwaKeyDistributionInfoProvider::ComponentData(
              /*version=*/base::Version("1.0.0"),
              /*key_rotations=*/{},
              /*version=*/
              {{origin_with_allowlisted_exception.host(),
                {.skip_capture_started_notification = true}}},
              /*managed_allowlist=*/{},
              /*special_app_permissions=*/true));

  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(true)).Times(1);
  const url::Origin origin_with_indicator =
      url::Origin::CreateFromNormalizedTuple(
          /*scheme=*/"https", /*host=*/"example.com", /*port=*/443);
  std::vector<url::Origin> skip_notification_origins = {
      origin_with_allowlisted_exception};

  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_1", /*origin=*/
      origin_with_indicator);
  EXPECT_EQ(1u, screen_capture_tray_item_view_->requests_.size());

  // This origin is exempt from showing the indicator and therefore
  // doesn't add another entry.
  screen_capture_tray_item_view_->MultiCaptureStarted(
      /*label=*/"label_2", /*origin=*/
      origin_with_allowlisted_exception);
  EXPECT_EQ(1u, screen_capture_tray_item_view_->requests_.size());

  task_environment()->FastForwardBy(minimal_tray_item_presence_time -
                                    base::Milliseconds(1));
  EXPECT_EQ(1u, screen_capture_tray_item_view_->requests_.size());

  screen_capture_tray_item_view_->MultiCaptureStopped(
      /*label=*/"label_1");
  EXPECT_EQ(1u, screen_capture_tray_item_view_->requests_.size());

  screen_capture_tray_item_view_->MultiCaptureStopped(
      /*label=*/"label_2");

  EXPECT_CALL(*screen_capture_tray_item_view_, SetVisible(false));
  task_environment()->FastForwardBy(base::Milliseconds(2));
  EXPECT_EQ(0u, screen_capture_tray_item_view_->requests_.size());
}

}  // namespace ash
