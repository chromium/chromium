// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_LOW_RISK_ORIGINS_H_
#define CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_LOW_RISK_ORIGINS_H_

#include "url/origin.h"

namespace digital_credentials {

// Returns whether the origin is a known low risk origin for which the
// digital credential interstitial should not be shown regardless of the
// credential being requested.
bool IsLowRiskOrigin(const url::Origin& to_check);

}  // namespace digital_credentials

#endif  // CHROME_BROWSER_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_LOW_RISK_ORIGINS_H_
