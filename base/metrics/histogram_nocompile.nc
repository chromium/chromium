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
  //
  // Note: the boundary enumerators are deliberately non-zero, to avoid also
  // tripping the "`boundary` must be greater than 0" assertion
  enum EnumA { A };
  enum EnumB { B = 1 };
  enum class EnumC { C };
  enum class EnumD { D = 1 };

  UMA_HISTOGRAM_ENUMERATION("", A, B);                // expected-error {{`sample` and `boundary` shouldn't be of different enums}}
  UMA_HISTOGRAM_ENUMERATION("", A, EnumD::D);         // expected-error {{`sample` and `boundary` shouldn't be of different enums}}
  UMA_HISTOGRAM_ENUMERATION("", EnumC::C, B);         // expected-error {{`sample` and `boundary` shouldn't be of different enums}}
  UMA_HISTOGRAM_ENUMERATION("", EnumC::C, EnumD::D);  // expected-error {{`sample` and `boundary` shouldn't be of different enums}}

  UmaHistogramEnumeration("", A, B);  // expected-error {{no matching function for call to 'UmaHistogramEnumeration'}}
}

void MaxOutOfRange() {
  // Boundaries must be positive and fit in an int.
  enum class TypeA { A = -1 };
  enum class TypeB : uint32_t { B = 0xffffffff };
  enum class TypeC { C = 0 };

  UMA_HISTOGRAM_ENUMERATION("", TypeA::A, TypeA::A);  // expected-error {{`boundary` is out of range of HistogramBase::Sample32}}
  UMA_HISTOGRAM_ENUMERATION("", TypeB::B, TypeB::B);  // expected-error {{`boundary` is out of range of HistogramBase::Sample32}}
  UMA_HISTOGRAM_ENUMERATION("", TypeC::C, TypeC::C);  // expected-error {{`boundary` must be greater than 0}}
}

void NoMaxValue() {
  // When boundary is omitted, sample enum must define `kMaxValue`.
  enum class NoMax { kVal };

  UmaHistogramEnumeration("", NoMax::kVal);  // expected-error@*:* {{no member named 'kMaxValue' in 'NoMax'}}
}

// Enums that define a `kMaxValue` enumerator may not manually specify a
// boundary value.
void HasMaxValue() {
  enum class SomeEnum {
    kZero,
    kOne,
    kMaxValue = kOne,
  };

  UmaHistogramEnumeration("", SomeEnum::kZero, SomeEnum::kMaxValue);  // expected-error@*:* {{use `base::UmaHistogramEnumeration(name, sample)` instead}}

  UMA_HISTOGRAM_ENUMERATION("", SomeEnum::kZero, SomeEnum::kMaxValue);  // expected-error {{omit the boundary argument so it is deduced from `kMaxValue`}}
  UMA_HISTOGRAM_ENUMERATION("", SomeEnum::kZero, 2);                    // expected-error {{omit the boundary argument so it is deduced from `kMaxValue`}}
}

}  // namespace base
