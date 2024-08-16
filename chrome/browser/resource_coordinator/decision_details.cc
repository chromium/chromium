// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/resource_coordinator/decision_details.h"

#include "services/metrics/public/cpp/ukm_builders.h"

namespace resource_coordinator {

namespace {

// These are intended to be human readable descriptions of the various failure
// reasons. They don't need to be localized as they are for a developer-only
// WebUI.
const char* kDecisionFailureReasonStrings[] = {
    "Browser opted out via enterprise policy",
    "Tab opted out via origin trial",
    "Origin is in global disallowlist",
    "Origin has been observed playing audio while backgrounded",
    "Origin has been observed updating favicon while backgrounded",
    "Origin is temporarily protected while under observation",
    "Origin has been observed updating title while backgrounded",
    "Tab is currently capturing the camera and/or microphone",
    "Tab has been protected by an extension",
    "Tab contains unsubmitted text form entry",
    "Tab contains a PDF",
    "Tab content is being mirrored/cast",
    "Tab is currently emitting audio",
    "Tab is currently using WebSockets",
    "Tab is currently using WebUSB",
    "Tab is currently visible",
    "Tab is currently using DevTools",
    "Tab is currently capturing a window or screen",
    "Tab is sharing its BrowsingInstance with another tab",
    "Tab is currently connected to a bluetooth device",
    "Tab is currently holding a WebLock",
    "Tab is currently holding an IndexedDB lock",
    "Tab has notification permission ",
    "Tab is a web application window",
    "Tab is displaying content in picture-in-picture",
};
static_assert(std::size(kDecisionFailureReasonStrings) ==
                  static_cast<size_t>(DecisionFailureReason::MAX),
              "kDecisionFailureReasonStrings not up to date with enum");

const char* kDecisionSuccessReasonStrings[] = {
    "Tab opted in via origin trial",
    "Origin is in global allowlist",
    "Origin has locally been observed to be safe via heuristic logic",
};
static_assert(std::size(kDecisionSuccessReasonStrings) ==
                  static_cast<size_t>(DecisionSuccessReason::MAX),
              "kDecisionSuccessReasonStrings not up to date with enum");

void PopulateSuccessReason(
    DecisionSuccessReason success_reason,
    ukm::builders::TabManager_LifecycleStateChange* ukm) {
  switch (success_reason) {
    case DecisionSuccessReason::INVALID:
      break;
    case DecisionSuccessReason::ORIGIN_TRIAL_OPT_IN:
      ukm->SetSuccessOriginTrialOptIn(1);
      break;
    case DecisionSuccessReason::GLOBAL_ALLOWLIST:
      ukm->SetSuccessGlobalAllowlist(1);
      break;
    case DecisionSuccessReason::HEURISTIC_OBSERVED_TO_BE_SAFE:
      ukm->SetSuccessHeuristic(1);
      break;
    case DecisionSuccessReason::MAX:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void PopulateFailureReason(
    DecisionFailureReason failure_reason,
    ukm::builders::TabManager_LifecycleStateChange* ukm) {
  switch (failure_reason) {
    case DecisionFailureReason::INVALID:
      break;
    case DecisionFailureReason::LIFECYCLES_ENTERPRISE_POLICY_OPT_OUT:
      ukm->SetFailureLifecyclesEnterprisePolicyOptOut(1);
      break;
    case DecisionFailureReason::ORIGIN_TRIAL_OPT_OUT:
      ukm->SetFailureOriginTrialOptOut(1);
      break;
    case DecisionFailureReason::GLOBAL_DISALLOWLIST:
      ukm->SetFailureGlobalDisallowlist(1);
      break;
    case DecisionFailureReason::HEURISTIC_AUDIO:
      ukm->SetFailureHeuristicAudio(1);
      break;
    case DecisionFailureReason::HEURISTIC_FAVICON:
      ukm->SetFailureHeuristicFavicon(1);
      break;
    case DecisionFailureReason::HEURISTIC_INSUFFICIENT_OBSERVATION:
      ukm->SetFailureHeuristicInsufficientObservation(1);
      break;
    case DecisionFailureReason::HEURISTIC_TITLE:
      ukm->SetFailureHeuristicTitle(1);
      break;
    case DecisionFailureReason::LIVE_STATE_CAPTURING:
      ukm->SetFailureLiveStateCapturing(1);
      break;
    case DecisionFailureReason::LIVE_STATE_EXTENSION_DISALLOWED:
      ukm->SetFailureLiveStateExtensionDisallowed(1);
      break;
    case DecisionFailureReason::LIVE_STATE_FORM_ENTRY:
      ukm->SetFailureLiveStateFormEntry(1);
      break;
    case DecisionFailureReason::LIVE_STATE_IS_PDF:
      ukm->SetFailureLiveStateIsPDF(1);
      break;
    case DecisionFailureReason::LIVE_STATE_MIRRORING:
      ukm->SetFailureLiveStateMirroring(1);
      break;
    case DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO:
      ukm->SetFailureLiveStatePlayingAudio(1);
      break;
    case DecisionFailureReason::LIVE_STATE_USING_WEB_SOCKETS:
      ukm->SetFailureLiveStateUsingWebSockets(1);
      break;
    case DecisionFailureReason::LIVE_STATE_USING_WEB_USB:
      ukm->SetFailureLiveStateUsingWebUSB(1);
      break;
    case DecisionFailureReason::LIVE_STATE_VISIBLE:
      ukm->SetFailureLiveStateVisible(1);
      break;
    case DecisionFailureReason::LIVE_STATE_DEVTOOLS_OPEN:
      ukm->SetFailureLiveStateDevToolsOpen(1);
      break;
    case DecisionFailureReason::LIVE_STATE_DESKTOP_CAPTURE:
      ukm->SetFailureLiveStateDesktopCapture(1);
      break;
    case DecisionFailureReason::LIVE_STATE_SHARING_BROWSING_INSTANCE:
      ukm->SetFailureLiveStateSharingBrowsingInstance(1);
      break;
    case DecisionFailureReason::LIVE_STATE_USING_BLUETOOTH:
      ukm->SetFailureLiveStateUsingBluetooth(1);
      break;
    case DecisionFailureReason::LIVE_STATE_USING_WEBLOCK:
      ukm->SetFailureLiveStateUsingWebLock(1);
      break;
    case DecisionFailureReason::LIVE_STATE_USING_INDEXEDDB_LOCK:
      ukm->SetFailureLiveStateUsingIndexedDBLock(1);
      break;
    case DecisionFailureReason::LIVE_STATE_HAS_NOTIFICATIONS_PERMISSION:
      ukm->SetFailureLiveStateHasNotificationsPermission(1);
      break;
    case DecisionFailureReason::LIVE_WEB_APP:
      ukm->SetFailureLiveWebApp(1);
      break;
    case DecisionFailureReason::LIVE_PICTURE_IN_PICTURE:
      ukm->SetFailureLivePictureInPicture(1);
      break;
    case DecisionFailureReason::MAX:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

}  // namespace

const char* ToString(DecisionFailureReason failure_reason) {
  if (failure_reason == DecisionFailureReason::INVALID ||
      failure_reason == DecisionFailureReason::MAX)
    return nullptr;
  return kDecisionFailureReasonStrings[static_cast<size_t>(failure_reason)];
}

const char* ToString(DecisionSuccessReason success_reason) {
  if (success_reason == DecisionSuccessReason::INVALID ||
      success_reason == DecisionSuccessReason::MAX)
    return nullptr;
  return kDecisionSuccessReasonStrings[static_cast<size_t>(success_reason)];
}

DecisionDetails::Reason::Reason()
    : success_reason_(DecisionSuccessReason::INVALID),
      failure_reason_(DecisionFailureReason::INVALID) {}

DecisionDetails::Reason::Reason(DecisionSuccessReason success_reason)
    : success_reason_(success_reason),
      failure_reason_(DecisionFailureReason::INVALID) {
  DCHECK(IsSuccess());
}

DecisionDetails::Reason::Reason(DecisionFailureReason failure_reason)
    : success_reason_(DecisionSuccessReason::INVALID),
      failure_reason_(failure_reason) {
  DCHECK(IsFailure());
}

DecisionDetails::Reason::Reason(const Reason& rhs) = default;
DecisionDetails::Reason::~Reason() = default;

DecisionDetails::Reason& DecisionDetails::Reason::operator=(const Reason& rhs) =
    default;

bool DecisionDetails::Reason::IsValid() const {
  return IsSuccess() || IsFailure();
}

bool DecisionDetails::Reason::IsSuccess() const {
  if (success_reason_ == DecisionSuccessReason::INVALID ||
      success_reason_ == DecisionSuccessReason::MAX ||
      failure_reason_ != DecisionFailureReason::INVALID)
    return false;
  return true;
}

bool DecisionDetails::Reason::IsFailure() const {
  if (failure_reason_ == DecisionFailureReason::INVALID ||
      failure_reason_ == DecisionFailureReason::MAX ||
      success_reason_ != DecisionSuccessReason::INVALID)
    return false;
  return true;
}

DecisionSuccessReason DecisionDetails::Reason::success_reason() const {
  DCHECK(IsSuccess());
  return success_reason_;
}

DecisionFailureReason DecisionDetails::Reason::failure_reason() const {
  DCHECK(IsFailure());
  return failure_reason_;
}

const char* DecisionDetails::Reason::ToString() const {
  if (!IsValid())
    return nullptr;
  if (IsSuccess())
    return ::resource_coordinator::ToString(success_reason_);
  DCHECK(IsFailure());
  return ::resource_coordinator::ToString(failure_reason_);
}

bool DecisionDetails::Reason::operator==(const Reason& rhs) const {
  return success_reason_ == rhs.success_reason_ &&
         failure_reason_ == rhs.failure_reason_;
}

bool DecisionDetails::Reason::operator!=(const Reason& rhs) const {
  return !(*this == rhs);
}

DecisionDetails::DecisionDetails() : toggled_(false) {}

DecisionDetails::~DecisionDetails() = default;

DecisionDetails& DecisionDetails::operator=(DecisionDetails&& rhs) {
  toggled_ = rhs.toggled_;
  reasons_ = std::move(rhs.reasons_);
  rhs.Clear();
  return *this;
}

bool DecisionDetails::AddReason(const Reason& reason) {
  reasons_.push_back(reason);
  return CheckIfToggled();
}

bool DecisionDetails::AddReason(DecisionFailureReason failure_reason) {
  reasons_.push_back(Reason(failure_reason));
  return CheckIfToggled();
}

bool DecisionDetails::AddReason(DecisionSuccessReason success_reason) {
  reasons_.push_back(Reason(success_reason));
  return CheckIfToggled();
}

bool DecisionDetails::IsPositive() const {
  // A decision without supporting reasons is negative by default.
  if (reasons_.empty())
    return false;
  return reasons_.front().IsSuccess();
}

DecisionSuccessReason DecisionDetails::SuccessReason() const {
  DCHECK(!reasons_.empty());
  return reasons_.front().success_reason();
}

DecisionFailureReason DecisionDetails::FailureReason() const {
  DCHECK(!reasons_.empty());
  return reasons_.front().failure_reason();
}

void DecisionDetails::Populate(
    ukm::builders::TabManager_LifecycleStateChange* ukm) const {
  DCHECK(!reasons_.empty());
  bool positive = IsPositive();
  ukm->SetOutcome(positive);
  for (const auto& reason : reasons_) {
    // Stop adding reasons once all of the initial reasons of the same type
    // have been added.
    bool success = reason.IsSuccess();
    if (success != positive)
      break;
    if (success) {
      PopulateSuccessReason(reason.success_reason(), ukm);
    } else {
      PopulateFailureReason(reason.failure_reason(), ukm);
    }
  }
}

std::vector<std::string> DecisionDetails::GetFailureReasonStrings() const {
  std::vector<std::string> reasons;
  for (const auto& reason : reasons_) {
    if (reason.IsSuccess())
      break;
    reasons.push_back(reason.ToString());
  }
  return reasons;
}

void DecisionDetails::Clear() {
  reasons_.clear();
  toggled_ = false;
}

bool DecisionDetails::CheckIfToggled() {
  if (toggled_)
    return true;
  if (reasons_.size() <= 1)
    return false;
  // Determine if the last reason is of a different type than the one before. If
  // so, then the toggle has occurred.
  toggled_ = reasons_[reasons_.size() - 1].IsSuccess() !=
             reasons_[reasons_.size() - 2].IsSuccess();
  return toggled_;
}

}  // namespace resource_coordinator
