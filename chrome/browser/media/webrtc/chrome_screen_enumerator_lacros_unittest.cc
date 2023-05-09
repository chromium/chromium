// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/webrtc/chrome_screen_enumerator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

class DesktopCapturerMock : public webrtc::DesktopCapturer {
 public:
  MOCK_METHOD(void, Start, (webrtc::DesktopCapturer::Callback*), (override));
  MOCK_METHOD(void, CaptureFrame, (), (override));
  MOCK_METHOD(bool,
              GetSourceList,
              (webrtc::DesktopCapturer::SourceList*),
              (override));
};

class ChromeScreenEnumeratorLacrosTest : public ::testing::Test {
 public:
  ~ChromeScreenEnumeratorLacrosTest() override = default;

  void RunEnumeratoreScreens() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Both usages of unretained are safe as both objects are tied to the life
    // time of the test fixture and all task runners will be torn down by the
    // destruction of the `task_environment_` before the fixture (and it's
    // member `enumerator_`) goes out of scope.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ChromeScreenEnumerator::EnumerateScreens,
            base::Unretained(enumerator_.get()),
            blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET,
            base::BindOnce(&ChromeScreenEnumeratorLacrosTest::ScreensCallback,
                           base::Unretained(this))));
    run_loop_.Run();
  }

  void ScreensCallback(const blink::mojom::StreamDevicesSet& stream_devices_set,
                       blink::mojom::MediaStreamRequestResult result) {
    actual_stream_devices_set_ = stream_devices_set.Clone();
    actual_result_ = result;
    run_loop_.Quit();
  }

  void SetUp() override {
    enumerator_ = std::make_unique<ChromeScreenEnumerator>();
    scoped_feature_list_.InitFromCommandLine(
        /*enable_features=*/
        "GetAllScreensMedia",
        /*disable_features=*/"");
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ChromeScreenEnumerator> enumerator_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::RunLoop run_loop_;
  blink::mojom::StreamDevicesSetPtr actual_stream_devices_set_;
  blink::mojom::MediaStreamRequestResult actual_result_;
};

TEST_F(ChromeScreenEnumeratorLacrosTest, NoScreenCapturer) {
  ChromeScreenEnumerator::SetDesktopCapturerForTesting(nullptr);

  RunEnumeratoreScreens();
  EXPECT_EQ(0u, actual_stream_devices_set_->stream_devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::NO_HARDWARE,
            actual_result_);
}

TEST_F(ChromeScreenEnumeratorLacrosTest, ScreenCapturerReturnsEmptyList) {
  ChromeScreenEnumerator::SetDesktopCapturerForTesting(
      std::make_unique<DesktopCapturerMock>());

  RunEnumeratoreScreens();
  EXPECT_EQ(0u, actual_stream_devices_set_->stream_devices.size());
  EXPECT_EQ(blink::mojom::MediaStreamRequestResult::NO_HARDWARE,
            actual_result_);
}
