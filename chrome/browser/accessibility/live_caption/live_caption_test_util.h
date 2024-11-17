// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_TEST_UTIL_H_
#define CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_TEST_UTIL_H_

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace captions {

// Base class for Live Caption browsertests. An InProcessBrowserTest with
// additional helpers. See in_process_browser_test.h.
class LiveCaptionBrowserTest : public InProcessBrowserTest {
 public:
  LiveCaptionBrowserTest() = default;
  ~LiveCaptionBrowserTest() override = default;
  LiveCaptionBrowserTest(const LiveCaptionBrowserTest&) = delete;
  LiveCaptionBrowserTest& operator=(const LiveCaptionBrowserTest&) = delete;

  // InProcessBrowserTest:
  void SetUp() override;
  void CreatedBrowserMainParts(content::BrowserMainParts*) override;

 protected:
  // Enables/disables the live caption pref on the specified profile (or default
  // profile) and marks the SODA library as installed.
  void SetLiveCaptionEnabled(bool enabled);

  void SetLiveCaptionEnabledOnProfile(bool enabled, Profile* profile);

  // Enables/disables the live translate pref.
  void SetLiveTranslateEnabled(bool enabled);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_TEST_UTIL_H_
