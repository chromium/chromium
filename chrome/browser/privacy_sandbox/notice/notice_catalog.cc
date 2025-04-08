// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"

#include <utility>

#include "base/feature_list.h"

namespace privacy_sandbox {

NoticeCatalog::NoticeCatalog() = default;
NoticeCatalog::~NoticeCatalog() = default;

NoticeApi* NoticeCatalog::RegisterAndRetrieveNewApi() {
  apis_.emplace_back(std::make_unique<NoticeApi>());
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
