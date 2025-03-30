// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
namespace privacy_sandbox {

// NoticeApi class definitions.
NoticeApi::NoticeApi() = default;
NoticeApi::NoticeApi(const NoticeApi& other) = default;
NoticeApi::~NoticeApi() = default;

void NoticeApi::CanBeFulfilledBy(Notice* notice) {
  linked_notices_.emplace_back(notice);
}

const std::vector<Notice*>& NoticeApi::GetLinkedNotices() {
  return linked_notices_;
}

// Notice class definitions.
Notice::Notice(NoticeId notice_id, const base::Feature* feature)
    : notice_id_(notice_id), feature_(feature) {}
Notice::Notice(const Notice& other) = default;
Notice::~Notice() = default;

const std::vector<raw_ptr<NoticeApi>>& Notice::GetTargetApis() {
  return target_apis_;
}

const std::vector<raw_ptr<NoticeApi>>& Notice::GetPreReqApis() {
  return pre_req_apis_;
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

std::vector<NoticeEvent> Notice::FulfillmentEvents() const {
  return {NoticeEvent::kAck, NoticeEvent::kSettings};
}

NoticeType Notice::GetNoticeType() {
  return NoticeType::kNotice;
}

// Consent class definitions.
Consent::Consent(NoticeId notice_id, const base::Feature* feature)
    : Notice(notice_id, feature) {}

std::vector<NoticeEvent> Consent::FulfillmentEvents() const {
  return {NoticeEvent::kOptIn, NoticeEvent::kOptOut};
}

NoticeType Consent::GetNoticeType() {
  return NoticeType::kConsent;
}

// Notice catalog class definitions.
NoticeCatalog::NoticeCatalog() = default;
NoticeCatalog::~NoticeCatalog() = default;

NoticeApi* NoticeCatalog::RegisterAndRetrieveNewApi() {
  apis_.push_back(std::make_unique<NoticeApi>(NoticeApi()));
  return apis_.back().get();
}

const std::vector<std::unique_ptr<NoticeApi>>& NoticeCatalog::GetNoticeApis() {
  return apis_;
}

const NoticeMap& NoticeCatalog::GetNoticeMap() {
  return notices_;
}

}  // namespace privacy_sandbox
