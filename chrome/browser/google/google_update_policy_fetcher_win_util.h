// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_WIN_UTIL_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_WIN_UTIL_H_

#include <wtypes.h>
#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "components/policy/core/common/policy_map.h"

using PolicyValueOverrideFunction = base::RepeatingCallback<base::Value(BSTR)>;

// Converts a |policy| into a PolicyMap::Entry. |value_override_function|
// is an optional callback that modifies the value of the resulting policy.
std::unique_ptr<policy::PolicyMap::Entry> ConvertPolicyStatusValueToPolicyEntry(
    IPolicyStatusValue* policy,
    const PolicyValueOverrideFunction& value_override_function);

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_WIN_UTIL_H_
