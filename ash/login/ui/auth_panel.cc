// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_panel.h"
#include "base/notreached.h"

namespace ash {

AuthPanel::AuthPanel() = default;

AuthPanel::~AuthPanel() = default;

void AuthPanel::InitializeUi(AuthFactorsSet factors,
                             AuthHubConnector* connector) {
  NOTIMPLEMENTED();
}

void AuthPanel::OnFactorListChanged(FactorsStatusMap factors_with_status) {
  NOTIMPLEMENTED();
}

void AuthPanel::OnFactorStatusesChanged(FactorsStatusMap incremental_update) {
  NOTIMPLEMENTED();
}

void AuthPanel::OnFactorAuthFailure(AshAuthFactor factor) {
  NOTIMPLEMENTED();
}

void AuthPanel::OnFactorAuthSuccess(AshAuthFactor factor) {
  NOTIMPLEMENTED();
}

void AuthPanel::OnEndAuthentication() {
  NOTIMPLEMENTED();
}

}  // namespace ash
