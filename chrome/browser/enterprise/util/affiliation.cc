// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/affiliation.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/affiliation.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chrome {
namespace enterprise_util {

bool IsProfileAffiliated(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (profile->IsMainProfile()) {
    return policy::PolicyLoaderLacros::IsMainUserAffiliated();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  return policy::IsAffiliated(
      profile->GetProfilePolicyConnector()->user_affiliation_ids(),
      g_browser_process->browser_policy_connector()->device_affiliation_ids());
}

}  // namespace enterprise_util
}  // namespace chrome
