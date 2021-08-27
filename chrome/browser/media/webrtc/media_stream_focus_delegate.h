// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_FOCUS_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_FOCUS_DELEGATE_H_

#include "build/build_config.h"

#if defined(OS_ANDROID)
#error "Unsupported on Android."
#endif  // defined(OS_ANDROID)

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents.h"

// When tab/window-capture is initiated, a window of opportunity opens,
// during which the render process can instruct the browser process as to
// whether to focus the tab/window or not.
// The window of opportunity is closed when either:
// (a) The render process sends its decision.
// (b) After 1s elapses (MediaStreamManager independently calls SetFocus).
// (c) The user changes the focused tab.
// (d) The user changes the activated window.
class MediaStreamFocusDelegate : public TabStripModelObserver {
 public:
  explicit MediaStreamFocusDelegate(content::WebContents* web_contents);

  ~MediaStreamFocusDelegate() override;

  MediaStreamFocusDelegate(const MediaStreamFocusDelegate&) = delete;
  MediaStreamFocusDelegate& operator=(const MediaStreamFocusDelegate&) = delete;

  void SetFocus(const content::DesktopMediaID& media_id, bool focus);

  // TabStripModelObserver implementation.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  bool IsWidgetFocused() const;
  void FocusTab(const content::DesktopMediaID& media_id);
  void FocusWindow(const content::DesktopMediaID& media_id);

  // |focus_window_of_opportunity_open_| tracks whether the window of
  // opportunity is open or closed.
  // |capturing_web_contents_| is used to check whether the Chrome window from
  // which the capture was initiated is still the active one. If not, then we
  // want to avoid yanking the user's focus around.
  base::WeakPtr<content::WebContents> capturing_web_contents_ = nullptr;
  bool focus_window_of_opportunity_open_ = true;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_FOCUS_DELEGATE_H_
