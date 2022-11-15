// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_FOCUS_DELEGATE_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_FOCUS_DELEGATE_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#error "Unsupported on Android."
#endif  // BUILDFLAG(IS_ANDROID)

#include "base/time/time.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

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

  void SetFocus(const content::DesktopMediaID& media_id,
                bool focus,
                bool is_from_microtask,
                bool is_from_timer);

  // TabStripModelObserver implementation.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void SetWindowCapturerForTesting(
      std::unique_ptr<webrtc::DesktopCapturer> window_capturer) {
    window_capturer_for_testing_ = std::move(window_capturer);
  }

 private:
  bool IsWidgetFocused() const;
  void FocusTab(const content::DesktopMediaID& media_id);
  void FocusWindow(const content::DesktopMediaID& media_id);

  // Returns a bool representing the validity of the call.
  // If |false|, the call was found to be invalid, the capturer's render
  // process was killed off, and execution of the focus-delegate logic
  // should not proceed.
  [[nodiscard]] bool UpdateUMA(bool focus,
                               bool is_from_microtask,
                               bool is_from_timer);

  // Kills off capturer render-process.
  // Returns |false| to make UpdateUMA()'s code a bit nicer.
  [[nodiscard]] bool BadMessage(bad_message::BadMessageReason reason);

  // UMA-related.
  const base::TimeTicks capture_start_time_;
  bool microtask_fired_ = false;
  bool timer_expired_ = false;
  bool explicit_decision_ = false;

  // |focus_window_of_opportunity_open_| tracks whether the window of
  // opportunity is open or closed.
  // |capturing_web_contents_| is used to check whether the Chrome window from
  // which the capture was initiated is still the active one. If not, then we
  // want to avoid yanking the user's focus around.
  base::WeakPtr<content::WebContents> capturing_web_contents_ = nullptr;
  bool focus_window_of_opportunity_open_ = true;

  std::unique_ptr<webrtc::DesktopCapturer> window_capturer_for_testing_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_STREAM_FOCUS_DELEGATE_H_
