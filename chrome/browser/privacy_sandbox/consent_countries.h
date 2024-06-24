// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_CONSENT_COUNTRIES_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_CONSENT_COUNTRIES_H_

#include "base/containers/fixed_flat_set.h"

namespace privacy_sandbox {

inline constexpr auto kPrivacySandboxConsentCountries =
    base::MakeFixedFlatSet<std::string_view>({
        "gb", "at", "ax", "be", "bg", "bl", "ch", "cy", "cz", "de", "dk",
        "ee", "es", "fi", "fr", "gf", "gg", "gi", "gp", "gr", "hr", "hu",
        "ie", "is", "it", "je", "ke", "li", "lt", "lu", "lv", "mf", "mt",
        "mq", "nc", "nl", "no", "pf", "pl", "pm", "pt", "qa", "re", "ro",
        "se", "si", "sk", "sj", "tf", "va", "wf", "yt",
    });

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_CONSENT_COUNTRIES_H_
