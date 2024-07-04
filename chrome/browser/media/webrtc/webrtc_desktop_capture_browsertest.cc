// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/desktop_capture/desktop_capture_api.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gl/gl_switches.h"

namespace {
static const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

content::WebContents* GetWebContents(Browser* browser, int tab) {
  return browser->tab_strip_model()->GetWebContentsAt(tab);
}

content::DesktopMediaID GetDesktopMediaIDForScreen() {
  return content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                 content::DesktopMediaID::kFakeId);
}

content::DesktopMediaID GetDesktopMediaIDForTab(Browser* browser, int tab) {
  content::RenderFrameHost* main_frame =
      GetWebContents(browser, tab)->GetPrimaryMainFrame();
  return content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                         main_frame->GetRoutingID()));
}

infobars::ContentInfoBarManager* GetInfoBarManager(Browser* browser, int tab) {
  return infobars::ContentInfoBarManager::FromWebContents(
      GetWebContents(browser, tab));
}

infobars::ContentInfoBarManager* GetInfoBarManager(
    content::WebContents* contents) {
  return infobars::ContentInfoBarManager::FromWebContents(contents);
}

TabSharingInfoBarDelegate* GetDelegate(Browser* browser, int tab) {
  return static_cast<TabSharingInfoBarDelegate*>(
      GetInfoBarManager(browser, tab)->infobars()[0]->delegate());
}

class InfobarUIChangeObserver : public TabStripModelObserver {
 public:
  explicit InfobarUIChangeObserver(Browser* browser) : browser_{browser} {
    for (int tab = 0; tab < browser_->tab_strip_model()->count(); ++tab) {
      auto* contents = browser_->tab_strip_model()->GetWebContentsAt(tab);
      observers_[contents] =
          std::make_unique<InfoBarChangeObserver>(base::BindOnce(
              &InfobarUIChangeObserver::EraseObserver, base::Unretained(this)));
      GetInfoBarManager(contents)->AddObserver(observers_[contents].get());
    }
    browser_->tab_strip_model()->AddObserver(this);
  }

  ~InfobarUIChangeObserver() override {
    for (auto& observer_iter : observers_) {
      auto* contents = observer_iter.first;
      auto* observer = observer_iter.second.get();

      GetInfoBarManager(contents)->RemoveObserver(observer);
    }
    browser_->tab_strip_model()->RemoveObserver(this);
    observers_.clear();
  }

  void ExpectCalls(size_t expected_changes) {
    run_loop_ = std::make_unique<base::RunLoop>();
    barrier_closure_ =
        base::BarrierClosure(expected_changes, run_loop_->QuitClosure());
    for (auto& observer : observers_) {
      observer.second->SetCallback(barrier_closure_);
    }
  }

  void Wait() { run_loop_->Run(); }

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted) {
      for (const auto& contents_with_index : change.GetInsert()->contents) {
        auto* contents = contents_with_index.contents.get();
        if (observers_.find(contents) == observers_.end()) {
          observers_[contents] = std::make_unique<InfoBarChangeObserver>(
              base::BindOnce(&InfobarUIChangeObserver::EraseObserver,
                             base::Unretained(this)));
          GetInfoBarManager(contents)->AddObserver(observers_[contents].get());
          if (!barrier_closure_.is_null()) {
            observers_[contents]->SetCallback(barrier_closure_);
          }
        }
      }
    }
  }
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    if (observers_.find(contents) == observers_.end()) {
      observers_[contents] =
          std::make_unique<InfoBarChangeObserver>(base::BindOnce(
              &InfobarUIChangeObserver::EraseObserver, base::Unretained(this)));
      GetInfoBarManager(contents)->AddObserver(observers_[contents].get());
      if (!barrier_closure_.is_null()) {
        observers_[contents]->SetCallback(barrier_closure_);
      }
    }
  }

 private:
  class InfoBarChangeObserver;

 public:
  void EraseObserver(InfoBarChangeObserver* observer) {
    auto iter = base::ranges::find(
        observers_, observer,
        [](const auto& observer_iter) { return observer_iter.second.get(); });
    observers_.erase(iter);
  }

 private:
  class InfoBarChangeObserver : public infobars::InfoBarManager::Observer {
   public:
    using ShutdownCallback = base::OnceCallback<void(InfoBarChangeObserver*)>;

    explicit InfoBarChangeObserver(ShutdownCallback shutdown_callback)
        : shutdown_callback_{std::move(shutdown_callback)} {}

    ~InfoBarChangeObserver() override = default;

    void SetCallback(base::RepeatingClosure change_closure) {
      DCHECK(!change_closure.is_null());
      change_closure_ = change_closure;
    }

    void OnInfoBarAdded(infobars::InfoBar* infobar) override {
      DCHECK(!change_closure_.is_null());
      change_closure_.Run();
    }

    void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
      DCHECK(!change_closure_.is_null());
      change_closure_.Run();
    }

    void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                           infobars::InfoBar* new_infobar) override {
      NOTREACHED_IN_MIGRATION();
    }

    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override {
      manager->RemoveObserver(this);
      DCHECK(!shutdown_callback_.is_null());
      std::move(shutdown_callback_).Run(this);
    }

   private:
    base::RepeatingClosure change_closure_;
    ShutdownCallback shutdown_callback_;
  };

  std::unique_ptr<base::RunLoop> run_loop_;
  std::map<content::WebContents*, std::unique_ptr<InfoBarChangeObserver>>
      observers_;
  raw_ptr<Browser> browser_;
  base::RepeatingClosure barrier_closure_;
};

}  // namespace

