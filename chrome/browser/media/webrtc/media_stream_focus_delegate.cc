// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_focus_delegate.h"

#include "build/build_config.h"

#if defined(OS_ANDROID)
#error "Unsupported on Android."
#endif  // defined(OS_ANDROID)

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

MediaStreamFocusDelegate::MediaStreamFocusDelegate(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!web_contents) {
    return;
  }

  Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
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
                                        bool focus) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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

  if (!capturing_web_contents_) {
    return false;
  }

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
  Browser* const browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser && browser->window()) {
    browser->window()->Activate();
  }
}

void MediaStreamFocusDelegate::FocusWindow(
    const content::DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<webrtc::DesktopCapturer> window_capturer =
      webrtc::DesktopCapturer::CreateWindowCapturer(
          content::desktop_capture::CreateDesktopCaptureOptions());
  if (window_capturer && window_capturer->SelectSource(media_id.id)) {
    window_capturer->FocusOnSelectedSource();
  }
}
