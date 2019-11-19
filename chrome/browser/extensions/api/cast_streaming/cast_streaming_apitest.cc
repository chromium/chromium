// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/in_process_receiver.h"
#include "media/cast/test/utility/net_utility.h"
#include "media/cast/test/utility/standalone_cast_environment.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/log/net_log_source.h"
#include "net/socket/udp_server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::cast::test::GetFreeLocalPort;

namespace extensions {

class CastStreamingApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
    command_line->AppendSwitchASCII(::switches::kWindowSize, "300,300");
  }
};

// Test running the test extension for Cast Mirroring API.
IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, Basics) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "basics.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, Stats) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "stats.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, BadLogging) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "bad_logging.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, DestinationNotSet) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "destination_not_set.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, StopNoStart) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "stop_no_start.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, NullStream) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "null_stream.html"))
      << message_;
}

namespace {

struct YUVColor {
  int y;
  int u;
  int v;

  YUVColor() : y(0), u(0), v(0) {}
  YUVColor(int y_val, int u_val, int v_val) : y(y_val), u(u_val), v(v_val) {}
};

media::cast::FrameReceiverConfig WithFakeAesKeyAndIv(
    media::cast::FrameReceiverConfig config) {
  config.aes_key = "0123456789abcdef";
  config.aes_iv_mask = "fedcba9876543210";
  return config;
}

// An in-process Cast receiver that examines the audio/video frames being
// received for expected colors and tones.  Used in
// CastStreamingApiTest.EndToEnd, below.
class TestPatternReceiver : public media::cast::InProcessReceiver {
 public:
  explicit TestPatternReceiver(
      const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
      const net::IPEndPoint& local_end_point)
      : InProcessReceiver(
            cast_environment,
            local_end_point,
            net::IPEndPoint(),
            WithFakeAesKeyAndIv(media::cast::GetDefaultAudioReceiverConfig()),
            WithFakeAesKeyAndIv(media::cast::GetDefaultVideoReceiverConfig())) {
  }

  ~TestPatternReceiver() override {}

  void AddExpectedTone(int tone_frequency) {
    expected_tones_.push_back(tone_frequency);
  }

  void AddExpectedColor(const YUVColor& yuv_color) {
    expected_yuv_colors_.push_back(yuv_color);
  }

  // Blocks the caller until all expected tones and colors have been observed.
  void WaitForExpectedTonesAndColors() {
    base::RunLoop run_loop;
    cast_env()->PostTask(
        media::cast::CastEnvironment::MAIN,
        FROM_HERE,
        base::Bind(&TestPatternReceiver::NotifyOnceObservedAllTonesAndColors,
                   base::Unretained(this),
                   media::BindToCurrentLoop(run_loop.QuitClosure())));
    run_loop.Run();
  }

