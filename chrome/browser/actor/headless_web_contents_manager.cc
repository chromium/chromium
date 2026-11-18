// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/headless_web_contents_manager.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "chrome/browser/actor/headless_page_host.h"
#include "chrome/browser/actor/javascript_dialog_interceptor.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"

namespace actor {

HeadlessWebContentsManager::HeadlessPage::HeadlessPage(
    std::unique_ptr<content::WebContents> contents)
    : contents(std::move(contents)),
      host(std::make_unique<HeadlessPageHost>(this->contents.get(),
                                              kViewportSize)) {}
HeadlessWebContentsManager::HeadlessPage::HeadlessPage(
    HeadlessPage&&) noexcept = default;
HeadlessWebContentsManager::HeadlessPage&
HeadlessWebContentsManager::HeadlessPage::operator=(HeadlessPage&&) noexcept =
    default;
HeadlessWebContentsManager::HeadlessPage::~HeadlessPage() = default;

HeadlessWebContentsManager::HeadlessWebContentsManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  CHECK(browser_context_);
}

HeadlessWebContentsManager::~HeadlessWebContentsManager() {
  while (!pages_.empty()) {
    Destroy(pages_.back().contents.get());
  }
}

content::WebContents* HeadlessWebContentsManager::Create() {
  content::WebContents::CreateParams params(browser_context_);
  std::unique_ptr<content::WebContents> contents =
      content::WebContents::Create(params);
  contents->SetDelegate(this);
  contents->Resize(gfx::Rect(kViewportSize));
  return pages_.emplace_back(std::move(contents)).contents.get();
}

void HeadlessWebContentsManager::Destroy(content::WebContents* contents) {
  auto it = FindPage(contents);
  CHECK(it != pages_.end());

  for (Observer& observer : observers_) {
    observer.OnHeadlessContentsWillBeDestroyed(contents);
  }

  // Unlist before destroying, so the dying WebContents is unreachable.
  HeadlessPage page = std::move(*it);
  pages_.erase(it);
}

std::unique_ptr<content::WebContents> HeadlessWebContentsManager::TakeContents(
    content::WebContents* contents) {
  auto it = FindPage(contents);
  CHECK(it != pages_.end());
  it->host.reset();
  std::unique_ptr<content::WebContents> owned = std::move(it->contents);
  pages_.erase(it);
  owned->SetDelegate(nullptr);
  return owned;
}

void HeadlessWebContentsManager::AdoptContents(
    std::unique_ptr<content::WebContents> contents) {
  CHECK(contents);
  CHECK_EQ(contents->GetBrowserContext(), browser_context_.get());
  contents->SetDelegate(this);
  pages_.emplace_back(std::move(contents));
}

bool HeadlessWebContentsManager::Contains(
    const content::WebContents* contents) const {
  return std::ranges::contains(pages_, contents, [](const HeadlessPage& page) {
    return page.contents.get();
  });
}

void HeadlessWebContentsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HeadlessWebContentsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HeadlessWebContentsManager::CloseContents(content::WebContents* source) {}

content::JavaScriptDialogManager*
HeadlessWebContentsManager::GetJavaScriptDialogManager(
    content::WebContents* source) {
  return JavaScriptDialogInterceptor::MaybeGetForWebContents(source);
}

std::vector<HeadlessWebContentsManager::HeadlessPage>::iterator
HeadlessWebContentsManager::FindPage(const content::WebContents* contents) {
  return std::ranges::find(pages_, contents, [](const HeadlessPage& page) {
    return page.contents.get();
  });
}

}  // namespace actor
