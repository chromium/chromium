// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_HANDLE_WRAPPER_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_HANDLE_WRAPPER_H_

#include <memory>
#include <optional>

#include "content/public/browser/prefetch_deduplication_utils.h"
#include "content/public/browser/prefetch_handle.h"
#include "net/http/http_no_vary_search_data.h"
#include "url/gurl.h"

namespace android_webview {

// A wrapper class for `content::PrefetchHandle` owned by `AwPrefetchManager`.
//
// Thread model:
//
// - Can be created on any thread, and then owned by `AwPrefetchManager`.
// - All non-special member functions (currently getter only) can be called from
//   any thread. Note that `handle_` is UI thread bound, not thread safe, and
//   won't be accessed after construction.
// - Can be destroyed on any thread, but `handle_` should be properly destroyed
//   on the UI thread.
//
// Under `kWebViewPrefetchOffTheMainThread` being enabled, the deduplication is
// performed solely by `AwPrefetchManager`'s `url_` and
// `expected_no_vary_search_`. Thus, we use less information compared to
// `BrowserContext::IsPrefetchDuplicate()`, which uses
// `PrefetchService`/`PrefetchContainer`'s state. i.e. `IsPrefetchStale()` is
// always false here.
class AwPrefetchHandleWrapper final
    : public content::PrefetchDeduplicationEntry {
 public:
  AwPrefetchHandleWrapper(
      const GURL& url,
      std::optional<net::HttpNoVarySearchData> expected_no_vary_search,
      std::unique_ptr<content::PrefetchHandle> handle);
  ~AwPrefetchHandleWrapper() override;

  AwPrefetchHandleWrapper(const AwPrefetchHandleWrapper&) = delete;
  AwPrefetchHandleWrapper& operator=(const AwPrefetchHandleWrapper&) = delete;
  AwPrefetchHandleWrapper(AwPrefetchHandleWrapper&&) = delete;
  AwPrefetchHandleWrapper& operator=(AwPrefetchHandleWrapper&&) = delete;

  // content::PrefetchDeduplicationEntry:
  const GURL& GetURL() const override;
  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint()
      const override;
  bool IsPrefetchStale() const override;

 private:
  const GURL url_;
  const std::optional<net::HttpNoVarySearchData> expected_no_vary_search_;
  const std::unique_ptr<content::PrefetchHandle> handle_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_HANDLE_WRAPPER_H_
