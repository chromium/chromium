// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"

#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"

namespace bound_session_credentials {

Timestamp TimeToTimestamp(base::Time time) {
  Timestamp timestamp = Timestamp();
  timestamp.set_microseconds(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return timestamp;
}

base::Time TimestampToTime(const Timestamp& timestamp) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(timestamp.microseconds()));
}

}  // namespace bound_session_credentials