// Top-level integration test for WebRTC. Uses an actual desktop capture
// extension to capture the whole screen or a tab.
class WebRtcDesktopCaptureBrowserTest : public WebRtcTestBase {
 public:
  using MediaIDCallback = base::OnceCallback<content::DesktopMediaID()>;

  WebRtcDesktopCaptureBrowserTest() {
    extensions::DesktopCaptureChooseDesktopMediaFunction::
        SetPickerFactoryForTests(&picker_factory_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Flags use to automatically select the right dekstop source and get
    // around security restrictions.
    command_line->AppendSwitchASCII(switches::kAutoSelectDesktopCaptureSource,
                                    "Entire screen");
    command_line->AppendSwitch(switches::kEnableUserMediaScreenCapturing);
    // MSan and GL do not get along so avoid using the GPU with MSan.
    // TODO(crbug.com/40260482): Remove this after fixing feature
    // detection in 0c tab capture path as it'll no longer be needed.
#if !BUILDFLAG(IS_CHROMEOS) && !defined(MEMORY_SANITIZER)
    command_line->AppendSwitch(switches::kUseGpuInTests);
#endif
  }

 protected:
  void InitializeTabSharingForFirstTab(
      MediaIDCallback media_id_callback,
      InfobarUIChangeObserver* observer,
      std::optional<std::string> extra_video_constraints = std::nullopt) {
    ASSERT_TRUE(embedded_test_server()->Start());
    LoadDesktopCaptureExtension();
    auto* first_tab = OpenTestPageInNewTab(kMainWebrtcTestHtmlPage);
    OpenTestPageInNewTab(kMainWebrtcTestHtmlPage);

    FakeDesktopMediaPickerFactory::TestFlags test_flags{
        .expect_screens = true,
        .expect_windows = true,
        .expect_tabs = true,
        .selected_source = std::move(media_id_callback).Run(),
    };
    picker_factory_.SetTestFlags(&test_flags, /*tests_count=*/1);

    std::string stream_id = GetDesktopMediaStream(first_tab);
    EXPECT_NE(stream_id, "");

    LOG(INFO) << "Opened desktop media stream, got id " << stream_id;

    std::string constraints = base::StrCat(
        {"{audio: false, video: { mandatory: {chromeMediaSource: 'desktop', "
         "chromeMediaSourceId: '",
         stream_id, "'", (extra_video_constraints.has_value() ? ", " : ""),
         extra_video_constraints.value_or(""), "}}}"});

    // Should create 3 infobars if a tab (webcontents) is shared!
    if (observer)
      observer->ExpectCalls(3);

    EXPECT_TRUE(GetUserMediaWithSpecificConstraintsAndAcceptIfPrompted(
        first_tab, constraints));

    if (observer)
      observer->Wait();
  }

  void DetectVideoAndHangUp(content::WebContents* first_tab,
                            content::WebContents* second_tab) {
    StartDetectingVideo(first_tab, "remote-view");
    StartDetectingVideo(second_tab, "remote-view");
#if !BUILDFLAG(IS_MAC)
    // Video is choppy on Mac OS X. http://crbug.com/443542.
    WaitForVideoToPlay(first_tab);
    WaitForVideoToPlay(second_tab);
#endif
    HangUp(first_tab);
    HangUp(second_tab);
  }

  void RunP2PScreenshareWhileSharing(MediaIDCallback media_id_callback) {
    InitializeTabSharingForFirstTab(std::move(media_id_callback), nullptr);
    auto* first_tab = browser()->tab_strip_model()->GetWebContentsAt(1);
    auto* second_tab = browser()->tab_strip_model()->GetWebContentsAt(2);
    GetUserMediaAndAccept(second_tab);

    SetupPeerconnectionWithLocalStream(first_tab);
    SetupPeerconnectionWithLocalStream(second_tab);
    NegotiateCall(first_tab, second_tab);
    DetectVideoAndHangUp(first_tab, second_tab);
  }

  FakeDesktopMediaPickerFactory picker_factory_;
};

// TODO(crbug.com/40915051): Fails on MAC.
// TODO(crbug.com/40915051): Fails with MSAN. Determine if enabling the test for
// MSAN is feasible or not.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TabCaptureProvidesMinFps DISABLED_TabCaptureProvidesMinFps
#elif defined(MEMORY_SANITIZER)
#define MAYBE_TabCaptureProvidesMinFps DISABLED_TabCaptureProvidesMinFps
#else
#define MAYBE_TabCaptureProvidesMinFps TabCaptureProvidesMinFps
#endif
IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       MAYBE_TabCaptureProvidesMinFps) {
  constexpr int kFps = 30;
  constexpr const char* kFpsString = "30";
  constexpr int kTestTimeSeconds = 2;
  // We wait with measuring frame rate until a few frames has passed. This is
  // because the frame rate frame dropper in VideoTrackAdapter is pretty
  // aggressive dropping frames when the stream starts.
  constexpr int kNumFramesBeforeStabilization = kFps;

  InitializeTabSharingForFirstTab(
      base::BindOnce(GetDesktopMediaIDForTab, base::Unretained(browser()), 1),
      nullptr, base::StrCat({"minFrameRate: ", kFpsString}));
  content::WebContents* first_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EnableVideoFrameCallbacks(first_tab, "local-view");

  // First wait for a frame to appear, then wait until we get the number of
  // frames expected during the test time.
  int initial_frame_counter = 0;
  base::TimeTicks initial_timestamp;
  ASSERT_TRUE(test::PollingWaitUntilClosureEvaluatesTrue(
      base::BindLambdaForTesting([&]() -> bool {
        initial_timestamp = base::TimeTicks::Now();
        initial_frame_counter = GetNumVideoFrameCallbacks(first_tab);
        return initial_frame_counter > kNumFramesBeforeStabilization;
      }),
      first_tab, base::Milliseconds(50)));
  int final_frame_counter = 0;
  base::TimeTicks final_timestamp;
  ASSERT_TRUE(test::PollingWaitUntilClosureEvaluatesTrue(
      base::BindLambdaForTesting([&]() -> bool {
        final_timestamp = base::TimeTicks::Now();
        final_frame_counter = GetNumVideoFrameCallbacks(first_tab);
        return final_frame_counter >=
               kTestTimeSeconds * kFps + initial_frame_counter;
      }),
      first_tab, base::Milliseconds(50)));
  int average_fps = (final_frame_counter - initial_frame_counter) * 1000 /
                    (final_timestamp - initial_timestamp).InMilliseconds();
  // MediaStreamVideoTrack upholds the min fps by way of an idle timer getting
  // reset for every received frame from the source. Sources being slow to
  // provide frames or plumbed main thread will ensure that the FPS provided is
  // actually always strictly lower than the requested minimum.
  // Expect at least 1/3 of the expected frames have appeared to aggressively
  // combat flakes.
  ASSERT_GE(average_fps, kFps / 3);
}

