// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_outcome_tracker.h"

#include <tuple>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"

namespace infobars {

InfoBarOutcomeTracker::InfoBarOutcomeTracker(
    InfoBarDelegate::InfoBarIdentifier identifier,
    InfoBarSpec::ResultCallback callback)
    : identifier_(identifier), callback_(std::move(callback)) {}

InfoBarOutcomeTracker::~InfoBarOutcomeTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ReportPending(/*web_contents=*/nullptr);
}

void InfoBarOutcomeTracker::Arm() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kUnarmed) {
    state_ = State::kPendingIgnored;
  }
}

void InfoBarOutcomeTracker::MarkLinkClicked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kPendingIgnored) {
    state_ = State::kPendingLinkClicked;
  }
}

void InfoBarOutcomeTracker::Suppress() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kResolved;
  // Move `callback_` out before destroying its bound state in case it holds the
  // last reference to `this`.
  std::ignore = std::exchange(callback_, {});
}

InfoBarOutcomeTracker::ClaimedCallback InfoBarOutcomeTracker::Claim(
    InfoBarResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(result, InfoBarResult::kIgnored);

  if (state_ != State::kPendingIgnored &&
      state_ != State::kPendingLinkClicked) {
    return base::NullCallback();
  }

  state_ = State::kResolved;
  InfoBarSpec::ResultCallback callback = std::exchange(callback_, {});
  if (!callback) {
    return base::NullCallback();
  }

  return base::BindOnce(
      [](InfoBarSpec::ResultCallback cb, InfoBarResult claimed_result,
         content::WebContents* web_contents) {
        std::move(cb).Run(web_contents, claimed_result);
      },
      std::move(callback), result);
}

void InfoBarOutcomeTracker::ReportPending(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kPendingIgnored &&
      state_ != State::kPendingLinkClicked) {
    return;
  }

  const InfoBarResult result = state_ == State::kPendingIgnored
                                   ? InfoBarResult::kIgnored
                                   : InfoBarResult::kLinkClicked;
  state_ = State::kResolved;
  InfoBarSpec::ResultCallback callback = std::exchange(callback_, {});

  if (result == InfoBarResult::kIgnored) {
    base::UmaHistogramSparse("InfoBar.Centralized.Ignored", identifier_);
  }

  if (callback) {
    std::move(callback).Run(web_contents, result);
  }
}

}  // namespace infobars
