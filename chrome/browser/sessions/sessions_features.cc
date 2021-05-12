// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Feature for session-only data deletion on startup.

#include "chrome/browser/sessions/sessions_features.h"

const base::Feature kDeleteSessionOnlyDataOnStartup{
    "DeleteSessionOnlyDataOnStartup", base::FEATURE_ENABLED_BY_DEFAULT};
