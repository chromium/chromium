// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/hotl_validator.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ttc/core/features.h"
#include "components/actor/core/journal_details_builder.h"

namespace ttc {

// static
std::string_view HotlValidator::ToString(Result result) {
  switch (result) {
    case Result::kApproved:
      return "approved";
    case Result::kDenied:
      return "denied";
    case Result::kTimedOut:
      return "timeout";
  }
}

// static
std::string_view HotlValidator::ToString(Source source) {
  switch (source) {
    case Source::kButton:
      return "button";
    case Source::kTimeout:
      return "timeout";
  }
}

HotlValidator::HotlValidator(SessionJournal& journal, ResultCallback callback)
    : journal_(journal), callback_(std::move(callback)) {
  CHECK(callback_);
  const base::TimeDelta timeout = kHotlConfirmationTimeout.Get();
  journal_event_ = journal_->BeginAsyncEvent(
      "TtcHotlConfirmation", actor::JournalDetailsBuilder()
                                 .Add("timeout_ms", timeout.InMilliseconds())
                                 .Build());
  CHECK(journal_event_);
  timeout_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&HotlValidator::OnTimeout, base::Unretained(this)));
}

HotlValidator::~HotlValidator() {
  if (journal_event_) {
    journal_event_->EndEntry(
        actor::JournalDetailsBuilder().Add("outcome", "cancelled").Build());
  }
}

void HotlValidator::Answer(bool approved) {
  CHECK(callback_);
  Resolve(approved ? Result::kApproved : Result::kDenied, Source::kButton);
}

void HotlValidator::OnTimeout() {
  Resolve(Result::kTimedOut, Source::kTimeout);
}

void HotlValidator::Resolve(Result result, Source source) {
  CHECK(callback_);
  timeout_.Stop();
  journal_event_->EndEntry(actor::JournalDetailsBuilder()
                               .Add("outcome", ToString(result))
                               .Add("source", ToString(source))
                               .Build());
  journal_event_.reset();
  ResultCallback callback = std::move(callback_);
  // Run the callback last because it may destroy `this`.
  std::move(callback).Run(result);
}

}  // namespace ttc
