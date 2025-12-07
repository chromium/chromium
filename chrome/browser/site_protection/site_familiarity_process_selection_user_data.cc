// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_process_selection_user_data.h"

namespace site_protection {

PROCESS_SELECTION_USER_DATA_KEY_IMPL(SiteFamiliarityProcessSelectionUserData);

SiteFamiliarityProcessSelectionUserData::
    SiteFamiliarityProcessSelectionUserData(bool is_site_familiar)
    : is_site_familiar_(is_site_familiar) {}

SiteFamiliarityProcessSelectionUserData::
    ~SiteFamiliarityProcessSelectionUserData() = default;

}  // namespace site_protection
