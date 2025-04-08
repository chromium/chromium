// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

namespace privacy_sandbox {

using notice::mojom::PrivacySandboxNoticeEvent;

// NoticeApi class definitions.
NoticeApi::NoticeApi() = default;
NoticeApi::~NoticeApi() = default;

void NoticeApi::CanBeFulfilledBy(Notice* notice) {
  linked_notices_.emplace_back(notice);
}

const std::vector<Notice*>& NoticeApi::GetLinkedNotices() {
  return linked_notices_;
}

NoticeApi* NoticeApi::SetEligibilityCallback(
    base::RepeatingCallback<EligibilityLevel()> callback) {
  eligibility_callback_ = std::move(callback);
  return this;
}

NoticeApi* NoticeApi::SetResultCallback(
    base::OnceCallback<void(bool)> callback) {
  result_callback_ = std::move(callback);
  return this;
}

EligibilityLevel NoticeApi::GetEligibilityLevel() {
  return !eligibility_callback_.is_null() ? eligibility_callback_.Run()
                                          : EligibilityLevel::kNotEligible;
}

void NoticeApi::UpdateResult(bool enabled) {
  if (!result_callback_.is_null()) {
    std::move(result_callback_).Run(enabled);
  }
}

bool NoticeApi::IsFulfilled() {
  EligibilityLevel eligibility = GetEligibilityLevel();

  for (Notice* notice : linked_notices_) {
    if (eligibility == EligibilityLevel::kEligibleConsent &&
        notice->GetNoticeType() == NoticeType::kNotice) {
      continue;
    }
    return notice->WasFulfilled();
  }
  return false;
}

// Notice class definitions.
Notice::Notice(NoticeId notice_id) : notice_id_(notice_id) {}
Notice::~Notice() = default;

const std::vector<raw_ptr<NoticeApi>>& Notice::GetTargetApis() {
  return target_apis_;
}

const std::vector<raw_ptr<NoticeApi>>& Notice::GetPreReqApis() {
  return pre_req_apis_;
}

Notice* Notice::SetFeature(const base::Feature* feature) {
  feature_ = feature;
  return this;
}

Notice* Notice::SetPreReqApis(const std::vector<NoticeApi*>& apis) {
  std::transform(apis.begin(), apis.end(), std::back_inserter(pre_req_apis_),
                 std::identity());
  return this;
}

Notice* Notice::SetTargetApis(const std::vector<NoticeApi*>& apis) {
  std::transform(apis.begin(), apis.end(), std::back_inserter(target_apis_),
                 std::identity());
  for (NoticeApi* api : apis) {
    api->CanBeFulfilledBy(this);
  }
  return this;
}

NoticeId Notice::GetNoticeId() {
  return notice_id_;
}

const base::Feature* Notice::GetFeature() {
  return feature_;
}

bool Notice::WasFulfilled() {
  // TODO(crbug.com/392612108): Check if an action was taken on this notice, if
  // it was check if it was one of the fulfillment actions.
  return false;
}

std::optional<bool> Notice::EvaluateNoticeEvent(
    PrivacySandboxNoticeEvent event) {
  switch (event) {
    // Fulfillment : Yes
    case PrivacySandboxNoticeEvent::kAck:
    case PrivacySandboxNoticeEvent::kSettings:
      return true;
    // Not Fulfillment
    case PrivacySandboxNoticeEvent::kShown:
      return std::nullopt;
    // Unexpected.
    default:
      NOTREACHED();
  }
}

std::optional<bool> Consent::EvaluateNoticeEvent(
    PrivacySandboxNoticeEvent event) {
  switch (event) {
    // Fulfillment : Yes
    case PrivacySandboxNoticeEvent::kOptIn:
      return true;
    // Fulfillment : No
    case PrivacySandboxNoticeEvent::kOptOut:
      return false;
    // Not Fulfillment
    case PrivacySandboxNoticeEvent::kShown:
      return std::nullopt;
    // Unexpected.
    default:
      NOTREACHED();
  }
}

void Notice::UpdateTargetApiResults(PrivacySandboxNoticeEvent event) {
  std::optional<bool> result = EvaluateNoticeEvent(event);
  if (!result.has_value()) {
    return;
  }
  for (NoticeApi* api : target_apis_) {
    api->UpdateResult(*result);
  }
}

NoticeType Notice::GetNoticeType() {
  return NoticeType::kNotice;
}

// Consent class definitions.
Consent::Consent(NoticeId notice_id) : Notice(notice_id) {}

NoticeType Consent::GetNoticeType() {
  return NoticeType::kConsent;
}

}  // namespace privacy_sandbox
