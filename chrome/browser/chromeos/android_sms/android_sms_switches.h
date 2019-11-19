// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SWITCHES_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SWITCHES_H_

namespace switches {

// When specified with a url string as parameter, the given url overrides the
// Android Messages for Web url used by AndroidSmsService.
extern const char kAlternateAndroidMessagesUrl[];

// When specified with a url string as parameter, the given url overrides the
// Android Messages for Web PWA installation url used by AndroidSmsService.
extern const char kAlternateAndroidMessagesInstallUrl[];

}  // namespace switches

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SWITCHES_H_
