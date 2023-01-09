// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_default_utils.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/reporting/metrics/metric_rate_controller.h"

namespace reporting::metrics {
namespace {

const base::TimeDelta GetDefaultRate(base::TimeDelta default_rate,
                                     base::TimeDelta testing_rate) {
  // If telemetry testing rates flag is enabled, use `testing_rate` to reduce
  // time before metric collection and reporting.
  return base::FeatureList::IsEnabled(kEnableTelemetryTestingRates)
             ? testing_rate
             : default_rate;
}

}  // namespace

const base::TimeDelta GetDefaultReportUploadFrequency() {
  return GetDefaultRate(kDefaultReportUploadFrequency,
                        kDefaultReportUploadFrequencyForTesting);
}

const base::TimeDelta GetDefaultCollectionRate(base::TimeDelta default_rate) {
  return GetDefaultRate(default_rate, kDefaultCollectionRateForTesting);
}

const base::TimeDelta GetDefaultEventCheckingRate(
    base::TimeDelta default_rate) {
  return GetDefaultRate(default_rate, kDefaultEventCheckingRateForTesting);
}

// static
base::TimeDelta InitDelayParam::init_delay = base::Minutes(1);

// static
void InitDelayParam::SetForTesting(const base::TimeDelta& delay) {
  InitDelayParam::init_delay = delay;
}

// static
const base::TimeDelta InitDelayParam::Get() {
  return InitDelayParam::init_delay;
}

// static
// 5 seconds is what was recommended by the upstream maintainer of FWUPD
base::TimeDelta PeripheralCollectionDelayParam::collection_delay_ =
    base::Seconds(5);

// static
void PeripheralCollectionDelayParam::SetForTesting(
    const base::TimeDelta& delay) {
  PeripheralCollectionDelayParam::collection_delay_ = delay;
}

// static
const base::TimeDelta PeripheralCollectionDelayParam::Get() {
  return PeripheralCollectionDelayParam::collection_delay_;
}
}  // namespace reporting::metrics
