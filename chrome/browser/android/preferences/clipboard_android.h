// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PREFERENCES_CLIPBOARD_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PREFERENCES_CLIPBOARD_ANDROID_H_

class PrefService;
class PrefRegistrySimple;

namespace android {

void RegisterClipboardAndroidPrefs(PrefRegistrySimple* registry);

void InitClipboardAndroidFromLocalState(PrefService* local_state);

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_PREFERENCES_CLIPBOARD_ANDROID_H_
