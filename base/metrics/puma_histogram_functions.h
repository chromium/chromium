// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_PUMA_HISTOGRAM_FUNCTIONS_H_
#define BASE_METRICS_PUMA_HISTOGRAM_FUNCTIONS_H_

// Private UMA (PUMA) is an alternative metrics collection system to UMA. It
// allows collecting metrics with e.g. reduced system information.
//
// Emitting to PUMA histograms requires using separate functions, which are
// defined below and are based on the standard UMA histogram functions.
// See //base/metrics/histogram_functions.h.
// Each function accepts an additional PumaType parameter which discriminates
// between different PUMA types.
//
// Implementation note: PUMA histogram values are emitted with a different set
// of flags in order to be distinguished from regular UMA histograms. The flag
// kUmaTargetedHistogramFlag is not being set for PUMA, and instead a separate
// PUMA-specific flag is set. For instance, for PUMA for Regional Capabilities
// it's kPumaRcTargetedHistogramFlag.

#include <stdint.h>

#include <string_view>

#include "base/base_export.h"
#include "base/check_op.h"
#include "base/metrics/histogram_base.h"

namespace base {

// PumaType discriminates between different Private UMA metrics collection
// systems. Different collection systems can e.g. use different backends or
// provide different system information.
//
// Implementation note: values of these enums should contain the final set of
// flags that the given type of histogram should have. This provides 0-cost
// mapping between PumaType and HistogramBase::Flags.
enum class PumaType : uint16_t {
  // Private UMA for Regional Capabilities.
  kRc = HistogramBase::Flags::kPumaRcTargetedHistogramFlag,
};

// Converts the given PumaType to histogram flags that should be applied to
// records emitted with this PumaType.
constexpr HistogramBase::Flags PumaTypeToHistogramFlags(PumaType puma_type) {
  return static_cast<HistogramBase::Flags>(static_cast<uint16_t>(puma_type));
}

// PUMA version of base::UmaHistogramBoolean().
BASE_EXPORT void PumaHistogramBoolean(PumaType puma_type,
                                      std::string_view name,
                                      bool sample);

// PUMA version of base::UmaHistogramExactLinear().
BASE_EXPORT void PumaHistogramExactLinear(PumaType puma_type,
                                          std::string_view name,
                                          int sample,
                                          int exclusive_max);

// PUMA version of base::UmaHistogramEnumeration().
template <typename T>
void PumaHistogramEnumeration(PumaType puma_type,
                              std::string_view name,
                              T sample) {
  static_assert(std::is_enum_v<T>, "T is not an enum.");
  // This also ensures that an enumeration that doesn't define kMaxValue fails
  // with a semi-useful error ("no member named 'kMaxValue' in ...").
  static_assert(static_cast<uintmax_t>(T::kMaxValue) <=
                    static_cast<uintmax_t>(INT_MAX) - 1,
                "Enumeration's kMaxValue is out of range of INT_MAX!");
  DCHECK_LE(static_cast<uintmax_t>(sample),
            static_cast<uintmax_t>(T::kMaxValue));
  return PumaHistogramExactLinear(puma_type, name, static_cast<int>(sample),
                                  static_cast<int>(T::kMaxValue) + 1);
}

// PUMA version of base::UmaHistogramEnumeration().
template <typename T>
void PumaHistogramEnumeration(PumaType puma_type,
                              std::string_view name,
                              T sample,
                              T enum_size) {
  static_assert(std::is_enum_v<T>, "T is not an enum.");
  DCHECK_LE(static_cast<uintmax_t>(enum_size), static_cast<uintmax_t>(INT_MAX));
  DCHECK_LT(static_cast<uintmax_t>(sample), static_cast<uintmax_t>(enum_size));
  return PumaHistogramExactLinear(puma_type, name, static_cast<int>(sample),
                                  static_cast<int>(enum_size));
}

}  // namespace base

#endif  // BASE_METRICS_PUMA_HISTOGRAM_FUNCTIONS_H_
