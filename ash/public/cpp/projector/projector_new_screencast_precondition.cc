// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"

#include "base/values.h"

namespace ash {

namespace {
constexpr char kState[] = "state";
constexpr char kReasons[] = "reasons";
}  // namespace

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

base::Value NewScreencastPrecondition::ToValue() const {
  base::Value::Dict result;
  result.Set(kState, static_cast<int>(state));

  base::Value::List reasons_value;
  for (const auto& reason : reasons)
    reasons_value.Append(static_cast<int>(reason));

  result.Set(kReasons, std::move(reasons_value));
  return base::Value(std::move(result));
}

bool NewScreencastPrecondition::operator==(
    const NewScreencastPrecondition& rhs) const {
  return rhs.state == state && rhs.reasons == reasons;
}

}  // namespace ash
