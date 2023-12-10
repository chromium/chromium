// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace base {

void NotEnums() {
  // Sample and boundary values must both be enums.
  enum EnumA { A };
  enum EnumB { B };

  UmaHistogramEnumeration("", A, 2);  // expected-error {{no matching function for call to 'UmaHistogramEnumeration'}}
  UmaHistogramEnumeration("", 1, B);  // expected-error {{no matching function for call to 'UmaHistogramEnumeration'}}
  UmaHistogramEnumeration("", 1, 2);  // expected-error@*:* {{static assertion failed due to requirement 'std::is_enum_v<int>'}}
}

void DifferentEnums() {
  // Sample and boundary values must not come from different enums.
  enum EnumA { A };
  enum EnumB { B };
  enum class EnumC { C };
  enum class EnumD { D };

  UMA_HISTOGRAM_ENUMERATION("", A, B);                // expected-error {{|sample| and |boundary| shouldn't be of different enums}}
  UMA_HISTOGRAM_ENUMERATION("", A, EnumD::D);         // expected-error {{|sample| and |boundary| shouldn't be of different enums}}
  UMA_HISTOGRAM_ENUMERATION("", EnumC::C, B);         // expected-error {{|sample| and |boundary| shouldn't be of different enums}}
  UMA_HISTOGRAM_ENUMERATION("", EnumC::C, EnumD::D);  // expected-error {{|sample| and |boundary| shouldn't be of different enums}}

  UmaHistogramEnumeration("", A, B);  // expected-error {{no matching function for call to 'UmaHistogramEnumeration'}}
}

void MaxOutOfRange() {
  // Boundaries must be nonnegative and fit in an int.
  enum class TypeA { A = -1 };
  enum class TypeB : uint32_t { B = 0xffffffff };

  UMA_HISTOGRAM_ENUMERATION("", TypeA::A, TypeA::A);  // expected-error {{|boundary| is out of range of HistogramBase::Sample}}
  UMA_HISTOGRAM_ENUMERATION("", TypeB::B, TypeB::B);  // expected-error {{|boundary| is out of range of HistogramBase::Sample}}
}

void NoMaxValue() {
  // When boundary is omitted, sample enum must define `kMaxValue`.
  enum class NoMax { kVal };

  UmaHistogramEnumeration("", NoMax::kVal);  // expected-error@*:* {{no member named 'kMaxValue' in 'NoMax'}}
}

}  // namespace base
