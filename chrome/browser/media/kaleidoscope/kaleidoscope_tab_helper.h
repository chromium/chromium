// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_TAB_HELPER_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class KaleidoscopeTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<KaleidoscopeTabHelper> {
 public:
  static const char kKaleidoscopeNavigationHistogramName[];
  static const char kKaleidoscopeOpenedMediaRecommendationHistogramName[];

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class KaleidoscopeNavigation {
    kNormal = 0,
    kMaxValue = kNormal,
  };

  ~KaleidoscopeTabHelper() override;
  KaleidoscopeTabHelper(const KaleidoscopeTabHelper&) = delete;
  KaleidoscopeTabHelper& operator=(const KaleidoscopeTabHelper&) = delete;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override;
  void WebContentsDestroyed() override;

  // A tab is Kaleidoscope derived if the tab was opened by Kaleidoscope and
  // remains on the same origin.
  bool IsKaleidoscopeDerived() const { return is_kaleidoscope_derived_; }

  // A tab is successful if it had a Kaleidoscope session in it that resulted
  // in the user opening another tab.
  void MarkAsSuccessful() { was_successful_ = true; }

 private:
  friend class content::WebContentsUserData<KaleidoscopeTabHelper>;

  explicit KaleidoscopeTabHelper(content::WebContents* web_contents);

  void RecordMetricsOnNavigation(content::NavigationHandle* handle);
  void SetAutoplayOnNavigation(content::NavigationHandle* handle);
  void OnKaleidoscopeSessionEnded();

  bool is_kaleidoscope_derived_ = false;
  bool was_successful_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_TAB_HELPER_H_
