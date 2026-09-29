// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/headless_web_contents_manager.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "content/public/browser/web_contents.h"

namespace actor {

HeadlessWebContentsManager::HeadlessWebContentsManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  CHECK(browser_context_);
}

HeadlessWebContentsManager::~HeadlessWebContentsManager() {
  while (!contents_.empty()) {
    Destroy(contents_.back().get());
  }
}

content::WebContents* HeadlessWebContentsManager::Create() {
  content::WebContents::CreateParams params(browser_context_);
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(params);
  contents->SetDelegate(this);
  return contents_.emplace_back(std::move(contents)).get();
}

void HeadlessWebContentsManager::Destroy(content::WebContents* contents) {
  auto it = std::ranges::find(contents_, contents,
                              &std::unique_ptr<content::WebContents>::get);
  CHECK(it != contents_.end());

  for (Observer& observer : observers_) {
    observer.OnHeadlessContentsWillBeDestroyed(contents);
  }

  // Unlist before destroying, so the dying WebContents is unreachable.
  std::unique_ptr<content::WebContents> owned = std::move(*it);
  contents_.erase(it);
}

void HeadlessWebContentsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HeadlessWebContentsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HeadlessWebContentsManager::CloseContents(content::WebContents* source) {
  Destroy(source);
}

}  // namespace actor
