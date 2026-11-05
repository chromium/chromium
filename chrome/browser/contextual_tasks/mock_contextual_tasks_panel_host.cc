// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_host.h"

namespace contextual_tasks {

MockContextualTasksPanelHost::MockContextualTasksPanelHost() = default;

MockContextualTasksPanelHost::~MockContextualTasksPanelHost() = default;

void MockContextualTasksPanelHost::SetErrorPageVisible(bool visible) {
  is_error_page_visible_ = visible;
}

bool MockContextualTasksPanelHost::IsErrorPageVisible() const {
  return is_error_page_visible_;
}

}  // namespace contextual_tasks
