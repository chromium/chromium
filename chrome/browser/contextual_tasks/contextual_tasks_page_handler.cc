// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"

#include "base/logging.h"
#include "url/gurl.h"

ContextualTasksPageHandler::ContextualTasksPageHandler(
    mojo::PendingRemote<contextual_tasks::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler)
    : page_(std::move(page)), page_handler_(this, std::move(page_handler)) {}

ContextualTasksPageHandler::~ContextualTasksPageHandler() = default;

void ContextualTasksPageHandler::GetThreadUrl(GetThreadUrlCallback callback) {
  // TODO(447633840): This is a placeholder URL until the real page is ready.
  std::move(callback).Run(GURL("https://google.com"));
}
