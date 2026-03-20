// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_handle_wrapper.h"

namespace android_webview {

AwPrefetchHandleWrapper::AwPrefetchHandleWrapper(
    const GURL& url,
    std::optional<net::HttpNoVarySearchData> expected_no_vary_search,
    std::unique_ptr<content::PrefetchHandle> handle)
    : url_(url),
      expected_no_vary_search_(std::move(expected_no_vary_search)),
      handle_(std::move(handle)) {}

AwPrefetchHandleWrapper::~AwPrefetchHandleWrapper() = default;

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
