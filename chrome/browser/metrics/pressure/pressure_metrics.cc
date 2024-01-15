// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/pressure/pressure_metrics.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace {
constexpr char kSomePrefix[] = "some";
constexpr char kFullPrefix[] = "full";
}  // namespace

PressureMetrics::PressureMetrics(const char* histogram_name,
                                 base::FilePath metric_path)
    : histogram_name_(histogram_name), metric_path_(std::move(metric_path)) {}

PressureMetrics::~PressureMetrics() = default;

std::optional<PressureMetrics::Sample> PressureMetrics::CollectCurrentPressure()
    const {
  std::string content;
  if (!ReadFileToString(metric_path_, &content) || content.empty()) {
    return std::nullopt;
  }

  // Example file content:
  //  some avg10=0.12 avg60=0.00 avg300=0.00 total=123456
  //  full avg10=0.01 avg60=0.00 avg300=0.00 total=776655
  std::vector<std::string> lines = base::SplitString(
      content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (lines.size() != 2 || !base::StartsWith(lines[0], kSomePrefix) ||
      !base::StartsWith(lines[1], kFullPrefix)) {
    return std::nullopt;
  }

  base::StringPairs some_kv_pairs;
  base::StringPairs full_kv_pairs;
  if (lines[0].size() <= std::size(kSomePrefix) ||
      !base::SplitStringIntoKeyValuePairs(
          lines[0].substr(std::size(kSomePrefix)), '=', ' ', &some_kv_pairs) ||
      some_kv_pairs.size() != 4 || lines[1].size() <= std::size(kFullPrefix) ||
      !base::SplitStringIntoKeyValuePairs(
          lines[1].substr(std::size(kFullPrefix)), '=', ' ', &full_kv_pairs) ||
      full_kv_pairs.size() != 4) {
    return std::nullopt;
  }

  Sample sample;
  if (!base::StringToDouble(some_kv_pairs[0].second, &sample.some_avg10) ||
      !base::StringToDouble(some_kv_pairs[1].second, &sample.some_avg60) ||
      !base::StringToDouble(some_kv_pairs[2].second, &sample.some_avg300) ||
      !base::StringToDouble(full_kv_pairs[0].second, &sample.full_avg10) ||
      !base::StringToDouble(full_kv_pairs[1].second, &sample.full_avg60) ||
      !base::StringToDouble(full_kv_pairs[2].second, &sample.full_avg300)) {
    return std::nullopt;
  }

  return sample;
}

void PressureMetrics::EmitCounters(const Sample& sample) const {
  TRACE_COUNTER1("resources", histogram_name_, sample.some_avg10);
}

void PressureMetrics::ReportToUMA(const Sample& sample) const {
  base::UmaHistogramPercentage(histogram_name_, sample.some_avg10);
}
