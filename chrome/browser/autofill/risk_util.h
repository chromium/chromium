// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_RISK_UTIL_H_
#define CHROME_BROWSER_AUTOFILL_RISK_UTIL_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"

namespace content {
class WebContents;
}

namespace autofill {

// Loads risk data for the client, getting the device's risk fingerprint before
// calling |callback|. |obfuscated_gaia_id| is used in the fingerprinting
// process if provided. |web_contents| is used during fingerprinting as well,
// when retrieving user prefs, and in determining window bounds when not on
// Android.
void LoadRiskData(uint64_t obfuscated_gaia_id,
                  content::WebContents* web_contents,
                  base::OnceCallback<void(const std::string&)> callback);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_RISK_UTIL_H_
