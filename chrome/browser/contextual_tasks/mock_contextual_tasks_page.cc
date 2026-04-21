// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"

namespace contextual_tasks {

MockContextualTasksPage::MockContextualTasksPage() = default;

MockContextualTasksPage::~MockContextualTasksPage() = default;

mojo::PendingRemote<mojom::Page> MockContextualTasksPage::BindAndGetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace contextual_tasks
