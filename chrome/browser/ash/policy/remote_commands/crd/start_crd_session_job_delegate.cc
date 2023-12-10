// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/start_crd_session_job_delegate.h"

namespace policy {

using SessionParameters = StartCrdSessionJobDelegate::SessionParameters;

SessionParameters::SessionParameters() = default;
SessionParameters::~SessionParameters() = default;

SessionParameters::SessionParameters(const SessionParameters&) = default;
SessionParameters& SessionParameters::operator=(const SessionParameters&) =
    default;
SessionParameters::SessionParameters(SessionParameters&&) = default;
SessionParameters& SessionParameters::operator=(SessionParameters&&) = default;

}  // namespace policy