// TODO(crbug.com/40915051): Fails on Linux ASan, LSan and MSan builders.
#if BUILDFLAG(IS_LINUX) &&                                      \
    ((defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)) || \
     defined(MEMORY_SANITIZER))
#define MAYBE_TabCaptureProvides0HzWith0MinFpsConstraintAndStaticContent \
  DISABLED_TabCaptureProvides0HzWith0MinFpsConstraintAndStaticContent
#else
#define MAYBE_TabCaptureProvides0HzWith0MinFpsConstraintAndStaticContent \
  TabCaptureProvides0HzWith0MinFpsConstraintAndStaticContent
#endif
IN_PROC_BROWSER_TEST_F(
    WebRtcDesktopCaptureBrowserTest,
    MAYBE_TabCaptureProvides0HzWith0MinFpsConstraintAndStaticContent) {
  constexpr base::TimeDelta kTestTime = base::Seconds(2);
  InitializeTabSharingForFirstTab(
      base::BindOnce(GetDesktopMediaIDForTab, base::Unretained(browser()), 1),
      nullptr, base::StrCat({"minFrameRate: 0, maxFrameRate: 30"}));
  content::WebContents* first_tab =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EnableVideoFrameCallbacks(first_tab, "local-view");

  // Sample received frame counts during the test time.
  int frame_counter = 0;
  base::TimeTicks initial_timestamp = base::TimeTicks::Now();
  ASSERT_TRUE(test::PollingWaitUntilClosureEvaluatesTrue(
      base::BindLambdaForTesting([&]() -> bool {
        frame_counter = GetNumVideoFrameCallbacks(first_tab);
        return base::TimeTicks::Now() - initial_timestamp >= kTestTime;
      }),
      first_tab, base::Milliseconds(50)));
  // Expect only a few initial frames.
  ASSERT_LE(frame_counter, 3);
}

