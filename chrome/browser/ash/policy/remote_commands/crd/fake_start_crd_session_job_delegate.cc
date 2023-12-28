// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/fake_start_crd_session_job_delegate.h"

#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// static
constexpr char FakeStartCrdSessionJobDelegate::kTestAccessCode[];

FakeStartCrdSessionJobDelegate::FakeStartCrdSessionJobDelegate() = default;
FakeStartCrdSessionJobDelegate::~FakeStartCrdSessionJobDelegate() = default;

void FakeStartCrdSessionJobDelegate::TerminateCrdSession(
    const base::TimeDelta& session_duration) {
  if (session_finished_callback_.has_value()) {
    std::move(session_finished_callback_.value()).Run(session_duration);
  }
}

StartCrdSessionJobDelegate::SessionParameters
FakeStartCrdSessionJobDelegate::session_parameters() const {
  EXPECT_TRUE(received_session_parameters_.has_value());
  return received_session_parameters_.value_or(SessionParameters{});
}

bool FakeStartCrdSessionJobDelegate::HasActiveSession() const {
  return has_active_session_;
}

void FakeStartCrdSessionJobDelegate::TerminateSession() {
  has_active_session_ = false;
  terminate_session_called_ = true;
}

void FakeStartCrdSessionJobDelegate::StartCrdHostAndGetCode(
    const SessionParameters& parameters,
    AccessCodeCallback success_callback,
    ErrorCallback error_callback,
    SessionEndCallback session_finished_callback) {
  received_session_parameters_ = parameters;
  session_finished_callback_ = std::move(session_finished_callback);

  if (error_) {
    std::move(error_callback).Run(error_.value(), "");
    error_.reset();
  } else {
    std::move(success_callback).Run(kTestAccessCode);
  }
}

}  // namespace policy
