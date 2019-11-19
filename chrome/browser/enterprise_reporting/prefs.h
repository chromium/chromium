// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_PREFS_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_PREFS_H_

#include "components/prefs/pref_registry_simple.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace enterprise_reporting {

extern const char kLastUploadTimestamp[];

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_PREFS_H_
