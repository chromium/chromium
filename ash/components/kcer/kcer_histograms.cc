// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/kcer_histograms.h"

#include "base/metrics/histogram_functions.h"

namespace kcer {
namespace {
constexpr char kPkcs12MigrationHistogram[] = "ChromeOS.Kcer.Pkcs12Migration";
constexpr char kKcerErrorHistogram[] = "ChromeOS.Kcer.Error";
}  // namespace

namespace internal {

void RecordKcerPkcs12ImportUmaEvent(internal::KcerPkcs12ImportEvent event) {
  base::UmaHistogramEnumeration(internal::KcerPkcs12ImportMetrics, event);
}

}  // namespace internal

void RecordPkcs12MigrationUmaEvent(Pkcs12MigrationUmaEvent event) {
  base::UmaHistogramEnumeration(kPkcs12MigrationHistogram, event);
}

void RecordKcerError(Error error) {
  base::UmaHistogramEnumeration(kKcerErrorHistogram, error);
}

}  // namespace kcer
