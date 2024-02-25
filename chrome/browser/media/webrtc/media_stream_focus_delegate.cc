// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_focus_delegate.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#error "Unsupported on Android."
#endif  // BUILDFLAG(IS_ANDROID)

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ConditionalFocusDecision {
  kExplicitFocusCapturedSurface = 0,
  kExplicitNoFocusChange = 1,
  kMicrotaskClosedWindow = 2,
  kBrowserSideTimerClosedWindow = 3,
  kMaxValue = kBrowserSideTimerClosedWindow
};
using Decision = ConditionalFocusDecision;

// Readability-enhancing aliases.
constexpr bad_message::BadMessageReason MSFD_MULTIPLE_EXPLICIT_CALLS_TO_FOCUS =
    bad_message::BadMessageReason::MSFD_MULTIPLE_EXPLICIT_CALLS_TO_FOCUS;
constexpr bad_message::BadMessageReason
    MSFD_MULTIPLE_CLOSURES_OF_FOCUSABILITY_WINDOW = bad_message::
        BadMessageReason::MSFD_MULTIPLE_CLOSURES_OF_FOCUSABILITY_WINDOW;
}  // namespace

MediaStreamFocusDelegate::MediaStreamFocusDelegate(
    content::WebContents* web_contents)
    : capture_start_time_(base::TimeTicks::Now()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents) {
    return;
  }

  Browser* const browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }

  TabStripModel* const tab_strip_model = browser->tab_strip_model();

  if (!tab_strip_model) {
    return;
  }

  tab_strip_model->AddObserver(this);

  capturing_web_contents_ = web_contents->GetWeakPtr();
}

MediaStreamFocusDelegate::~MediaStreamFocusDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void MediaStreamFocusDelegate::SetFocus(const content::DesktopMediaID& media_id,
                                        bool focus,
                                        bool is_from_microtask,
                                        bool is_from_timer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!capturing_web_contents_) {
    return;
  }

  if (!UpdateUMA(focus, is_from_microtask, is_from_timer)) {
    return;  // Render process killed off - |capturing_web_contents_| invalid.
  }

  if (!focus_window_of_opportunity_open_) {
    return;  // Too late.
  }
  focus_window_of_opportunity_open_ = false;

  if (!focus) {
    return;  // Window of opportunity to change focus now closed - we're done.
  }

  if (!IsWidgetFocused()) {
    // Capturing window not focused - likely the user has alt-tabbed away.
    return;
  }

  if (media_id.type == content::DesktopMediaID::TYPE_WEB_CONTENTS) {
    FocusTab(media_id);
  } else if (media_id.type == content::DesktopMediaID::TYPE_WINDOW) {
    FocusWindow(media_id);
  }
}

void MediaStreamFocusDelegate::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  focus_window_of_opportunity_open_ = false;
}

bool MediaStreamFocusDelegate::IsWidgetFocused() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(capturing_web_contents_);  // Tested by caller.

  content::RenderWidgetHostView* const rwhv =
      capturing_web_contents_->GetRenderWidgetHostView();
  if (!rwhv) {
    return false;
  }

  return rwhv->HasFocus();
}

void MediaStreamFocusDelegate::FocusTab(
    const content::DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* const rfh = content::RenderFrameHost::FromID(
      media_id.web_contents_id.render_process_id,
      media_id.web_contents_id.main_render_frame_id);
  if (!rfh) {
    return;
  }

  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return;
  }

  content::WebContentsDelegate* const delegate = web_contents->GetDelegate();
  if (!delegate) {
    return;
  }

  delegate->ActivateContents(web_contents);
  Browser* const browser = chrome::FindBrowserWithTab(web_contents);
  if (browser && browser->window()) {
    browser->window()->Activate();
  }
}

void MediaStreamFocusDelegate::FocusWindow(
    const content::DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<webrtc::DesktopCapturer> window_capturer =
      window_capturer_for_testing_ != nullptr
          ? std::move(window_capturer_for_testing_)
          : webrtc::DesktopCapturer::CreateWindowCapturer(
                content::desktop_capture::CreateDesktopCaptureOptions());
  if (window_capturer && window_capturer->SelectSource(media_id.id)) {
    window_capturer->FocusOnSelectedSource();
  }
}

bool MediaStreamFocusDelegate::UpdateUMA(bool focus,
                                         bool is_from_microtask,
                                         bool is_from_timer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(capturing_web_contents_);               // Tested by caller.
  DCHECK(!is_from_microtask || !is_from_timer);  // Can't be both.

  const bool explicit_decision = (!is_from_microtask && !is_from_timer);
  // Invocations from the microtask/timer focus the captured display surface.
  DCHECK(explicit_decision || focus);

  // The shape and result of the API invocation is only recorded once,
  // on the first invocation that has an effect.
  if (focus_window_of_opportunity_open_) {
    base::UmaHistogramEnumeration(
        "Media.ConditionalFocus.Decision",
        is_from_microtask
            ? Decision::kMicrotaskClosedWindow
            : is_from_timer ? Decision::kBrowserSideTimerClosedWindow
                            : focus ? Decision::kExplicitFocusCapturedSurface
                                    : Decision::kExplicitNoFocusChange);
  }

  const base::TimeDelta delay = base::TimeTicks::Now() - capture_start_time_;

  if (explicit_decision) {
    if (explicit_decision_) {
      return BadMessage(MSFD_MULTIPLE_EXPLICIT_CALLS_TO_FOCUS);
    }
    explicit_decision_ = true;

    if (!microtask_fired_ && !timer_expired_) {  // Timely API invocation.
      // Record the delay of this on-time explicit API invocation.
      // Note that 1s corresponds to the value GetConditionalFocusWindow()
      // returns by default.
      UMA_HISTOGRAM_CUSTOM_TIMES("Media.ConditionalFocus.ExplicitOnTimeCall",
                                 delay, base::Milliseconds(1), base::Seconds(1),
                                 100);
    } else if (timer_expired_) {  // Late (compared to browser-side timer).
      // Record the delay of this late explicit API invocation.
      // Note that 1s corresponds to the value GetConditionalFocusWindow()
      // returns by default.
      UMA_HISTOGRAM_CUSTOM_TIMES("Media.ConditionalFocus.ExplicitLateCall",
                                 delay, base::Seconds(1), base::Seconds(5),
                                 100);
    } else {  // microtask_fired_
      // The case of |microtask_fired_| is not currently measured.
      // It's an error on the Web-application's part and addressable by the app.
    }
  }

  if (is_from_microtask) {
    if (microtask_fired_) {
      return BadMessage(MSFD_MULTIPLE_CLOSURES_OF_FOCUSABILITY_WINDOW);
    }

    UMA_HISTOGRAM_CUSTOM_TIMES("Media.ConditionalFocus.MicrotaskDelay", delay,
                               base::Milliseconds(1), base::Seconds(5), 100);

    microtask_fired_ = true;
  }

  if (is_from_timer) {
    DCHECK(!timer_expired_);  // The timer can only expire once.
    timer_expired_ = true;
  }

  return true;
}

bool MediaStreamFocusDelegate::BadMessage(
    bad_message::BadMessageReason reason) {
  content::RenderFrameHost* const rfh =
      capturing_web_contents_->GetPrimaryMainFrame();
  if (rfh) {
    bad_message::ReceivedBadMessage(rfh->GetProcess(), reason);
  }
  return false;
}
