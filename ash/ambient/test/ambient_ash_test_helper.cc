// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_ash_test_helper.h"

#include "ash/ambient/test/test_ambient_client.h"

namespace ash {

AmbientAshTestHelper::AmbientAshTestHelper() = default;

AmbientAshTestHelper::~AmbientAshTestHelper() = default;

void AmbientAshTestHelper::IssueAccessToken(bool is_empty) {
  ambient_client_.IssueAccessToken(is_empty);
}

bool AmbientAshTestHelper::IsAccessTokenRequestPending() const {
  return ambient_client_.IsAccessTokenRequestPending();
}

}  // namespace ash