// Flaky on ASan bots. See https://crbug.com/40270173.
// Crashes on some Macs. See https://crbug.com/351095634.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || BUILDFLAG(IS_MAC)
#define MAYBE_RunP2PScreenshareWhileSharingScreen \
  DISABLED_RunP2PScreenshareWhileSharingScreen
#else
#define MAYBE_RunP2PScreenshareWhileSharingScreen \
  RunP2PScreenshareWhileSharingScreen
#endif
IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       MAYBE_RunP2PScreenshareWhileSharingScreen) {
  RunP2PScreenshareWhileSharing(base::BindOnce(GetDesktopMediaIDForScreen));
}

// Flaky on ASan bots. See https://crbug.com/40270173.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_RunP2PScreenshareWhileSharingTab \
  DISABLED_RunP2PScreenshareWhileSharingTab
#else
#define MAYBE_RunP2PScreenshareWhileSharingTab RunP2PScreenshareWhileSharingTab
#endif
IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       MAYBE_RunP2PScreenshareWhileSharingTab) {
  RunP2PScreenshareWhileSharing(
      base::BindOnce(GetDesktopMediaIDForTab, base::Unretained(browser()), 2));
}

IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       SwitchSharedTabBackAndForth) {
  InfobarUIChangeObserver observer(browser());

  InitializeTabSharingForFirstTab(
      base::BindOnce(GetDesktopMediaIDForTab, base::Unretained(browser()), 2),
      &observer);

  // Should delete 3 infobars and create 3 new!
  observer.ExpectCalls(6);
  // Switch shared tab from 2 to 0.
  GetDelegate(browser(), 0)->ShareThisTabInstead();
  observer.Wait();

  // Should delete 3 infobars and create 3 new!
  observer.ExpectCalls(6);
  // Switch shared tab from 0 to 2.
  GetDelegate(browser(), 2)->ShareThisTabInstead();
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(WebRtcDesktopCaptureBrowserTest,
                       CloseAndReopenNonSharedTab) {
  InfobarUIChangeObserver observer(browser());

  InitializeTabSharingForFirstTab(
      base::BindOnce(GetDesktopMediaIDForTab, base::Unretained(browser()), 2),
      &observer);

  // Should delete 1 infobar.
  observer.ExpectCalls(1);
  // Close non-shared and non-sharing, i.e., unrelated tab
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  observer.Wait();

  // Should create 1 infobar.
  observer.ExpectCalls(1);
  // Restore tab
  chrome::RestoreTab(browser());
  observer.Wait();
}
