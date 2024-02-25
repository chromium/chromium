// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_LIFECYCLE_AW_CONTENTS_LIFECYCLE_NOTIFIER_H_
#define ANDROID_WEBVIEW_BROWSER_LIFECYCLE_AW_CONTENTS_LIFECYCLE_NOTIFIER_H_

#include <map>

#include "android_webview/browser/lifecycle/webview_app_state_observer.h"
#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"

namespace android_webview {

class AwContents;

class AwContentsLifecycleNotifier {
 public:
  using OnLoseForegroundCallback = base::RepeatingClosure;

  enum class AwContentsState {
    // AwContents isn't attached to a window.
    kDetached,
    // AwContents is attached to a window and window is visible.
    kForeground,
    // AwContents is attached to a window and window is invisible.
    kBackground,
  };

  static AwContentsLifecycleNotifier& GetInstance();
  static void InitForTesting();

  // The |onLoseForegroundCallback| will be invoked after all observers when app
  // lose foreground.
  explicit AwContentsLifecycleNotifier(
      OnLoseForegroundCallback on_lose_foreground_callback);

  AwContentsLifecycleNotifier(const AwContentsLifecycleNotifier&) = delete;
  AwContentsLifecycleNotifier& operator=(const AwContentsLifecycleNotifier&) =
      delete;

  virtual ~AwContentsLifecycleNotifier();

  void OnWebViewCreated(const AwContents* aw_contents);
  void OnWebViewDestroyed(const AwContents* aw_contents);
  void OnWebViewAttachedToWindow(const AwContents* aw_contents);
  void OnWebViewDetachedFromWindow(const AwContents* aw_contents);
  void OnWebViewWindowBeVisible(const AwContents* aw_contents);
  void OnWebViewWindowBeInvisible(const AwContents* aw_contents);

  void AddObserver(WebViewAppStateObserver* observer);
  void RemoveObserver(WebViewAppStateObserver* observer);

  bool has_aw_contents_ever_created() const {
    return has_aw_contents_ever_created_;
  }

  std::vector<const AwContents*> GetAllAwContents() const;

 private:
  struct AwContentsData {
    AwContentsData();
    AwContentsData(AwContentsData&& data);

    AwContentsData(const AwContentsData&) = delete;

    ~AwContentsData();

    bool attached_to_window = false;
    bool window_visible = false;
    AwContentsState aw_content_state = AwContentsState::kDetached;
  };

  friend class TestAwContentsLifecycleNotifier;

  void EnsureOnValidSequence() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  size_t ToIndex(AwContentsState state) const;
  void OnAwContentsStateChanged(
      AwContentsLifecycleNotifier::AwContentsData* data);

  void UpdateAppState();

  bool HasAwContentsInstance() const;

  AwContentsLifecycleNotifier::AwContentsData* GetAwContentsData(
      const AwContents* aw_contents);

  // The AwContents to AwContentsData mapping.
  std::map<const AwContents*, AwContentsLifecycleNotifier::AwContentsData>
      aw_contents_to_data_;

  // The number of AwContents instances in each AwContentsState.
  int state_count_[3]{};

  bool has_aw_contents_ever_created_ = false;

  base::ObserverList<WebViewAppStateObserver>::Unchecked observers_;

  OnLoseForegroundCallback on_lose_foreground_callback_;

  WebViewAppStateObserver::State app_state_ =
      WebViewAppStateObserver::State::kDestroyed;

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_LIFECYCLE_AW_CONTENTS_LIFECYCLE_NOTIFIER_H_