 private:
  void NotifyOnceObservedAllTonesAndColors(const base::Closure& done_callback) {
    DCHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));
    done_callback_ = done_callback;
    MaybeRunDoneCallback();
  }

  void MaybeRunDoneCallback() {
    DCHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));
    if (done_callback_.is_null())
      return;
    if (expected_tones_.empty() && expected_yuv_colors_.empty()) {
      std::move(done_callback_).Run();
    } else {
      LOG(INFO) << "Waiting to encounter " << expected_tones_.size()
                << " more tone(s) and " << expected_yuv_colors_.size()
                << " more color(s).";
    }
  }

  // Invoked by InProcessReceiver for each received audio frame.
  void OnAudioFrame(std::unique_ptr<media::AudioBus> audio_frame,
                    base::TimeTicks playout_time,
                    bool is_continuous) override {
    DCHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    if (audio_frame->frames() <= 0) {
      NOTREACHED() << "OnAudioFrame called with no samples?!?";
      return;
    }

    if (done_callback_.is_null() || expected_tones_.empty())
      return;  // No need to waste CPU doing analysis on the signal.

    // Assume the audio signal is a single sine wave (it can have some
    // low-amplitude noise).  Count zero crossings, and extrapolate the
    // frequency of the sine wave in |audio_frame|.
    int crossings = 0;
    for (int ch = 0; ch < audio_frame->channels(); ++ch) {
      crossings += media::cast::CountZeroCrossings(audio_frame->channel(ch),
                                                   audio_frame->frames());
    }
    crossings /= audio_frame->channels();  // Take the average.
    const float seconds_per_frame =
        audio_frame->frames() / static_cast<float>(audio_config().rtp_timebase);
    const float frequency = crossings / seconds_per_frame / 2.0f;
    VLOG(1) << "Current audio tone frequency: " << frequency;

    const int kTargetWindowHz = 20;
    for (auto it = expected_tones_.begin(); it != expected_tones_.end(); ++it) {
      if (abs(static_cast<int>(frequency) - *it) < kTargetWindowHz) {
        LOG(INFO) << "Heard tone at frequency " << *it << " Hz.";
        expected_tones_.erase(it);
        MaybeRunDoneCallback();
        break;
      }
    }
  }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                    base::TimeTicks playout_time,
                    bool is_continuous) override {
    DCHECK(cast_env()->CurrentlyOn(media::cast::CastEnvironment::MAIN));

    CHECK(video_frame->format() == media::PIXEL_FORMAT_YV12 ||
          video_frame->format() == media::PIXEL_FORMAT_I420 ||
          video_frame->format() == media::PIXEL_FORMAT_I420A);

    if (done_callback_.is_null() || expected_yuv_colors_.empty())
      return;  // No need to waste CPU doing analysis on the frame.

    // Take the median value of each plane because the test image will contain a
    // letterboxed content region of mostly a solid color plus a small piece of
    // "something" that's animating to keep the tab capture pipeline generating
    // new frames.
    const gfx::Rect region = FindLetterboxedContentRegion(*video_frame);
    YUVColor current_color;
    current_color.y = ComputeMedianIntensityInRegionInPlane(
        region,
        video_frame->stride(media::VideoFrame::kYPlane),
        video_frame->data(media::VideoFrame::kYPlane));
    current_color.u = ComputeMedianIntensityInRegionInPlane(
        gfx::ScaleToEnclosedRect(region, 0.5f),
        video_frame->stride(media::VideoFrame::kUPlane),
        video_frame->data(media::VideoFrame::kUPlane));
    current_color.v = ComputeMedianIntensityInRegionInPlane(
        gfx::ScaleToEnclosedRect(region, 0.5f),
        video_frame->stride(media::VideoFrame::kVPlane),
        video_frame->data(media::VideoFrame::kVPlane));
    VLOG(1) << "Current video color: yuv(" << current_color.y << ", "
            << current_color.u << ", " << current_color.v << ')';

    // Note: The range of acceptable colors is quite large because there's no
    // way to know whether software compositing is being used for screen
    // capture; and, if software compositing is being used, there is no color
    // space management and color values can be off by a lot. That said, color
    // accuracy is being tested by a suite of content_browsertests.
    const int kTargetWindow = 50;
    for (auto it = expected_yuv_colors_.begin();
         it != expected_yuv_colors_.end(); ++it) {
      if (abs(current_color.y - it->y) < kTargetWindow &&
          abs(current_color.u - it->u) < kTargetWindow &&
          abs(current_color.v - it->v) < kTargetWindow) {
        LOG(INFO) << "Saw expected color yuv(" << it->y << ", " << it->u << ", "
                  << it->v << ") as yuv(" << current_color.y << ", "
                  << current_color.u << ", " << current_color.v << ").";
        expected_yuv_colors_.erase(it);
        MaybeRunDoneCallback();
        break;
      }
    }
  }

  // Return the region that excludes the black letterboxing borders surrounding
  // the content within |frame|, if any.
  static gfx::Rect FindLetterboxedContentRegion(
      const media::VideoFrame& frame) {
    const int kNonBlackIntensityThreshold = 20;  // 16 plus some fuzz.
    const int width = frame.row_bytes(media::VideoFrame::kYPlane);
    const int height = frame.rows(media::VideoFrame::kYPlane);
    const int stride = frame.stride(media::VideoFrame::kYPlane);

    gfx::Rect result;

    // Scan from the bottom-right until the first non-black pixel is
    // encountered.
    for (int y = height - 1; y >= 0; --y) {
      const uint8_t* const start =
          frame.data(media::VideoFrame::kYPlane) + y * stride;
      const uint8_t* const end = start + width;
      for (const uint8_t* p = end - 1; p >= start; --p) {
        if (*p > kNonBlackIntensityThreshold) {
          result.set_width(p - start + 1);
          result.set_height(y + 1);
          y = 0;  // Discontinue outer loop.
          break;
        }
      }
    }

    // Scan from the upper-left until the first non-black pixel is encountered.
    for (int y = 0; y < result.height(); ++y) {
      const uint8_t* const start =
          frame.data(media::VideoFrame::kYPlane) + y * stride;
      const uint8_t* const end = start + result.width();
      for (const uint8_t* p = start; p < end; ++p) {
        if (*p > kNonBlackIntensityThreshold) {
          result.set_x(p - start);
          result.set_width(result.width() - result.x());
          result.set_y(y);
          result.set_height(result.height() - result.y());
          y = result.height();  // Discontinue outer loop.
          break;
        }
      }
    }

    return result;
  }

  static uint8_t ComputeMedianIntensityInRegionInPlane(const gfx::Rect& region,
                                                       int stride,
                                                       const uint8_t* data) {
    if (region.IsEmpty())
      return 0;
    const size_t num_values = region.size().GetArea();
    std::unique_ptr<uint8_t[]> values(new uint8_t[num_values]);
    for (int y = 0; y < region.height(); ++y) {
      memcpy(values.get() + y * region.width(),
             data + (region.y() + y) * stride + region.x(),
             region.width());
    }
    const size_t middle_idx = num_values / 2;
    std::nth_element(values.get(),
                     values.get() + middle_idx,
                     values.get() + num_values);
    return values[middle_idx];
  }

  std::vector<int> expected_tones_;
  std::vector<YUVColor> expected_yuv_colors_;
  base::Closure done_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestPatternReceiver);
};

}  // namespace

