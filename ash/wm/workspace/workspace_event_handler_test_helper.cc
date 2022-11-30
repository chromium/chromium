// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler_test_helper.h"

namespace ash {

WorkspaceEventHandlerTestHelper::WorkspaceEventHandlerTestHelper(
    WorkspaceEventHandler* handler)
    : handler_(handler) {}

WorkspaceEventHandlerTestHelper::~WorkspaceEventHandlerTestHelper() = default;

}  // namespace ash
