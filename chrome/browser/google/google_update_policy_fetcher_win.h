// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_WIN_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_WIN_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_conversions.h"

namespace policy {
class PolicyMap;
}

struct GoogleUpdateState {
  std::wstring version;
  base::Time last_checked_time;
};

struct GoogleUpdatePoliciesAndState {
  GoogleUpdatePoliciesAndState();
  ~GoogleUpdatePoliciesAndState();
  std::unique_ptr<policy::PolicyMap> policies;
  std::unique_ptr<GoogleUpdateState> state;
};

// Returns a list of all the Google Update policies available through the
// IPolicyStatus COM interface.
base::Value GetGoogleUpdatePolicyNames();

// Returns a list of all the Google Update policies available through the
// IPolicyStatus COM interface.
policy::PolicyConversions::PolicyToSchemaMap GetGoogleUpdatePolicySchemas();

// Fetches all the Google Update Policies and state values available through the
// IPolicyStatus2 or IPolicyStatus COM interface. Only the policies that have
// been set are returned by this function. This function returns null if the
// fetch fails because IPolicyStatus interface could not be instantiated. This
// function must run on a COM STA thread because it makes some COM calls.
std::unique_ptr<GoogleUpdatePoliciesAndState> GetGoogleUpdatePoliciesAndState();

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_WIN_H_
