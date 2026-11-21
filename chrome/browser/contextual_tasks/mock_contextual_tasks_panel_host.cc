// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_host.h"

#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

MockContextualTasksPanelHost::MockContextualTasksPanelHost() {
  ON_CALL(*this, Show).WillByDefault([this](AnimationStyle) {
    is_open_ = true;
    if (web_contents_) {
      web_contents_->WasShown();
    }
  });
  ON_CALL(*this, Close).WillByDefault([this](AnimationStyle) {
    is_open_ = false;
    if (web_contents_ && !web_contents_->IsBeingDestroyed()) {
      web_contents_->WasHidden();
    }
  });
}

MockContextualTasksPanelHost::~MockContextualTasksPanelHost() = default;

void MockContextualTasksPanelHost::SetWebContents(
    content::WebContents* web_contents) {
  if (web_contents_.get() == web_contents) {
    return;
  }
  if (is_open_ && web_contents_ && !web_contents_->IsBeingDestroyed()) {
    web_contents_->WasHidden();
  }
  web_contents_ = web_contents ? web_contents->GetWeakPtr() : nullptr;
  if (is_open_ && web_contents_) {
    web_contents_->WasShown();
  }
}

void MockContextualTasksPanelHost::SetErrorPageVisible(bool visible) {
  is_error_page_visible_ = visible;
}

bool MockContextualTasksPanelHost::IsErrorPageVisible() const {
  return is_error_page_visible_;
}

}  // namespace contextual_tasks
