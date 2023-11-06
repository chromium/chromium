// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_access_auth_timeout_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/base/features.h"

namespace extensions {

PasswordAccessAuthTimeoutHandler::PasswordAccessAuthTimeoutHandler() = default;

PasswordAccessAuthTimeoutHandler::~PasswordAccessAuthTimeoutHandler() = default;

void PasswordAccessAuthTimeoutHandler::Init(TimeoutCallback timeout_call) {
  timeout_call_ = std::move(timeout_call);
}

// static
base::TimeDelta PasswordAccessAuthTimeoutHandler::GetAuthValidityPeriod() {
  if (!base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
    return base::Seconds(60);
  }
  return syncer::kPasswordNotesAuthValidity.Get();
}

void PasswordAccessAuthTimeoutHandler::RestartAuthTimer() {
  if (timeout_timer_.IsRunning()) {
    timeout_timer_.Reset();
  }
}

void PasswordAccessAuthTimeoutHandler::OnUserReauthenticationResult(
    bool authenticated) {
  if (authenticated) {
    CHECK(!timeout_call_.is_null());
    timeout_timer_.Start(FROM_HERE, GetAuthValidityPeriod(),
                         base::BindRepeating(timeout_call_));
  }
}

}  // namespace extensions
