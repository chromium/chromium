// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"

namespace privacy_sandbox {

using notice::mojom::PrivacySandboxNoticeEvent;

// NoticeApi class definitions.
NoticeApi::NoticeApi() = default;
NoticeApi::~NoticeApi() = default;

void NoticeApi::SetFulfilledBy(Notice* notice) {
  linked_notices_.emplace_back(notice);
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

NoticeApi* NoticeApi::SetFeature(const base::Feature* feature) {
  feature_ = feature;
  return this;
}

bool NoticeApi::IsEnabled() {
  return feature_ && base::FeatureList::IsEnabled(*feature_);
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

  for (Notice* notice : linked_notices()) {
    if (eligibility == EligibilityLevel::kEligibleConsent &&
        notice->GetNoticeType() == NoticeType::kNotice) {
      continue;
    }
    if (notice->was_fulfilled()) {
      return true;
    };
  }
  return false;
}

// Notice class definitions.
Notice::Notice(NoticeId notice_id) : notice_id_(notice_id) {}
Notice::~Notice() = default;

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
    api->SetFulfilledBy(this);
  }
  return this;
}

Notice* Notice::SetViewGroup(std::pair<NoticeViewGroup, int> view_group) {
  view_group_ = view_group;
  return this;
}

const char* Notice::GetStorageName() const {
  CHECK(feature());
  return feature()->name;
}

void Notice::RefreshFulfillmentStatus(NoticeStorage& storage) {
  auto data = storage.ReadNoticeData(GetStorageName());
  if (!data) {
    was_fulfilled_ = false;
    return;
  }

  for (const auto& event_pair_ptr : data->notice_events) {
    if (!event_pair_ptr) {
      continue;
    }
    if (EvaluateNoticeEvent(event_pair_ptr->event).has_value()) {
      was_fulfilled_ = true;
      return;
    }
  }
  was_fulfilled_ = false;
}

bool Notice::CanFulfillAllTargetApis() {
  // TODO(crbug.com/417727236) Add caching here: We shouldn't recompute this
  // every time.
  for (NoticeApi* api : target_apis()) {
    auto eligibility = api->GetEligibilityLevel();
    if (eligibility == EligibilityLevel::kEligibleConsent &&
        GetNoticeType() == NoticeType::kConsent) {
      continue;
    }
    if (eligibility == EligibilityLevel::kEligibleNotice &&
        GetNoticeType() == NoticeType::kNotice) {
      continue;
    }
    return false;
  }
  return true;
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
  for (NoticeApi* api : target_apis()) {
    api->UpdateResult(*result);
  }
}

bool Notice::IsEnabled() const {
  return feature() && base::FeatureList::IsEnabled(*feature());
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
