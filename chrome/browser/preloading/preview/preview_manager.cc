// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_manager.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/preloading/preview/preview_tab.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// A duration to wait a prerendering after renderer deciced to preview.
// TODO(b:296992745): Consider to let renderer handle the initiations of both
// prerendering and preview.
const base::TimeDelta kPreviewWarmupDuration = base::Milliseconds(300);

PreviewManager::PreviewManager(content::WebContents* web_contents)
    : content::WebContentsUserData<PreviewManager>(*web_contents) {}

PreviewManager::~PreviewManager() = default;

void PreviewManager::InitiatePreview(const GURL& url) {
  // Other preloadings are features to speed up navigations, which user agents
  // may do. On the other hand, preview is a feature that is UI-triggered and
  // gives UI feedback to users. So, we don't check eligibiilty with
  // prefetch::IsSomePreloadingEnabled.

  // TODO(b:292184832): Pass more load params.
  tab_ = std::make_unique<PreviewTab>(GetWebContents(), url);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PreviewManager::Show, weak_factory_.GetWeakPtr(),
                     tab_.get()),
      kPreviewWarmupDuration);
}

void PreviewManager::Show(PreviewTab* tab) {
  // Show preview if PreviewManager didn't receive other new requests.
  if (tab_.get() == tab) {
    tab_->Show();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PreviewManager);
