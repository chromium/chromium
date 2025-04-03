// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

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

bool Notice::IsFulfillmentEvent(PrivacySandboxNoticeEvent event) {
  const std::set<PrivacySandboxNoticeEvent>& enabled_set =
      EnablementFulfillEvents();
  const std::set<PrivacySandboxNoticeEvent>& disabled_set =
      DisablementFulfillEvents();
  if (enabled_set.find(event) != enabled_set.end()) {
    return true;
  }
  if (disabled_set.find(event) != disabled_set.end()) {
    return true;
  }
  return false;
}

void Notice::UpdateTargetApiResults(PrivacySandboxNoticeEvent event) {
  if (!IsFulfillmentEvent(event)) {
    return;
  }
  const std::set<PrivacySandboxNoticeEvent>& enabled_set =
      EnablementFulfillEvents();
  const std::set<PrivacySandboxNoticeEvent>& disabled_set =
      DisablementFulfillEvents();
  for (NoticeApi* api : target_apis_) {
    if (enabled_set.find(event) != enabled_set.end()) {
      api->UpdateResult(true);
      continue;
    }
    if (disabled_set.find(event) != disabled_set.end()) {
      api->UpdateResult(false);
      continue;
    }
  }
}

NoticeType Notice::GetNoticeType() {
  return NoticeType::kNotice;
}

const std::set<PrivacySandboxNoticeEvent>& Notice::EnablementFulfillEvents() {
  static base::NoDestructor<std::set<PrivacySandboxNoticeEvent>> enabled_set{
      {PrivacySandboxNoticeEvent::kAck, PrivacySandboxNoticeEvent::kSettings}};
  return *enabled_set;
}

const std::set<PrivacySandboxNoticeEvent>& Notice::DisablementFulfillEvents() {
  static base::NoDestructor<std::set<PrivacySandboxNoticeEvent>> disabled_set{
      {}};
  return *disabled_set;
}

// Consent class definitions.
Consent::Consent(NoticeId notice_id) : Notice(notice_id) {}

NoticeType Consent::GetNoticeType() {
  return NoticeType::kConsent;
}

const std::set<PrivacySandboxNoticeEvent>& Consent::EnablementFulfillEvents() {
  static base::NoDestructor<std::set<PrivacySandboxNoticeEvent>> enabled_set{
      {PrivacySandboxNoticeEvent::kOptIn}};
  return *enabled_set;
}

const std::set<PrivacySandboxNoticeEvent>& Consent::DisablementFulfillEvents() {
  static base::NoDestructor<std::set<PrivacySandboxNoticeEvent>> disabled_set{
      {PrivacySandboxNoticeEvent::kOptOut}};
  return *disabled_set;
}

// Notice catalog class definitions.
NoticeCatalog::NoticeCatalog() = default;
NoticeCatalog::~NoticeCatalog() = default;

NoticeApi* NoticeCatalog::RegisterAndRetrieveNewApi() {
  apis_.push_back(std::make_unique<NoticeApi>());
  return apis_.back().get();
}

const std::vector<std::unique_ptr<NoticeApi>>& NoticeCatalog::GetNoticeApis() {
  return apis_;
}

Notice* NoticeCatalog::RegisterAndRetrieveNewNotice(
    std::unique_ptr<Notice> (*notice_creator)(NoticeId),
    NoticeId notice_id) {
  notices_.emplace(notice_id, notice_creator(notice_id));
  return notices_[notice_id].get();
}

void NoticeCatalog::RegisterNoticeGroup(
    std::unique_ptr<Notice> (*notice_creator)(NoticeId),
    std::vector<std::pair<NoticeId, const base::Feature*>>&& notice_ids,
    std::vector<NoticeApi*>&& target_apis,
    std::vector<NoticeApi*>&& pre_req_apis) {
  const std::vector<NoticeApi*>& pre_req_apis1 = pre_req_apis;
  for (auto [notice_id, feature] : notice_ids) {
    RegisterAndRetrieveNewNotice(notice_creator, notice_id)
        ->SetFeature(feature)
        ->SetTargetApis(target_apis)
        ->SetPreReqApis(pre_req_apis1);
  }
}

const NoticeMap& NoticeCatalog::GetNoticeMap() {
  return notices_;
}

}  // namespace privacy_sandbox
