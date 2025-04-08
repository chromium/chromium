// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_CATALOG_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_CATALOG_H_

#include <absl/container/flat_hash_map.h>

#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

namespace privacy_sandbox {

using NoticeMap = absl::flat_hash_map<NoticeId, std::unique_ptr<Notice>>;

class NoticeCatalog {
 public:
  NoticeCatalog();
  ~NoticeCatalog();

  // Accessors.
  const std::vector<std::unique_ptr<NoticeApi>>& GetNoticeApis();
  const NoticeMap& GetNoticeMap();

  // Registers a new notice api.
  NoticeApi* RegisterAndRetrieveNewApi();

  // Registers a new notice.
  Notice* RegisterAndRetrieveNewNotice(
      std::unique_ptr<Notice> (*notice_creator)(NoticeId),
      NoticeId notice_id);

  // Registers a group of notices with the same requirements to be shown (for
  // ex. Topics can have TopicsClankBrApp, TopicsDesktop and TopicsClankCCT)
  void RegisterNoticeGroup(
      std::unique_ptr<Notice> (*notice_creator)(NoticeId),
      std::vector<std::pair<NoticeId, const base::Feature*>>&& notice_ids,
      std::vector<NoticeApi*>&& target_apis,
      std::vector<NoticeApi*>&& pre_req_apis = {});

 private:
  std::vector<std::unique_ptr<NoticeApi>> apis_;
  NoticeMap notices_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_CATALOG_H_
