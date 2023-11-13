// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_manager.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/preloading/preview/preview_tab.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

PreviewManager::PreviewManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PreviewManager>(*web_contents) {}

PreviewManager::~PreviewManager() = default;

void PreviewManager::PrimaryPageChanged(content::Page& page) {
  // When initiator page has gone, cancel preview.
  tab_.reset();
}

void PreviewManager::InitiatePreview(const GURL& url) {
  // TODO(b:292184832): Pass more load params.
  tab_ = std::make_unique<PreviewTab>(this, GetWebContents(), url);
}

void PreviewManager::Cancel() {
  if (!tab_) {
    return;
  }

  // Delete `tab_` asynchronously so that we can call this inside PreviewTab.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(tab_)));
}

void PreviewManager::PromoteToNewTab() {
  if (!tab_) {
    return;
  }

  tab_->PromoteToNewTab(GetWebContents());
  // Delete `tab_` asynchronously so that we can call this inside PreviewTab.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(tab_)));
}

base::WeakPtr<content::WebContents>
PreviewManager::GetWebContentsForPreviewTab() {
  CHECK(tab_);
  return tab_->GetWebContents();
}

void PreviewManager::CloseForTesting() {
  CHECK(tab_);
  tab_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreviewManager);
