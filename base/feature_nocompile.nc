// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/feature_list.h"

namespace base {

// 1. BASE_FEATURE with FEATURE_ENABLED_FOR_COUNTRIES passed as the default state.
// Should fail static_assert(!base::IsCountrySpecificFeatureState(default_state))
// expected-error@+1 {{variable does not have a constant initializer}}
BASE_FEATURE(kFeatureInvalidDefault, "FeatureInvalidDefault", FEATURE_ENABLED_FOR_COUNTRIES);  // expected-error@*:* {{static assertion failed due to requirement '!base::IsCountrySpecificFeatureState}}

// 2. BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS with an incompatible default state passed in.
// Should fail static_assert(base::IsCountrySpecificFeatureState(default_state))
// expected-error@+1 {{variable does not have a constant initializer}}
BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kFeatureInvalidState, FEATURE_ENABLED_BY_DEFAULT, "de");  // expected-error@*:* {{static assertion failed due to requirement 'base::IsCountrySpecificFeatureState}}

// 3. BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS with an empty country list (braced).
// Should fail deduction of MakeCountryCodeStorage template arguments.
BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kFeatureEmptyCountries, FEATURE_ENABLED_FOR_COUNTRIES, {});  // expected-error@*:* {{MakeCountryCodeStorage}}

// 4. BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS with country codes with the wrong amount of letters.
// Should fail static_assert(countries.AreCodesValid(), ...)
BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kFeatureWrongLetters, FEATURE_ENABLED_FOR_COUNTRIES, "de", "eng");  // expected-error@*:* {{All country parameters must consist of two characters between a and z}}

// 5. BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS with capitalized country codes.
// Should fail static_assert(countries.AreCodesValid(), ...)
BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kFeatureCapitalized, FEATURE_ENABLED_FOR_COUNTRIES, "DE");  // expected-error@*:* {{All country parameters must consist of two characters between a and z}}

// 6. BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS with no country arguments at all.
// Should fail static_assert(sizeof...(Args) > 0, "The country list must be non-empty")
BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(kFeatureEmptyArgs, FEATURE_ENABLED_FOR_COUNTRIES);  // expected-error@*:* {{The country list must be non-empty}}

}  // namespace base
