// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/webrtc/chrome_screen_enumerator.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

class ChromeScreenEnumeratorTest : public ChromeAshTestBase {
 public:
  ChromeScreenEnumeratorTest() {}

  explicit ChromeScreenEnumeratorTest(const ChromeScreenEnumerator&) = delete;
  ChromeScreenEnumeratorTest& operator=(const ChromeScreenEnumerator&) = delete;

  ~ChromeScreenEnumeratorTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    enumerator_ = std::make_unique<ChromeScreenEnumerator>();
    scoped_feature_list_.InitFromCommandLine(
        /*enable_features=*/
        "GetAllScreensMedia",
        /*disable_features=*/"");
  }

  std::vector<raw_ptr<aura::Window, VectorExperimental>> GenerateScreensList(
      size_t number_of_screens) {
    screens_.clear();
    window_delegates_.clear();
    std::vector<raw_ptr<aura::Window, VectorExperimental>> screens;
    for (size_t i = 0; i < number_of_screens; ++i) {
      auto window_delegate = std::make_unique<aura::test::TestWindowDelegate>();
      auto screen = std::make_unique<aura::Window>(window_delegate.get());
      screen->Init(ui::LayerType::LAYER_NOT_DRAWN);
      screens.push_back(screen.get());
      screens_.emplace_back(std::move(screen));
      window_delegates_.emplace_back(std::move(window_delegate));
    }
    return screens;
  }

 protected:
  std::vector<std::unique_ptr<aura::test::TestWindowDelegate>>
      window_delegates_;
  std::vector<std::unique_ptr<aura::Window>> screens_;
  std::unique_ptr<ChromeScreenEnumerator> enumerator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeScreenEnumeratorTest, NoScreen) {
  ChromeScreenEnumerator::SetRootWindowsForTesting(/*root_windows=*/{});
  base::RunLoop run_loop;
  blink::mojom::StreamDevicesSetPtr actual_stream_devices_set;
  blink::mojom::MediaStreamRequestResult actual_result;
  enumerator_->EnumerateScreens(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
      base::BindLambdaForTesting(
          [&run_loop, &actual_stream_devices_set, &actual_result](
              const blink::mojom::StreamDevicesSet& stream_devices_set,
              blink::mojom::MediaStreamRequestResult result) {
            actual_stream_devices_set = stream_devices_set.Clone();
            actual_result = result;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_EQ(0u, actual_stream_devices_set->stream_devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::NO_HARDWARE, actual_result);
}

// TODO(crbug.com/40247902): Fix these tests for lacros.
TEST_F(ChromeScreenEnumeratorTest, SingleScreen) {
  ChromeScreenEnumerator::SetRootWindowsForTesting(
      GenerateScreensList(/*number_of_screens=*/1u));

  base::RunLoop run_loop;
  blink::mojom::StreamDevicesSetPtr actual_stream_devices_set;
  blink::mojom::MediaStreamRequestResult actual_result;
  enumerator_->EnumerateScreens(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
      base::BindLambdaForTesting(
          [&run_loop, &actual_stream_devices_set, &actual_result](
              const blink::mojom::StreamDevicesSet& stream_devices_set,
              blink::mojom::MediaStreamRequestResult result) {
            actual_stream_devices_set = stream_devices_set.Clone();
            actual_result = result;
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_EQ(1u, actual_stream_devices_set->stream_devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, actual_result);
}

TEST_F(ChromeScreenEnumeratorTest, MultipleScreens) {
  ChromeScreenEnumerator::SetRootWindowsForTesting(
      GenerateScreensList(/*number_of_screens=*/6u));

  base::RunLoop run_loop;
  blink::mojom::StreamDevicesSetPtr actual_stream_devices_set;
  blink::mojom::MediaStreamRequestResult actual_result;
  enumerator_->EnumerateScreens(
      blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
      base::BindLambdaForTesting(
          [&run_loop, &actual_stream_devices_set, &actual_result](
              const blink::mojom::StreamDevicesSet& stream_devices_set,
              blink::mojom::MediaStreamRequestResult result) {
            actual_stream_devices_set = stream_devices_set.Clone();
            actual_result = result;
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_EQ(6u, actual_stream_devices_set->stream_devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::OK, actual_result);
}
