// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_identity_low_risk_origins.h"

#include "url/gurl.h"
#include "url/origin.h"

namespace digital_credentials {
namespace {

/*
 * Temporary list of origins considered lower-risk to facilitate an experimental
 * test of the Digital Credential API while standardized trust signals are being
 * developed (such as in
 * https://github.com/WICG/digital-credentials/issues/136). This list is used
 * only as a heuristic for UI purposes, not to gate any API access. To submit
 * proposals for changes to this list, please file an issue at
 * https://bit.ly/cr-dc-origin-risk
 */
// TODO(https://crbug.com/350946977): Populate.
constexpr char* kKnownLowRiskOrigins[] = {};

}  // anonymous namespace

bool IsLowRiskOrigin(const url::Origin& to_check) {
  for (const char* low_risk_origin : kKnownLowRiskOrigins) {
    if (url::Origin::Create(GURL(low_risk_origin)) == to_check) {
      return true;
    }
  }
  return false;
}

}  // namespace digital_credentials
