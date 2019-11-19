// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_perf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/python_utils.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/public/common/features.h"
#include "ui/gl/gl_switches.h"

namespace {
std::string MakeLabel(const char* test_name, const std::string& video_codec) {
  std::string codec_label = video_codec.empty() ? "" : "_" + video_codec;
  return base::StringPrintf("%s%s", test_name, codec_label.c_str());
}
}  // namespace

static const base::FilePath::CharType kFrameAnalyzerExecutable[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("frame_analyzer.exe");
#else
    FILE_PATH_LITERAL("frame_analyzer");
#endif

static const base::FilePath::CharType kCapturedYuvFileName[] =
    FILE_PATH_LITERAL("captured_video.yuv");
static const base::FilePath::CharType kCapturedWebmFileName[] =
    FILE_PATH_LITERAL("captured_video.webm");
static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";
static const char kCapturingWebrtcHtmlPage[] =
    "/webrtc/webrtc_video_quality_test.html";

static const struct VideoQualityTestConfig {
  const char* test_name;
  int width;
  int height;
  const base::FilePath::CharType* reference_video;
  const char* constraints;
} kVideoConfigurations[] = {
  { "360p", 640, 360,
    test::kReferenceFileName360p,
    WebRtcTestBase::kAudioVideoCallConstraints360p },
    { "720p", 1280, 720,
    test::kReferenceFileName720p,
    WebRtcTestBase::kAudioVideoCallConstraints720p },
};

// Test the video quality of the WebRTC output.
//
// Prerequisites: This test case must run on a machine with a chrome playing
// the video from the reference files located in GetReferenceFilesDir().
// The file kReferenceY4mFileName.kY4mFileExtension is played using a
// FileVideoCaptureDevice and its sibling with kYuvFileExtension is used for
// comparison.
//
// You must also compile the frame_analyzer target before you run this
// test to get all the tools built.
//
// The test runs several custom binaries - rgba_to_i420 converter and
// frame_analyzer. Both tools can be found under third_party/webrtc/rtc_tools.
// The test also runs a stand alone Python implementation of a WebSocket server
// (pywebsocket) and a barcode_decoder script.
class WebRtcVideoQualityBrowserTest : public WebRtcTestBase,
    public testing::WithParamInterface<VideoQualityTestConfig> {
 public:
  WebRtcVideoQualityBrowserTest()
      : environment_(base::Environment::Create()) {
    test_config_ = GetParam();
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.

    ASSERT_TRUE(temp_working_dir_.CreateUniqueTempDir());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set up the command line option with the expected file name. We will check
    // its existence in HasAllRequiredResources().
    webrtc_reference_video_y4m_ = test::GetReferenceFilesDir()
        .Append(test_config_.reference_video)
        .AddExtension(test::kY4mFileExtension);
    command_line->AppendSwitchPath(switches::kUseFileForFakeVideoCapture,
                                   webrtc_reference_video_y4m_);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

  // Writes the captured video to a webm file.
  void WriteCapturedWebmVideo(content::WebContents* capturing_tab,
                              const base::FilePath& webm_video_filename) {
    std::string base64_encoded_video =
        ExecuteJavascript("getRecordedVideoAsBase64()", capturing_tab);
    std::string recorded_video;
    ASSERT_TRUE(base::Base64Decode(base64_encoded_video, &recorded_video));
    base::File video_file(webm_video_filename,
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    size_t written =
        video_file.Write(0, recorded_video.c_str(), recorded_video.length());
    ASSERT_EQ(recorded_video.length(), written);
  }

  // Runs ffmpeg on the captured webm video and writes it to a yuv video file.
  bool RunWebmToI420Converter(const base::FilePath& webm_video_filename,
                              const base::FilePath& yuv_video_filename,
                              const int width,
                              const int height) {
    base::FilePath path_to_ffmpeg = test::GetToolForPlatform("ffmpeg");
    if (!base::PathExists(path_to_ffmpeg)) {
      LOG(ERROR) << "Missing ffmpeg: should be in " << path_to_ffmpeg.value();
      return false;
    }

    // Set up ffmpeg to output at a certain resolution (-s) and bitrate (-b:v).
    // This is needed because WebRTC is free to start the call at a lower
    // resolution before ramping up. Without these flags, ffmpeg would output a
    // video in the inital lower resolution, causing the SSIM and PSNR results
    // to become meaningless.
    base::CommandLine ffmpeg_command(path_to_ffmpeg);
    ffmpeg_command.AppendArg("-i");
    ffmpeg_command.AppendArgPath(webm_video_filename);
    ffmpeg_command.AppendArg("-s");
    ffmpeg_command.AppendArg(base::StringPrintf("%dx%d", width, height));
    ffmpeg_command.AppendArg("-b:v");
    ffmpeg_command.AppendArg(base::StringPrintf("%d", 120 * width * height));
    ffmpeg_command.AppendArgPath(yuv_video_filename);

    // We produce an output file that will later be used as an input to the
    // barcode decoder and frame analyzer tools.
    DVLOG(0) << "Running " << ffmpeg_command.GetCommandLineString();
    std::string result;
    bool ok = base::GetAppOutputAndError(ffmpeg_command, &result);
    DVLOG(0) << "Output was:\n\n" << result;
    return ok;
  }

  // Compares the |captured_video_filename| with the |reference_video_filename|.
  //
  // The barcode decoder decodes the captured video containing barcodes overlaid
  // into every frame of the video. It produces a set of PNG images.
  // The frames should be of size |width| x |height|.
  // All measurements calculated are printed as perf parsable numbers to stdout.
  bool CompareVideosAndPrintResult(
      const std::string& test_label,
      int width,
      int height,
      const base::FilePath& captured_video_filename,
      const base::FilePath& reference_video_filename) {
    base::FilePath path_to_analyzer = base::MakeAbsoluteFilePath(
        GetBrowserDir().Append(kFrameAnalyzerExecutable));
    base::FilePath path_to_compare_script = GetSourceDir().Append(
        FILE_PATH_LITERAL("third_party/webrtc/rtc_tools/compare_videos.py"));

    if (!base::PathExists(path_to_analyzer)) {
      LOG(ERROR) << "Missing frame analyzer: should be in "
          << path_to_analyzer.value()
          << ". Try building the frame_analyzer target.";
      return false;
    }
    if (!base::PathExists(path_to_compare_script)) {
      LOG(ERROR) << "Missing video compare script: should be in "
          << path_to_compare_script.value();
      return false;
    }

    // Note: don't append switches to this command since it will mess up the
    // -u in the python invocation!
    base::CommandLine compare_command(base::CommandLine::NO_PROGRAM);
    EXPECT_TRUE(GetPythonCommand(&compare_command));

    compare_command.AppendArgPath(path_to_compare_script);
    compare_command.AppendArg("--label=" + test_label);
    compare_command.AppendArg("--ref_video");
    compare_command.AppendArgPath(reference_video_filename);
    compare_command.AppendArg("--test_video");
    compare_command.AppendArgPath(captured_video_filename);
    compare_command.AppendArg("--frame_analyzer");
    compare_command.AppendArgPath(path_to_analyzer);
    compare_command.AppendArg("--yuv_frame_width");
    compare_command.AppendArg(base::NumberToString(width));
    compare_command.AppendArg("--yuv_frame_height");
    compare_command.AppendArg(base::NumberToString(height));

    DVLOG(0) << "Running " << compare_command.GetCommandLineString();
    std::string output;
    bool ok = base::GetAppOutput(compare_command, &output);

    // Print to stdout to ensure the perf numbers are parsed properly by the
    // buildbot step. The tool should print a handful RESULT lines.
    printf("Output was:\n\n%s\n", output.c_str());
    bool has_result_lines = output.find("RESULT") != std::string::npos;
    if (!ok || !has_result_lines) {
      LOG(ERROR) << "Failed to compare videos; see output to see what "
                 << "the error was:\n\n"
                 << output;
      return false;
    }
    // TODO(http://crbug.com/1874811): Enable this and drop the printf above
    // when ready to switch to histogram sets.
    // if (!test::WriteCompareVideosOutputAsHistogram(test_label, output))
    //  return false;

    return true;
  }

  void TestVideoQuality(const std::string& video_codec,
                        bool prefer_hw_video_codec) {
    ASSERT_GE(TestTimeouts::test_launcher_timeout().InSeconds(), 150)
        << "This is a long-running test; you must specify "
           "--test-launcher-timeout to have a value of at least 150000.";
    ASSERT_GE(TestTimeouts::action_max_timeout().InSeconds(), 150)
        << "This is a long-running test; you must specify "
           "--ui-test-action-max-timeout to have a value of at least 150000.";
    ASSERT_LT(TestTimeouts::action_max_timeout(),
              TestTimeouts::test_launcher_timeout())
        << "action_max_timeout needs to be strictly-less-than "
           "test_launcher_timeout";
    ASSERT_TRUE(test::HasReferenceFilesInCheckout());
    ASSERT_TRUE(embedded_test_server()->Start());

    content::WebContents* left_tab =
        OpenPageAndGetUserMediaInNewTabWithConstraints(
            embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage),
            test_config_.constraints);
    content::WebContents* right_tab =
        OpenPageAndGetUserMediaInNewTabWithConstraints(
            embedded_test_server()->GetURL(kCapturingWebrtcHtmlPage),
            test_config_.constraints);

    SetupPeerconnectionWithLocalStream(left_tab);
    SetupPeerconnectionWithLocalStream(right_tab);

    if (!video_codec.empty()) {
      SetDefaultVideoCodec(left_tab, video_codec, prefer_hw_video_codec);
      SetDefaultVideoCodec(right_tab, video_codec, prefer_hw_video_codec);
    }
    NegotiateCall(left_tab, right_tab);

    // Poll slower here to avoid flooding the log with messages: capturing and
    // sending frames take quite a bit of time.
    int polling_interval_msec = 1000;

    EXPECT_TRUE(test::PollingWaitUntil("doneFrameCapturing()", "done-capturing",
                                       right_tab, polling_interval_msec));

    HangUp(left_tab);

    WriteCapturedWebmVideo(right_tab,
                           GetWorkingDir().Append(kCapturedWebmFileName));

    // Shut everything down to avoid having the javascript race with the
    // analysis tools. For instance, dont have console log printouts interleave
    // with the RESULT lines from the analysis tools (crbug.com/323200).
    chrome::CloseWebContents(browser(), left_tab, false);
    chrome::CloseWebContents(browser(), right_tab, false);

    RunWebmToI420Converter(GetWorkingDir().Append(kCapturedWebmFileName),
                           GetWorkingDir().Append(kCapturedYuvFileName),
                           test_config_.width, test_config_.height);

    ASSERT_TRUE(CompareVideosAndPrintResult(
        MakeLabel(test_config_.test_name, video_codec), test_config_.width,
        test_config_.height, GetWorkingDir().Append(kCapturedYuvFileName),
        test::GetReferenceFilesDir()
            .Append(test_config_.reference_video)
            .AddExtension(test::kYuvFileExtension)));
  }

 protected:
  VideoQualityTestConfig test_config_;

  base::FilePath GetWorkingDir() { return temp_working_dir_.GetPath(); }

 private:
  base::FilePath GetSourceDir() {
    base::FilePath source_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir);
    return source_dir;
  }

  base::FilePath GetBrowserDir() {
    base::FilePath browser_dir;
    EXPECT_TRUE(base::PathService::Get(base::DIR_MODULE, &browser_dir));
    return browser_dir;
  }

  std::unique_ptr<base::Environment> environment_;
  base::FilePath webrtc_reference_video_y4m_;
  base::ScopedTempDir temp_working_dir_;
};

INSTANTIATE_TEST_SUITE_P(WebRtcVideoQualityBrowserTests,
                         WebRtcVideoQualityBrowserTest,
                         testing::ValuesIn(kVideoConfigurations));

// WebRTC's frame_analyzer doesn't build from a Chromium's component build.
#if defined(COMPONENT_BUILD)
#define MAYBE_MANUAL_TestVideoQualityVp8 DISABLED_MANUAL_TestVideoQualityVp8
#else
#define MAYBE_MANUAL_TestVideoQualityVp8 MANUAL_TestVideoQualityVp8
#endif
IN_PROC_BROWSER_TEST_P(WebRtcVideoQualityBrowserTest,
                       MAYBE_MANUAL_TestVideoQualityVp8) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  TestVideoQuality("VP8", false /* prefer_hw_video_codec */);
}

