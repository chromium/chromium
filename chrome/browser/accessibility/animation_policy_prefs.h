// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ANIMATION_POLICY_PREFS_H_
#define CHROME_BROWSER_ACCESSIBILITY_ANIMATION_POLICY_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

extern const char kAnimationPolicyAllowed[];
extern const char kAnimationPolicyOnce[];
extern const char kAnimationPolicyNone[];

void RegisterAnimationPolicyPrefs(user_prefs::PrefRegistrySyncable* registry);

#endif  // CHROME_BROWSER_ACCESSIBILITY_ANIMATION_POLICY_PREFS_H_
