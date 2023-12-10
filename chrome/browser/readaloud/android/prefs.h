// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_ANDROID_PREFS_H_
#define CHROME_BROWSER_READALOUD_ANDROID_PREFS_H_

#include "components/pref_registry/pref_registry_syncable.h"

namespace readaloud {

// Should be called once during startup in browser_prefs.cc.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_ANDROID_PREFS_H_
