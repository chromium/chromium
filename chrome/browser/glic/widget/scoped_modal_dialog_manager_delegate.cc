// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/scoped_modal_dialog_manager_delegate.h"

#include <algorithm>

#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"

namespace glic {

ScopedModalDialogManagerDelegate::WebContentsWatcher::WebContentsWatcher(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

ScopedModalDialogManagerDelegate::WebContentsWatcher::~WebContentsWatcher() =
    default;

ScopedModalDialogManagerDelegate::ScopedModalDialogManagerDelegate(
    web_modal::WebContentsModalDialogManagerDelegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);
}

ScopedModalDialogManagerDelegate::~ScopedModalDialogManagerDelegate() {
  Reset();
}

void ScopedModalDialogManagerDelegate::AddWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  if (std::ranges::any_of(watchers_, [web_contents](const auto& watcher) {
        return watcher->web_contents() == web_contents;
      })) {
    return;
  }

  watchers_.push_back(std::make_unique<WebContentsWatcher>(web_contents));

  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
  if (auto* dialog_manager =
          web_modal::WebContentsModalDialogManager::FromWebContents(
              web_contents)) {
    dialog_manager->SetDelegate(delegate_);
  }
}

void ScopedModalDialogManagerDelegate::RemoveWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  auto it =
      std::ranges::find_if(watchers_, [web_contents](const auto& watcher) {
        return watcher->web_contents() == web_contents;
      });
  if (it == watchers_.end()) {
    return;
  }
  watchers_.erase(it);

  if (auto* dialog_manager =
          web_modal::WebContentsModalDialogManager::FromWebContents(
              web_contents)) {
    if (dialog_manager->delegate() == delegate_) {
      dialog_manager->CloseAllDialogs();
      dialog_manager->SetDelegate(nullptr);
      web_contents->RemoveUserData(
          web_modal::WebContentsModalDialogManager::UserDataKey());
    }
  }
}

void ScopedModalDialogManagerDelegate::Reset() {
  while (!watchers_.empty()) {
    auto watcher = std::move(watchers_.back());
    watchers_.pop_back();
    if (content::WebContents* wc = watcher->web_contents()) {
      if (auto* dialog_manager =
              web_modal::WebContentsModalDialogManager::FromWebContents(wc)) {
        if (dialog_manager->delegate() == delegate_) {
          dialog_manager->CloseAllDialogs();
          dialog_manager->SetDelegate(nullptr);
          wc->RemoveUserData(
              web_modal::WebContentsModalDialogManager::UserDataKey());
        }
      }
    }
  }
}

}  // namespace glic
