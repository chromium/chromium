// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/desk_profiles_delegate.h"

namespace ash {

LacrosProfileSummary::LacrosProfileSummary() = default;
LacrosProfileSummary::LacrosProfileSummary(const LacrosProfileSummary&) =
    default;
LacrosProfileSummary::LacrosProfileSummary(LacrosProfileSummary&&) = default;
LacrosProfileSummary& LacrosProfileSummary::operator=(
    const LacrosProfileSummary&) = default;
LacrosProfileSummary& LacrosProfileSummary::operator=(LacrosProfileSummary&&) =
    default;

uint64_t DeskProfilesDelegate::ResolveProfileId(uint64_t profile_id) {
  return profile_id ? profile_id : GetPrimaryProfileId();
}

}  // namespace ash
