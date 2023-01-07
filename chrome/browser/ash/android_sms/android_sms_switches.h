// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_SWITCHES_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_SWITCHES_H_

namespace switches {

// When specified with a url string as parameter, the given url overrides the
// Android Messages for Web PWA installation and app urls using a base of the
// given domain with approrpiate suffixes.
extern const char kCustomAndroidMessagesDomain[];

}  // namespace switches

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_SWITCHES_H_
