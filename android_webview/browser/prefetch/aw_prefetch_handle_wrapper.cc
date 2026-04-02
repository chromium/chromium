// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_handle_wrapper.h"

#include "android_webview/common/aw_features.h"
#include "content/public/browser/browser_thread.h"

namespace android_webview {

AwPrefetchHandleWrapper::AwPrefetchHandleWrapper(
    const GURL& url,
    std::optional<net::HttpNoVarySearchData> expected_no_vary_search,
    std::unique_ptr<content::PrefetchHandle> prefetch_handle)
    : url_(url),
      expected_no_vary_search_(std::move(expected_no_vary_search)),
      prefetch_handle_(std::move(prefetch_handle)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

AwPrefetchHandleWrapper::AwPrefetchHandleWrapper(
    const GURL& url,
    std::optional<net::HttpNoVarySearchData> expected_no_vary_search,
    std::unique_ptr<content::PrePrefetchHandle> pre_prefetch_handle)
    : url_(url),
      expected_no_vary_search_(std::move(expected_no_vary_search)),
      pre_prefetch_handle_(std::move(pre_prefetch_handle)) {}

AwPrefetchHandleWrapper::~AwPrefetchHandleWrapper() {
  if (prefetch_handle_) {
    // Delete the handle on the UI thread since it may touch
    // `PrefetchContainer` if it is `PrefetchHandle`.
    if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      CHECK(base::FeatureList::IsEnabled(
          features::kWebViewPrefetchOffTheMainThread));
      content::GetUIThreadTaskRunner({})->DeleteSoon(
          FROM_HERE, std::move(prefetch_handle_));
    }
  }
}

const GURL& AwPrefetchHandleWrapper::GetURL() const {
  return url_;
}

const std::optional<net::HttpNoVarySearchData>&
AwPrefetchHandleWrapper::GetNoVarySearchHint() const {
  return expected_no_vary_search_;
}

bool AwPrefetchHandleWrapper::IsPrefetchStale() const {
  // We can't touch the inner handle during deduplication, which can happen on
  // any thread. Thus, we always return false.
  return false;
}

}  // namespace android_webview
