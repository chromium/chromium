// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_HANDLE_WRAPPER_H_
#define ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_HANDLE_WRAPPER_H_

#include <memory>
#include <optional>

#include "content/public/browser/pre_prefetch_handle.h"
#include "content/public/browser/prefetch_deduplication_utils.h"
#include "content/public/browser/prefetch_handle.h"
#include "net/http/http_no_vary_search_data.h"
#include "url/gurl.h"

namespace android_webview {

// A wrapper class for `content::PrefetchHandle` or `content::PrePrefetchHandle`
// owned by `AwPrefetchManager`.
//
// Thread model:
//
// - Can be created on any thread, and then owned by `AwPrefetchManager`.
// - All non-special member functions (currently getter only) can be called from
//   any thread. Note that `prefetch_handle_` is UI thread bound, not thread
//   safe, and won't be accessed after construction.
// - Can be destroyed on any thread, but `prefetch_handle_` should be properly
//   destroyed on the UI thread.
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
  // Creates an empty reserved wrapper with no handles, which sets the state to
  // `kReserved`.
  AwPrefetchHandleWrapper(
      const GURL& url,
      std::optional<net::HttpNoVarySearchData> expected_no_vary_search);

  // Commits a `PrefetchHandle` or `PrePrefetchHandle`, which transitions the
  // state from `kReserved` to `kPrefetchHandleCommitted` or
  // `kPrePrefetchHandleCommitted` respectively.
  void CommitInitialPrePrefetchHandle(
      std::unique_ptr<content::PrePrefetchHandle> pre_prefetch_handle);
  void CommitInitialPrefetchHandle(
      std::unique_ptr<content::PrefetchHandle> prefetch_handle);

  // A legacy ctor directly committing a PrefetchHandle. Only used when
  // `kWebViewPrefetchOffTheMainThread` is disabled.
  AwPrefetchHandleWrapper(
      const GURL& url,
      std::optional<net::HttpNoVarySearchData> expected_no_vary_search,
      std::unique_ptr<content::PrefetchHandle> prefetch_handle);

  ~AwPrefetchHandleWrapper() override;

  AwPrefetchHandleWrapper(const AwPrefetchHandleWrapper&) = delete;
  AwPrefetchHandleWrapper& operator=(const AwPrefetchHandleWrapper&) = delete;
  AwPrefetchHandleWrapper(AwPrefetchHandleWrapper&&) = delete;
  AwPrefetchHandleWrapper& operator=(AwPrefetchHandleWrapper&&) = delete;

  // Represents a valid state of `this`.
  // Please see `SetState()` and `CheckState()` for the actual transitions and
  // `prefetch_handle_` / `pre_prefetch_handle_` states.
  enum class State {
    // Initial state for an empty wrapper with no handles.
    // The wrapper should hold:
    // - `prefetch_handle_` is null.
    // - `pre_prefetch_handle_` is null.
    kReserved,

    // State after PrePrefetchHandle is committed.
    // The wrapper should hold:
    // - `prefetch_handle_` is null.
    // - `pre_prefetch_handle_` is non-null.
    kPrePrefetchHandleCommitted,

    // The wrapper's `pre_prefetch_handle_` is currently being consumed to start
    // a regular Prefetch.
    // The wrapper should hold:
    // - `prefetch_handle_` is null.
    // - `pre_prefetch_handle_` is null.
    kPrePrefetchConsumeStarted,

    // Final state for PrePrefetch, and initial state and final state for a
    // Prefetch. No further transitions are allowed.
    // The wrapper should hold:
    // - `prefetch_handle_` is non-null.
    // - `pre_prefetch_handle_` is null.
    kPrefetchHandleCommitted,
  };

  // Returns true if `pre_prefetch_handle_` can be taken for consume.
  bool CanTakePrePrefetchHandleForConsume() const;

  // Takes `pre_prefetch_handle_` for consume, which transitions the state from
  // `kPrePrefetchHandleCommitted` to `kPrePrefetchConsumeStarted`.
  std::unique_ptr<content::PrePrefetchHandle> TakePrePrefetchHandleForConsume();

  // Commits `prefetch_handle_` after `pre_prefetch_handle_` was consumed by
  // `TakePrePrefetchHandleForConsume()`, which transitions the state from
  // `kPrePrefetchConsumeStarted` to `kPrefetchHandleCommitted`.
  void CommitPrefetchHandleAfterConsume(
      std::unique_ptr<content::PrefetchHandle> prefetch_handle);

  // content::PrefetchDeduplicationEntry:
  const GURL& GetURL() const override;
  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint()
      const override;
  bool IsPrefetchStale() const override;

 private:
  void CheckState() const;
  void SetState(State new_state);

  const GURL url_;
  const std::optional<net::HttpNoVarySearchData> expected_no_vary_search_;

  // Must be destructed and dereferenced only on the UI thread.
  std::unique_ptr<content::PrefetchHandle> prefetch_handle_;

  // Can be destructed on any thread.
  std::unique_ptr<content::PrePrefetchHandle> pre_prefetch_handle_;

  State state_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PREFETCH_AW_PREFETCH_HANDLE_WRAPPER_H_
