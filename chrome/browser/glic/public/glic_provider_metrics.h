// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_METRICS_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_METRICS_H_

#include <string_view>
#include <type_traits>

#include "base/time/time.h"

class Profile;

namespace glic {

// Records Glic histograms under both the combined name (Glic.X) and the
// per-provider name (Glic.Consumer.X / Glic.Enterprise.X). Resolves the
// provider once at construction (Consumer when the profile is null or
// non-regular), so it is cheap to copy and safe to use during teardown.
// `name` is the histogram name without the leading `Glic.`.
class GlicMetricReporter {
 public:
  explicit GlicMetricReporter(Profile* profile);

  std::string_view infix() const { return infix_; }

  void RecordBoolean(std::string_view name, bool sample) const;
  void RecordCounts100(std::string_view name, int sample) const;
  void RecordCounts1000(std::string_view name, int sample) const;
  void RecordCounts10000(std::string_view name, int sample) const;
  void RecordCustomTimes(std::string_view name,
                         base::TimeDelta sample,
                         base::TimeDelta min,
                         base::TimeDelta max,
                         int buckets) const;
  void RecordExactLinear(std::string_view name,
                         int sample,
                         int exclusive_max) const;

  template <typename T>
  void RecordEnumeration(std::string_view name, T sample) const {
    static_assert(std::is_enum_v<T>);
    RecordExactLinear(name, static_cast<int>(sample),
                      static_cast<int>(T::kMaxValue) + 1);
  }

  template <typename T>
  void RecordEnumeration(std::string_view name, T sample, T enum_size) const {
    static_assert(std::is_enum_v<T>);
    RecordExactLinear(name, static_cast<int>(sample),
                      static_cast<int>(enum_size));
  }

  void RecordLongTimes(std::string_view name, base::TimeDelta sample) const;
  void RecordMediumTimes(std::string_view name, base::TimeDelta sample) const;
  void RecordMemoryLargeMB(std::string_view name, int sample) const;
  void RecordSparse(std::string_view name, int sample) const;
  void RecordTimes(std::string_view name, base::TimeDelta sample) const;

  static GlicMetricReporter CreateForInfixForTesting(std::string_view infix) {
    return GlicMetricReporter(infix);
  }

 private:
  explicit GlicMetricReporter(std::string_view infix) : infix_(infix) {}
  std::string_view infix_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_METRICS_H_