class CastStreamingApiTestWithPixelOutput
    : public CastStreamingApiTest,
      public testing::WithParamInterface<bool> {
 public:

  void SetUp() override {
    EnablePixelOutput();
    CastStreamingApiTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(::switches::kWindowSize, "128,128");
    CastStreamingApiTest::SetUpCommandLine(command_line);
  }

 private:
  base::test::ScopedFeatureList audio_service_features_;
};

// Tests the Cast streaming API and its basic functionality end-to-end.  An
// extension subtest is run to generate test content, capture that content, and
// use the API to send it out.  At the same time, this test launches an
// in-process Cast receiver, listening on a localhost UDP socket, to receive the
// content and check whether it matches expectations.
#if defined(NDEBUG) && !defined(OS_MACOSX)
#define MAYBE_EndToEnd EndToEnd
#else
// Flaky on Mac: https://crbug.com/841387
#define MAYBE_EndToEnd DISABLED_EndToEnd  // crbug.com/396413
#endif
IN_PROC_BROWSER_TEST_F(CastStreamingApiTestWithPixelOutput, MAYBE_EndToEnd) {
  std::unique_ptr<net::UDPServerSocket> receive_socket(
      new net::UDPServerSocket(NULL, net::NetLogSource()));
  receive_socket->AllowAddressReuse();
  ASSERT_EQ(net::OK, receive_socket->Listen(GetFreeLocalPort()));
  net::IPEndPoint receiver_end_point;
  ASSERT_EQ(net::OK, receive_socket->GetLocalAddress(&receiver_end_point));
  receive_socket.reset();

  // Start the in-process receiver that examines audio/video for the expected
  // test patterns.
  const scoped_refptr<media::cast::StandaloneCastEnvironment> cast_environment(
      new media::cast::StandaloneCastEnvironment());
  TestPatternReceiver* const receiver =
      new TestPatternReceiver(cast_environment, receiver_end_point);

  // Launch the page that: 1) renders the source content; 2) uses the
  // chrome.tabCapture and chrome.cast.streaming APIs to capture its content and
  // stream using Cast; and 3) calls chrome.test.succeed() once it is
  // operational.
  const std::string page_url = base::StringPrintf(
      "end_to_end_sender.html?port=%d&aesKey=%s&aesIvMask=%s",
      receiver_end_point.port(),
      base::HexEncode(receiver->audio_config().aes_key.data(),
                      receiver->audio_config().aes_key.size()).c_str(),
      base::HexEncode(receiver->audio_config().aes_iv_mask.data(),
                      receiver->audio_config().aes_iv_mask.size()).c_str());
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", page_url)) << message_;

  // Examine the Cast receiver for expected audio/video test patterns.  The
  // colors and tones specified here must match those in end_to_end_sender.js.
  // Note that we do not check that the color and tone are received
  // simultaneously since A/V sync should be measured in perf tests.
  receiver->AddExpectedTone(200 /* Hz */);
  receiver->AddExpectedTone(500 /* Hz */);
  receiver->AddExpectedTone(1800 /* Hz */);
  receiver->AddExpectedColor(YUVColor(63, 102, 239));  // rgb(255, 0, 0)
  receiver->AddExpectedColor(YUVColor(173, 41, 26));   // rgb(0, 255, 0)
  receiver->AddExpectedColor(YUVColor(32, 239, 117));  // rgb(0, 0, 255)
  receiver->Start();
  receiver->WaitForExpectedTonesAndColors();
  receiver->Stop();

  delete receiver;
  base::ScopedAllowBlockingForTesting allow_blocking;
  cast_environment->Shutdown();
}

#if !defined(OS_MACOSX)
#define MAYBE_RtpStreamError RtpStreamError
#else
// Flaky on Mac https://crbug.com/841986
#define MAYBE_RtpStreamError DISABLED_RtpStreamError
#endif
IN_PROC_BROWSER_TEST_F(CastStreamingApiTestWithPixelOutput,
                       MAYBE_RtpStreamError) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "rtp_stream_error.html"));
}

}  // namespace extensions
