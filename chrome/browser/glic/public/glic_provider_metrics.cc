// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_metrics.h"

#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"

namespace glic {

namespace {

// "X" -> "Glic.X".
std::string GlicCombinedMetricName(std::string_view name) {
  return base::StrCat({"Glic.", name});
}

// ("Consumer", "X") -> "Glic.Consumer.X".
std::string GlicProviderMetricName(std::string_view infix,
                                   std::string_view name) {
  return base::StrCat({"Glic.", infix, ".", name});
}

}  // namespace

GlicMetricReporter::GlicMetricReporter(Profile* profile)
    : infix_(GetGlicProviderSpec(profile).GetMetricsInfix()) {}

void GlicMetricReporter::RecordBoolean(std::string_view name,
                                       bool sample) const {
  base::UmaHistogramBoolean(GlicCombinedMetricName(name), sample);
  base::UmaHistogramBoolean(GlicProviderMetricName(infix_, name), sample);
}

void GlicMetricReporter::RecordCounts100(std::string_view name,
                                         int sample) const {
  base::UmaHistogramCounts100(GlicCombinedMetricName(name), sample);
  base::UmaHistogramCounts100(GlicProviderMetricName(infix_, name), sample);
}

void GlicMetricReporter::RecordCounts1000(std::string_view name,
                                          int sample) const {
  base::UmaHistogramCounts1000(GlicCombinedMetricName(name), sample);
  base::UmaHistogramCounts1000(GlicProviderMetricName(infix_, name), sample);
}

void GlicMetricReporter::RecordCounts10000(std::string_view name,
                                           int sample) const {
  base::UmaHistogramCounts10000(GlicCombinedMetricName(name), sample);
  base::UmaHistogramCounts10000(GlicProviderMetricName(infix_, name), sample);
}

void GlicMetricReporter::RecordCustomTimes(std::string_view name,
                                           base::TimeDelta sample,
                                           base::TimeDelta min,
                                           base::TimeDelta max,
                                           int buckets) const {
  base::UmaHistogramCustomTimes(GlicCombinedMetricName(name), sample, min, max,
                                buckets);
  base::UmaHistogramCustomTimes(GlicProviderMetricName(infix_, name), sample,
                                min, max, buckets);
}

void GlicMetricReporter::RecordExactLinear(std::string_view name,
                                           int sample,
                                           int exclusive_max) const {
  base::UmaHistogramExactLinear(GlicCombinedMetricName(name), sample,
                                exclusive_max);
  base::UmaHistogramExactLinear(GlicProviderMetricName(infix_, name), sample,
                                exclusive_max);
}

void GlicMetricReporter::RecordLongTimes(std::string_view name,
                                         base::TimeDelta sample) const {
  base::UmaHistogramLongTimes(GlicCombinedMetricName(name), sample);
  base::UmaHistogramLongTimes(GlicProviderMetricName(infix_, name), sample);
}

void GlicMetricReporter::RecordMediumTimes(std::string_view name,
                                           base::TimeDelta sample) const {
  base::UmaHistogramMediumTimes(GlicCombinedMetricName(name), sample);
  base::UmaHistogramMediumTimes(GlicProviderMetricName(infix_, name), sample);
}

void GlicMetricReporter::RecordMemoryLargeMB(std::string_view name,
                                             int sample) const {
  base::UmaHistogramMemoryLargeMB(GlicCombinedMetricName(name), sample);
  base::UmaHistogramMemoryLargeMB(GlicProviderMetricName(infix_, name), sample);
}

void GlicMetricReporter::RecordSparse(std::string_view name, int sample) const {
  base::UmaHistogramSparse(GlicCombinedMetricName(name), sample);
  base::UmaHistogramSparse(GlicProviderMetricName(infix_, name), sample);
}

void GlicMetricReporter::RecordTimes(std::string_view name,
                                     base::TimeDelta sample) const {
  base::UmaHistogramTimes(GlicCombinedMetricName(name), sample);
  base::UmaHistogramTimes(GlicProviderMetricName(infix_, name), sample);
}

}  // namespace glic
