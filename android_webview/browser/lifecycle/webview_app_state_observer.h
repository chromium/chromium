// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_LIFECYCLE_WEBVIEW_APP_STATE_OBSERVER_H_
#define ANDROID_WEBVIEW_BROWSER_LIFECYCLE_WEBVIEW_APP_STATE_OBSERVER_H_

namespace android_webview {

// The interface for being notified of app state change, the implementation
// shall be added to observer list through AwContentsLifecycleNotifier.
class WebViewAppStateObserver {
 public:
  enum class State {
    // All WebViews are detached from window.
    kUnknown,
    // At least one WebView is foreground.
    kForeground,
    // No WebView is foreground and at least one WebView is background.
    kBackground,
    // All WebViews are destroyed or no WebView has been created.
    // Observers shall use
    // AwContentsLifecycleNotifier::has_aw_contents_ever_created() to find if A
    // WebView has ever been created.
    kDestroyed,
  };

  WebViewAppStateObserver();
  virtual ~WebViewAppStateObserver();

  // Invoked when app state is changed or right after this observer is added
  // into observer list.
  virtual void OnAppStateChanged(State state) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_LIFECYCLE_WEBVIEW_APP_STATE_OBSERVER_H_
