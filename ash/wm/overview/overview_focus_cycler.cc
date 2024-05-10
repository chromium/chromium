// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_focus_cycler.h"

namespace ash {

OverviewFocusCycler::OverviewFocusCycler(OverviewSession* overview_session)
    : overview_session_(overview_session) {}

OverviewFocusCycler::~OverviewFocusCycler() = default;

void OverviewFocusCycler::MoveFocus(bool reverse) {}

}  // namespace ash
