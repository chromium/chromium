// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_manager.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/preloading/preview/preview_tab.h"
#include "content/public/browser/preview_cancel_reason.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

PreviewManager::PreviewManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PreviewManager>(*web_contents) {}

PreviewManager::~PreviewManager() = default;

void PreviewManager::PrimaryPageChanged(content::Page& page) {
  // When initiator page has gone, cancel preview.
  tab_.reset();

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<PreviewManager> self) {
                       // Reset `usage_` asynchronously as other observers will
                       // still refer the usage on the page change.
                       if (self) {
                         self->usage_ = Usage::kNotUsed;
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

void PreviewManager::InitiatePreview(const GURL& url) {
  if (usage_ == Usage::kNotUsed) {
    // Upgrade to kUsedButNotPromoted iff this is the first time to initiate
    // the feature.
    usage_ = Usage::kUsedButNotPromoted;
  }
  // TODO(b:292184832): Pass more load params.
  tab_ = std::make_unique<PreviewTab>(this, GetWebContents(), url);
}

void PreviewManager::Cancel(content::PreviewCancelReason reason) {
  if (!tab_) {
    return;
  }

  tab_->CancelPreview(std::move(reason));
  // Delete `tab_` asynchronously so that we can call this inside PreviewTab.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(tab_)));
}

void PreviewManager::PromoteToNewTab() {
  if (!tab_) {
    return;
  }

  // Upgrade to kUsedAndPromoted. This will be asynchronously reset to kNotUsed
  // when the primary page is changed to the next page.
  usage_ = Usage::kUsedAndPromoted;
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

PreviewZoomController* PreviewManager::PreviewZoomControllerForTesting() const {
  return tab_->preview_zoom_controller();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreviewManager);
