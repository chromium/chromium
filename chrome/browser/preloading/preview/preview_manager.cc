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
    : content::WebContentsUserData<PreviewManager>(*web_contents) {}

PreviewManager::~PreviewManager() = default;

void PreviewManager::InitiatePreview(const GURL& url) {
  // TODO(b:292184832): Pass more load params.
  tab_ = std::make_unique<PreviewTab>(GetWebContents(), url);
}

base::WeakPtr<content::WebContents>
PreviewManager::GetWebContentsForPreviewTab() {
  CHECK(tab_);
  return tab_->GetWebContents();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreviewManager);
