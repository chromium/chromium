// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_focus_delegate.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#error "Unsupported on Android."
#endif  // BUILDFLAG(IS_ANDROID)

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
// Readability-enhancing aliases.
constexpr bad_message::BadMessageReason MSFD_MULTIPLE_EXPLICIT_CALLS_TO_FOCUS =
    bad_message::BadMessageReason::MSFD_MULTIPLE_EXPLICIT_CALLS_TO_FOCUS;
constexpr bad_message::BadMessageReason
    MSFD_MULTIPLE_CLOSURES_OF_FOCUSABILITY_WINDOW = bad_message::
        BadMessageReason::MSFD_MULTIPLE_CLOSURES_OF_FOCUSABILITY_WINDOW;
}  // namespace

MediaStreamFocusDelegate::MediaStreamFocusDelegate(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents) {
    return;
  }

  BrowserWindowInterface* const browser =
      chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }

  TabStripModel* const tab_strip_model = browser->GetTabStripModel();

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

  if (!ValidateCall(focus, is_from_microtask, is_from_timer)) {
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
  BrowserWindowInterface* const browser =
      chrome::FindBrowserWithTab(web_contents);
  if (browser && browser->GetWindow()) {
    browser->GetWindow()->Activate();
  }
}

void MediaStreamFocusDelegate::FocusWindow(
    const content::DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<webrtc::DesktopCapturer> window_capturer =
      window_capturer_for_testing_ != nullptr
          ? std::move(window_capturer_for_testing_)
          : content::desktop_capture::CreateWindowCapturer(
                content::desktop_capture::CreateDesktopCaptureOptions());
  if (window_capturer && window_capturer->SelectSource(media_id.id)) {
    window_capturer->FocusOnSelectedSource();
  }
}

bool MediaStreamFocusDelegate::ValidateCall(bool focus,
                                            bool is_from_microtask,
                                            bool is_from_timer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(capturing_web_contents_);               // Tested by caller.
  DCHECK(!is_from_microtask || !is_from_timer);  // Can't be both.

  const bool explicit_decision = (!is_from_microtask && !is_from_timer);
  // Invocations from the microtask/timer focus the captured display surface.
  DCHECK(explicit_decision || focus);

  if (explicit_decision) {
    if (explicit_decision_) {
      return BadMessage(MSFD_MULTIPLE_EXPLICIT_CALLS_TO_FOCUS);
    }
    explicit_decision_ = true;
  }

  if (is_from_microtask) {
    if (microtask_fired_) {
      return BadMessage(MSFD_MULTIPLE_CLOSURES_OF_FOCUSABILITY_WINDOW);
    }

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
