// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CUSTOMTABS_CHROME_ORIGIN_VERIFIER_H_
#define CHROME_BROWSER_ANDROID_CUSTOMTABS_CHROME_ORIGIN_VERIFIER_H_

namespace customtabs {

// JNI bridge for ChromeOriginVerifier.java
class ChromeOriginVerifier {
 public:
  ChromeOriginVerifier(const ChromeOriginVerifier&) = delete;
  ChromeOriginVerifier& operator=(const ChromeOriginVerifier&) = delete;

  static void ClearBrowsingData();
  static int GetClearBrowsingDataCallCountForTesting();

 private:
  static int clear_browsing_data_call_count_for_tests_;
};

}  // namespace customtabs

#endif  // CHROME_BROWSER_ANDROID_CUSTOMTABS_CHROME_ORIGIN_VERIFIER_H_
