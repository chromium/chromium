// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/byte_size.h"

#include <limits>

#include "base/rand_util.h"

namespace base {

void ConstructByteSizeFromSigned() {
  // Any runtime signed value not allowed.
  [[maybe_unused]] ByteSize bytes(RandInt(-10, 10));  // expected-error {{call to consteval function}}

  // Negative constant not allowed.
  [[maybe_unused]] ByteSize bytes2(-1);  // expected-error {{call to consteval function}}
}

void ConstructByteSizeFromFloat() {
  [[maybe_unused]] ByteSize bytes(1.0);  // expected-error {{calling a private constructor}}
}

void ConstructByteSizeFromOtherUnit() {
  // Any runtime signed value not allowed.
  int i = RandInt(-10, 10);
  [[maybe_unused]] ByteSize kib = KiBU(i);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize mib = MiBU(i);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize gib = GiBU(i);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize tib = TiBU(i);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize pib = PiBU(i);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize eib = EiBU(i);  // expected-error {{not a constant expression}}

  // Negative constant not allowed.
  [[maybe_unused]] ByteSize kib2 = KiBU(-1);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize mib2 = MiBU(-1);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize gib2 = GiBU(-1);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize tib2 = TiBU(-1);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize pib2 = PiBU(-1);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize eib2 = EiBU(-1);  // expected-error {{not a constant expression}}

  // Out-of-range signed constant not allowed.
  constexpr int64_t kLargeInt = std::numeric_limits<int64_t>::max();
  [[maybe_unused]] ByteSize kib3 = KiBU(kLargeInt);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize mib3 = MiBU(kLargeInt);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize gib3 = GiBU(kLargeInt);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize tib3 = TiBU(kLargeInt);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize pib3 = PiBU(kLargeInt);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSize eib3 = EiBU(kLargeInt);  // expected-error {{not a constant expression}}
}

void ConstructByteSizeDeltaFromUnsigned() {
  // Any runtime unsigned value not allowed (require explicit cast).
  unsigned u = static_cast<unsigned>(RandInt(0, 10));
  [[maybe_unused]] ByteSizeDelta delta(u);  // expected-error {{call to consteval function}}

  // Out-of-range constant not allowed.
  [[maybe_unused]] ByteSizeDelta delta2(std::numeric_limits<uint64_t>::max());  // expected-error {{call to consteval function}}
}

void ConstructByteSizeDeltaFromFloat() {
  [[maybe_unused]] ByteSizeDelta delta(1.0);  // expected-error {{calling a private constructor}}
}

void ConstructByteSizeDeltaFromOtherUnit() {
  // Any runtime unsigned value not allowed (require explicit cast).
  unsigned u = static_cast<unsigned>(RandInt(0, 10));
  [[maybe_unused]] ByteSizeDelta kib = KiBS(u);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta mib = MiBS(u);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta gib = GiBS(u);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta tib = TiBS(u);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta pib = PiBS(u);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta eib = EiBS(u);  // expected-error {{not a constant expression}}

  // Out-of-range unsigned constant not allowed.

  constexpr uint64_t kLargeUnsigned = std::numeric_limits<int64_t>::max();
  [[maybe_unused]] ByteSizeDelta kib3 = KiBS(kLargeUnsigned);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta mib3 = MiBS(kLargeUnsigned);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta gib3 = GiBS(kLargeUnsigned);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta tib3 = TiBS(kLargeUnsigned);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta pib3 = PiBS(kLargeUnsigned);  // expected-error {{not a constant expression}}
  [[maybe_unused]] ByteSizeDelta eib3 = EiBS(kLargeUnsigned);  // expected-error {{not a constant expression}}
}

}  // namespace base
