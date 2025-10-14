// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"

#include "url/gurl.h"

namespace contextual_tasks {

namespace {
constexpr char kAiPageHost[] = "https://google.com";
}  // namespace

ContextualTasksUiService::ContextualTasksUiService() {
  ai_page_host_ = GURL(kAiPageHost);
}

ContextualTasksUiService::~ContextualTasksUiService() = default;

const GURL& ContextualTasksUiService::GetAiPageHost() {
  return ai_page_host_;
}

void ContextualTasksUiService::OnThreadLinkClicked() {
  // TODO(449161768): Hook this up to the navigation throttle and implement.
}

}  // namespace contextual_tasks
