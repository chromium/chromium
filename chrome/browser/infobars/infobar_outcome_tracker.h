// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_OUTCOME_TRACKER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_OUTCOME_TRACKER_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "components/infobars/core/infobar_delegate.h"

namespace content {
class WebContents;
}

namespace infobars {

// Tracks the at-most-once terminal `InfoBarResult` for one logical show,
// whether tab-scoped or globally mirrored. Not thread-safe; must be used and
// have all references released on the UI thread sequence.
//
// Starts unarmed so rejected `AddInfoBar()` calls report nothing. Once
// `Arm()`ed, transitions monotonically to resolved via `Claim()` for explicit
// user interactions, `Suppress()` for programmatic dismissal, `ReportPending()`
// when the logical infobar ends without an explicit interaction, or
// destruction, which falls back to `ReportPending(nullptr)`.
class InfoBarOutcomeTracker : public base::RefCounted<InfoBarOutcomeTracker> {
 public:
  using ClaimedCallback = base::OnceCallback<void(content::WebContents*)>;

  InfoBarOutcomeTracker(InfoBarDelegate::InfoBarIdentifier identifier,
                        InfoBarSpec::ResultCallback callback);

  InfoBarOutcomeTracker(const InfoBarOutcomeTracker&) = delete;
  InfoBarOutcomeTracker& operator=(const InfoBarOutcomeTracker&) = delete;

  // Arms the tracker with a pending `kIgnored` fallback outcome. Idempotent:
  // no-op once already armed or resolved.
  void Arm();

  // Upgrades a pending `kIgnored` fallback outcome to `kLinkClicked`. No-op if
  // unarmed, already `kLinkClicked`, or resolved.
  void MarkLinkClicked();

  // Resolves the tracker without invoking the callback or recording any
  // metrics. Also prevents a subsequent `Arm()` if called while unarmed. No-op
  // if already resolved.
  void Suppress();

  // Claims the pending outcome for an explicit user interaction and transitions
  // to resolved so no fallback outcome is reported later. `result` must not be
  // `kIgnored`, which is only emitted via `ReportPending()` or destruction.
  // Returns a one-shot callback that delivers `result`, or a null callback if
  // the tracker was not pending or had no result callback.
  ClaimedCallback Claim(InfoBarResult result);

  // Reports and clears any still-pending `kIgnored` or `kLinkClicked` fallback
  // outcome, recording `InfoBar.Centralized.Ignored` for `kIgnored`. No-op if
  // unarmed or already resolved.
  void ReportPending(content::WebContents* web_contents);

 private:
  friend class base::RefCounted<InfoBarOutcomeTracker>;

  enum class State {
    kUnarmed,
    kPendingIgnored,
    kPendingLinkClicked,
    kResolved,
  };

  ~InfoBarOutcomeTracker();

  const InfoBarDelegate::InfoBarIdentifier identifier_;
  InfoBarSpec::ResultCallback callback_;
  State state_ = State::kUnarmed;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace infobars

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_OUTCOME_TRACKER_H_
