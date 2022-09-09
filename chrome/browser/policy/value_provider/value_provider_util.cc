// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/value_provider/value_provider_util.h"

#include <utility>

#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_service.h"

policy::PolicyService* GetPolicyService(Profile* profile) {
  return profile->GetProfilePolicyConnector()->policy_service();
}
