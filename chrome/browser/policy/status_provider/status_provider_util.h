// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_STATUS_PROVIDER_UTIL_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_STATUS_PROVIDER_UTIL_H_

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"

extern const char kDevicePolicyStatusDescription[];
extern const char kUserPolicyStatusDescription[];

// Looks for "username" key in `dict` and sets "domain" key with the
// domain extracted from username.
void SetDomainExtractedFromUsername(base::Value::Dict& dict);

// Adds a new entry to |dict| with the affiliation status of the user associated
// with |profile|. This method shouldn't be called for device scope status.
void GetUserAffiliationStatus(base::Value::Dict* dict, Profile* profile);

// Adds a new entry to |dict| with the enterprise profile identifier of the
// current |profile|.
void SetProfileId(base::Value::Dict* dict, Profile* profile);

#if BUILDFLAG(IS_CHROMEOS_ASH)
void GetOffHoursStatus(base::Value::Dict* dict);

// Adds a new entry to |dict| with the enterprise domain manager of the user
// associated with |profile|. This method shouldn't be called for device scope
// status.
void GetUserManager(base::Value::Dict* dict, Profile* profile);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_STATUS_PROVIDER_UTIL_H_
