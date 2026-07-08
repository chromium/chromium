// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/i18n/tags.h"

namespace base::i18n {

// "en-XX" is a valid BCP47 tag, but not in our known list of canonical tags in
// Chromium.
// expected-error@* {{not a constant expression}}
// expected-error@* {{declaration requires an exit-time destructor}}
// expected-note@* {{non-constexpr function 'ERROR_TagIsUnknown' cannot be used in a constant expression}}
auto tag1 = GetKnownLanguageTag("en-XX");

// expected-error@* {{not a constant expression}}
// expected-error@* {{declaration requires an exit-time destructor}}
// expected-note@* {{non-constexpr function 'ERROR_TagIsUnknown' cannot be used in a constant expression}}
auto tag2 = GetKnownLanguageTag("xx");

}  // namespace base::i18n
