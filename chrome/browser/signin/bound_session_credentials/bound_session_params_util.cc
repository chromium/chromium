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

bool AreParamsValid(const BoundSessionParams& bound_session_params) {
  // TODO(crbug.com/1441168): Check for validity of other fields once they are
  // available.
  // Note: The check for params validity checks for empty value as
  // `bound_session_params.has*()` doesn't check against explicitly set empty
  // value.
  return !bound_session_params.session_id().empty() &&
         !bound_session_params.site().empty() &&
         !bound_session_params.wrapped_key().empty();
}
}  // namespace bound_session_credentials
