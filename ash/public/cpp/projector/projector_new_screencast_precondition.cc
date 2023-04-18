// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"

namespace ash {

NewScreencastPrecondition::NewScreencastPrecondition() = default;

NewScreencastPrecondition::NewScreencastPrecondition(
    NewScreencastPreconditionState new_state,
    const std::vector<NewScreencastPreconditionReason>& new_state_reason)
    : state(new_state), reasons(new_state_reason) {}

NewScreencastPrecondition::NewScreencastPrecondition(
    const NewScreencastPrecondition&) = default;

NewScreencastPrecondition& NewScreencastPrecondition::operator=(
    const NewScreencastPrecondition&) = default;

NewScreencastPrecondition::~NewScreencastPrecondition() = default;

bool NewScreencastPrecondition::operator==(
    const NewScreencastPrecondition& rhs) const {
  return rhs.state == state && rhs.reasons == reasons;
}

}  // namespace ash
