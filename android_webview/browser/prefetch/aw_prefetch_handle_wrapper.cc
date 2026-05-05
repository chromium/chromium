// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/prefetch/aw_prefetch_handle_wrapper.h"

#include <ostream>

#include "android_webview/browser/prefetch/aw_preloading_utils.h"
#include "android_webview/common/aw_features.h"
#include "base/state_transitions.h"
#include "content/public/browser/browser_thread.h"

namespace android_webview {

AwPrefetchHandleWrapper::AwPrefetchHandleWrapper(
    const GURL& url,
    std::optional<net::HttpNoVarySearchData> expected_no_vary_search)
    : url_(url),
      expected_no_vary_search_(std::move(expected_no_vary_search)),
      state_(State::kReserved) {
  // TODO(crbug.com/452406598): The transition to
  // either `kPrePrefetchHandleCommitted` or `kPrefetchHandleCommitted` will be
  // strictly bound to the writer interface granting write permission to
  // `this`, which ensures the handle is eventually committed.
  CheckState();
}

void AwPrefetchHandleWrapper::CommitInitialPrePrefetchHandle(
    std::unique_ptr<content::PrePrefetchHandle> pre_prefetch_handle) {
  CHECK_EQ(state_, State::kReserved);
  // A valid handle should be provided to commit.
  CHECK(pre_prefetch_handle);

  pre_prefetch_handle_ = std::move(pre_prefetch_handle);
  SetState(State::kPrePrefetchHandleCommitted);
}

void AwPrefetchHandleWrapper::CommitInitialPrefetchHandle(
    std::unique_ptr<content::PrefetchHandle> prefetch_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_EQ(state_, State::kReserved);
  // A valid handle should be provided to commit.
  CHECK(prefetch_handle);

  prefetch_handle_ =
      content::CrossThreadPrefetchHandle::Create(std::move(prefetch_handle));
  SetState(State::kPrefetchHandleCommitted);
}

AwPrefetchHandleWrapper::AwPrefetchHandleWrapper(
    const GURL& url,
    std::optional<net::HttpNoVarySearchData> expected_no_vary_search,
    std::unique_ptr<content::PrefetchHandle> prefetch_handle)
    : url_(url),
      expected_no_vary_search_(std::move(expected_no_vary_search)),
      prefetch_handle_(content::CrossThreadPrefetchHandle::Create(
          std::move(prefetch_handle))),
      state_(State::kPrefetchHandleCommitted) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!IsWebViewPrefetchOffTheMainThreadEnabled());
  CheckState();
}

AwPrefetchHandleWrapper::~AwPrefetchHandleWrapper() = default;

void AwPrefetchHandleWrapper::CheckState() const {
  switch (state_) {
    case State::kReserved:
      CHECK(!pre_prefetch_handle_);
      CHECK(!prefetch_handle_);
      break;
    case State::kPrePrefetchHandleCommitted:
      CHECK(pre_prefetch_handle_);
      CHECK(!prefetch_handle_);
      break;
    case State::kPrePrefetchConsumeStarted:
      CHECK(!pre_prefetch_handle_);
      CHECK(!prefetch_handle_);
      break;
    case State::kPrefetchHandleCommitted:
      CHECK(!pre_prefetch_handle_);
      CHECK(prefetch_handle_);
      break;
  }
}

std::ostream& operator<<(std::ostream& os,
                         AwPrefetchHandleWrapper::State state) {
  switch (state) {
    case AwPrefetchHandleWrapper::State::kReserved:
      return os << "kReserved";
    case AwPrefetchHandleWrapper::State::kPrePrefetchHandleCommitted:
      return os << "kPrePrefetchHandleCommitted";
    case AwPrefetchHandleWrapper::State::kPrePrefetchConsumeStarted:
      return os << "kPrePrefetchConsumeStarted";
    case AwPrefetchHandleWrapper::State::kPrefetchHandleCommitted:
      return os << "kPrefetchHandleCommitted";
  }
}

void AwPrefetchHandleWrapper::SetState(State new_state) {
  {
    using T = State;
    static const base::NoDestructor<base::StateTransitions<T>> transitions(
        base::StateTransitions<T>({
            {T::kReserved,
             {T::kPrePrefetchHandleCommitted, T::kPrefetchHandleCommitted}},
            {T::kPrePrefetchHandleCommitted, {T::kPrePrefetchConsumeStarted}},
            {T::kPrePrefetchConsumeStarted, {T::kPrefetchHandleCommitted}},
            {T::kPrefetchHandleCommitted, {}},
        }));
    CHECK_STATE_TRANSITION(transitions, state_, new_state);
  }
  state_ = new_state;
  CheckState();
}

bool AwPrefetchHandleWrapper::CanTakePrePrefetchHandleForConsume() const {
  return state_ == State::kPrePrefetchHandleCommitted;
}

std::unique_ptr<content::PrePrefetchHandle>
AwPrefetchHandleWrapper::TakePrePrefetchHandleForConsume() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_EQ(state_, State::kPrePrefetchHandleCommitted);
  // TODO(crbug.com/452406598): The transition to
  // `kPrePrefetchConsumeStarted` will be strictly bound to the writer
  // interface granting write permission to `this`, which ensures the
  // handle is eventually committed.
  auto pre_prefetch_handle = std::move(pre_prefetch_handle_);
  SetState(State::kPrePrefetchConsumeStarted);
  return pre_prefetch_handle;
}

void AwPrefetchHandleWrapper::CommitPrefetchHandleAfterConsume(
    std::unique_ptr<content::PrefetchHandle> prefetch_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_EQ(state_, State::kPrePrefetchConsumeStarted);

  // A valid handle should be provided to commit.
  CHECK(prefetch_handle);
  prefetch_handle_ =
      content::CrossThreadPrefetchHandle::Create(std::move(prefetch_handle));
  SetState(State::kPrefetchHandleCommitted);
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
