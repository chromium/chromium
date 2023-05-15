// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_PREFS_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_PREFS_H_

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace enterprise_reporting {

extern const char kLastUploadVersion[];
extern const char kCloudExtensionRequestUploadedIds[];

extern const char kCloudLegacyTechReportAllowlist[];

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_PREFS_H_
