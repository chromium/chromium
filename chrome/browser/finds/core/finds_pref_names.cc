// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_pref_names.h"

namespace finds::prefs {

// The timestamp of the last successful model execution for the Finds service.
const char kFindsModelExecutionLastTimestamp[] =
    "finds.model_execution.last_timestamp";

// A dictionary storing the last time a user marked a theme as "not
// interested".
const char kFindsNotInterestedThemesLastTimestamp[] =
    "finds.themes.not_interested_last_timestamp";

}  // namespace finds::prefs
