// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_STATUS_PROVIDER_UTIL_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_STATUS_PROVIDER_UTIL_H_

#include <optional>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/resources/webui/mojom/policy.mojom-forward.h"

extern const char kDevicePolicyStatusDescription[];
extern const char kUserPolicyStatusDescription[];

// Looks for "username" key in `dict` and sets "domain" key with the domain
// extracted from username.
void SetDomainExtractedFromUsername(base::DictValue& dict);
void SetDomainExtractedFromUsername(policy::mojom::StatusPtr& status);

// Returns the affiliation status of the user associated
// with |profile|. This method shouldn't be called for device scope status.
// Returns nullopt if the affiliation status can't be determined.
std::optional<bool> GetUserAffiliationStatus(Profile* profile);

// Returns the enterprise profile identifier of the `profile`.
// Returns nullopt if the profile id can't be determined.
std::optional<std::string> GetProfileId(Profile* profile);

#if BUILDFLAG(IS_CHROMEOS)
std::optional<bool> GetOffHoursStatus();

// Adds a new entry to |dict| with the enterprise domain manager of the user
// associated with |profile|. This method shouldn't be called for device scope
// status.
void GetUserManager(base::DictValue* dict, Profile* profile);
#endif  // BUILDFLAG(IS_CHROMEOS)

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_STATUS_PROVIDER_UTIL_H_