// Flaky on windows and WebRTC's frame_analyzer doesn't build from a Chromium's
// component build.
// TODO(crbug.com/1008766): re-enable when flakiness is investigated, diagnosed
// and resolved.
#if defined(OS_WIN) || defined(COMPONENT_BUILD)
#define MAYBE_MANUAL_TestVideoQualityVp9 DISABLED_MANUAL_TestVideoQualityVp9
#else
#define MAYBE_MANUAL_TestVideoQualityVp9 MANUAL_TestVideoQualityVp9
#endif
IN_PROC_BROWSER_TEST_P(WebRtcVideoQualityBrowserTest,
                       MAYBE_MANUAL_TestVideoQualityVp9) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  TestVideoQuality("VP9", true /* prefer_hw_video_codec */);
}

#if BUILDFLAG(RTC_USE_H264)

// Flaky on mac (crbug.com/754684) and WebRTC's frame_analyzer doesn't build
// from a Chromium's component build.
#if defined(OS_MACOSX) || defined(COMPONENT_BUILD)
#define MAYBE_MANUAL_TestVideoQualityH264 DISABLED_MANUAL_TestVideoQualityH264
#else
#define MAYBE_MANUAL_TestVideoQualityH264 MANUAL_TestVideoQualityH264
#endif

IN_PROC_BROWSER_TEST_P(WebRtcVideoQualityBrowserTest,
                       MAYBE_MANUAL_TestVideoQualityH264) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Only run test if run-time feature corresponding to |rtc_use_h264| is on.
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebRtcH264WithOpenH264FFmpeg)) {
    LOG(WARNING) << "Run-time feature WebRTC-H264WithOpenH264FFmpeg disabled. "
        "Skipping WebRtcVideoQualityBrowserTest.MANUAL_TestVideoQualityH264 "
        "(test \"OK\")";
    return;
  }
  TestVideoQuality("H264", true /* prefer_hw_video_codec */);
}

#endif  // BUILDFLAG(RTC_USE_H264)
