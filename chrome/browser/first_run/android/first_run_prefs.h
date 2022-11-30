// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_RUN_ANDROID_FIRST_RUN_PREFS_H_
#define CHROME_BROWSER_FIRST_RUN_ANDROID_FIRST_RUN_PREFS_H_

namespace first_run {

// Must also match definition in policy_templates.json.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.firstrun
enum class TosDialogBehavior { UNSET = 0, STANDARD = 1, SKIP = 2 };

extern const char kTosDialogBehavior[];

}  // namespace first_run

#endif  // CHROME_BROWSER_FIRST_RUN_ANDROID_FIRST_RUN_PREFS_H_
