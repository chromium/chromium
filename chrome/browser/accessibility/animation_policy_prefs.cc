// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/animation_policy_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

const char kAnimationPolicyAllowed[] = "allowed";
const char kAnimationPolicyOnce[] = "once";
const char kAnimationPolicyNone[] = "none";

void RegisterAnimationPolicyPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kAnimationPolicy,
                               kAnimationPolicyAllowed);
}
