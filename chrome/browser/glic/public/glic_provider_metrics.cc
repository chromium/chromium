// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_metrics.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_view_util.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace glic {

namespace {

// Emits both "Glic.<infix>.<name>" and "Glic.<name>" using a single
// stack-allocated buffer (falling back to the heap if > 128 chars).
void RecordBoth(std::string_view infix,
                std::string_view name,
                base::FunctionRef<void(std::string_view)> fn) {
  constexpr std::string_view kPrefix = "Glic.";
  const size_t total_len = kPrefix.size() + infix.size() + 1 + name.size();
  absl::InlinedVector<char, 128> buf(total_len);
  base::span<char> span(buf);

  span.first(kPrefix.size()).copy_from(kPrefix);
  span.subspan(kPrefix.size(), infix.size()).copy_from(infix);
  span[kPrefix.size() + infix.size()] = '.';
  span.subspan(kPrefix.size() + infix.size() + 1).copy_from(name);

  fn(base::as_string_view(span));

  base::span<char> combined = span.subspan(infix.size() + 1);
  combined.first(kPrefix.size()).copy_from(kPrefix);
  fn(base::as_string_view(combined));
}

}  // namespace

GlicMetricReporter::GlicMetricReporter(Profile* profile)
    : infix_(GetGlicProviderSpec(profile).GetMetricsInfix()) {}

void GlicMetricReporter::RecordBoolean(std::string_view name,
                                       bool sample) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramBoolean(full_name, sample);
  });
}

void GlicMetricReporter::RecordCounts100(std::string_view name,
                                         int sample) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramCounts100(full_name, sample);
  });
}

void GlicMetricReporter::RecordCounts1000(std::string_view name,
                                          int sample) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramCounts1000(full_name, sample);
  });
}

void GlicMetricReporter::RecordCounts10000(std::string_view name,
                                           int sample) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramCounts10000(full_name, sample);
  });
}

void GlicMetricReporter::RecordCustomTimes(std::string_view name,
                                           base::TimeDelta sample,
                                           base::TimeDelta min,
                                           base::TimeDelta max,
                                           int buckets) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramCustomTimes(full_name, sample, min, max, buckets);
  });
}

void GlicMetricReporter::RecordExactLinear(std::string_view name,
                                           int sample,
                                           int exclusive_max) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramExactLinear(full_name, sample, exclusive_max);
  });
}

void GlicMetricReporter::RecordLongTimes(std::string_view name,
                                         base::TimeDelta sample) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramLongTimes(full_name, sample);
  });
}

void GlicMetricReporter::RecordMediumTimes(std::string_view name,
                                           base::TimeDelta sample) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramMediumTimes(full_name, sample);
  });
}

void GlicMetricReporter::RecordMemoryLargeMB(std::string_view name,
                                             int sample) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramMemoryLargeMB(full_name, sample);
  });
}

void GlicMetricReporter::RecordSparse(std::string_view name, int sample) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramSparse(full_name, sample);
  });
}

void GlicMetricReporter::RecordTimes(std::string_view name,
                                     base::TimeDelta sample) const {
  RecordBoth(infix_, name, [&](std::string_view full_name) {
    base::UmaHistogramTimes(full_name, sample);
  });
}

}  // namespace glic
