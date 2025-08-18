// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_CATALOG_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_CATALOG_H_

#include <absl/container/flat_hash_map.h>

#include "base/containers/span.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"

class Profile;

namespace privacy_sandbox {

class NoticeCatalog {
 public:
  virtual ~NoticeCatalog() = default;
  // Accessors.
  virtual base::span<NoticeApi*> GetNoticeApis() = 0;
  virtual base::span<Notice*> GetNotices() = 0;
  virtual Notice* GetNotice(NoticeId notice_id) = 0;
};

class NoticeCatalogImpl : public NoticeCatalog {
 public:
  explicit NoticeCatalogImpl(Profile* profile);
  ~NoticeCatalogImpl() override;

  base::span<NoticeApi*> GetNoticeApis() override;
  base::span<Notice*> GetNotices() override;
  Notice* GetNotice(NoticeId notice_id) override;

 private:
  // Registers a new API and returns a pointer to it.
  NoticeApi* RegisterAndRetrieveNewApi();

  // Registers a new notice and returns a pointer to it.
  Notice* RegisterAndRetrieveNewNotice(
      std::unique_ptr<Notice> (*notice_creator)(NoticeId),
      NoticeId notice_id);

  // Registers a group of notices with the same requirements to be shown (for
  // ex. Topics can have TopicsClankBrApp, TopicsDesktop and TopicsClankCCT)
  void RegisterNoticeGroup(
      std::unique_ptr<Notice> (*notice_creator)(NoticeId),
      std::vector<std::pair<NoticeId, const base::Feature*>>&& notice_ids,
      std::vector<NoticeApi*>&& target_apis,
      std::vector<NoticeApi*>&& pre_req_apis = {},
      std::pair<NoticeViewGroup, int> view_group = {NoticeViewGroup::kNotSet,
                                                    0});

  // Populates the catalog with all the notices and their requirements.
  void Populate();

  template <typename T>
  auto EligibilityCallback(auto (T::*f)());

  template <typename T>
  T* GetApiService();

  raw_ptr<Profile> profile_;
  std::vector<std::unique_ptr<NoticeApi>> apis_;
  absl::flat_hash_map<NoticeId, std::unique_ptr<Notice>> notices_;
  std::vector<Notice*> notice_ptrs_;
  std::vector<NoticeApi*> apis_ptrs_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_CATALOG_H_
