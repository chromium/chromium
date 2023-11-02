// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/collection_params.h"

#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

namespace metrics {

namespace {

// Returns a TimeDelta profile duration based on the current chrome channel.
base::TimeDelta ProfileDuration() {
  switch (chrome::GetChannel()) {
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      return base::Seconds(4);
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
    default:
      return base::Seconds(2);
  }
}

// Returns a TimeDelta interval duration for periodic collection based on the
// current chrome channel.
base::TimeDelta PeriodicCollectionInterval() {
  switch (chrome::GetChannel()) {
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      return base::Minutes(90);
    case version_info::Channel::STABLE:
    case version_info::Channel::UNKNOWN:
    default:
      return base::Minutes(180);
  }
}

}  // namespace

// Defines default collection parameters.
CollectionParams::CollectionParams() {
  collection_duration = ProfileDuration();
  periodic_interval = PeriodicCollectionInterval();

  resume_from_suspend.sampling_factor = 10;
  resume_from_suspend.max_collection_delay = base::Seconds(5);

  restore_session.sampling_factor = 10;
  restore_session.max_collection_delay = base::Seconds(10);
}

}  // namespace metrics
