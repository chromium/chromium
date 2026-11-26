// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_HOTL_HOTL_VALIDATOR_H_
#define CHROME_BROWSER_TTC_CORE_HOTL_HOTL_VALIDATOR_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/timer/timer.h"
#include "components/ttc/app/public/session_journal.h"

namespace ttc {

// Resolves one Human-on-the-Loop (HOTL) confirmation. The confirmation starts
// on construction and ends when the user answers or when the timeout expires.
// Destroying the validator before it resolves cancels the timer and drops the
// pending callback without running it.
class HotlValidator {
 public:
  enum class Result {
    kApproved,
    kDenied,
    kTimedOut,
  };
  using ResultCallback = base::OnceCallback<void(Result)>;

  // Starts a confirmation and runs `callback` once with its result. `journal`
  // must outlive `this`.
  HotlValidator(SessionJournal& journal, ResultCallback callback);
  HotlValidator(const HotlValidator&) = delete;
  HotlValidator& operator=(const HotlValidator&) = delete;
  ~HotlValidator();

  // Resolves the confirmation with the user's answer. Must not be called after
  // the confirmation has already resolved.
  void Answer(bool approved);

 private:
  enum class Source {
    kButton,
    kTimeout,
  };

  static std::string_view ToString(Result result);
  static std::string_view ToString(Source source);

  void OnTimeout();

  // Ends the async journal slice, stops the timer, and runs `callback_` with
  // `result`. `callback_` may destroy `this`.
  void Resolve(Result result, Source source);

  const raw_ref<SessionJournal> journal_;
  std::unique_ptr<SessionJournal::PendingAsyncEvent> journal_event_;
  ResultCallback callback_;
  base::OneShotTimer timeout_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_HOTL_HOTL_VALIDATOR_H_
