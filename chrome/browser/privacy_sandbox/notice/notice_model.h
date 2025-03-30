// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_MODEL_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_MODEL_H_

#include <absl/container/flat_hash_map.h>

#include "components/privacy_sandbox/privacy_sandbox_notice.mojom.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

namespace privacy_sandbox {
class NoticeApi;

// Types of notices that can be shown.
enum class NoticeType {
  kNotice,   // This type of notice requires a user to have acknowledged it.
  kConsent,  // This type of notice requires an explicit choice to be made.
};

// The different surface types a notice can be shown on.
enum SurfaceType {
  kDesktopNewTab,
  kClankBrApp,      // Clank Browser App.
  kClankCustomTab,  // Clank CCT.
};

using NoticeId = std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType>;
class Notice {
  // TODO(crbug.com/392612108): Include view group information.
 public:
  explicit Notice(NoticeId notice_id, const base::Feature*);
  Notice(const Notice& other);
  virtual ~Notice();

  // Sets Apis that need to be eligible or previously fulfilled to see this
  // notice.
  Notice* SetTargetApis(const std::vector<NoticeApi*>& apis);
  Notice* SetPreReqApis(const std::vector<NoticeApi*>& apis);

  // TODO(crbug.com/392612108): Implement a function to check if this
  // notice was ever fulfilled.

  // Accessors.
  const std::vector<raw_ptr<NoticeApi>>& GetTargetApis();
  const std::vector<raw_ptr<NoticeApi>>& GetPreReqApis();
  NoticeId GetNoticeId();
  const base::Feature* GetFeature();

  // Gets the type of notice.
  virtual NoticeType GetNoticeType();

  // TODO(crbug.com/392612108) NoticeViews should also implement a function to
  // guard against a notice showing in certain conditions, even if it is the
  // only one that fulfills a certain Api. Example of this: Measurement Only
  // notice showing for the wrong group of users: Over 18 for example.

 private:
  virtual std::vector<NoticeEvent> FulfillmentEvents() const;
  NoticeId notice_id_;
  std::vector<raw_ptr<NoticeApi>> target_apis_;
  std::vector<raw_ptr<NoticeApi>> pre_req_apis_;
  raw_ptr<const base::Feature> feature_;
};

class Consent : public Notice {
 public:
  explicit Consent(NoticeId notice_id, const base::Feature* feature);
  NoticeType GetNoticeType() override;

 private:
  std::vector<NoticeEvent> FulfillmentEvents() const override;
};

class NoticeApi {
 public:
  NoticeApi();
  NoticeApi(const NoticeApi& other);
  ~NoticeApi();

  // Accessors.
  const std::vector<Notice*>& GetLinkedNotices();

  // TODO(crbug.com/392612108): Add required callbacks.

  // TODO(crbug.com/392612108): Have enablement of an api set by a feature
  // flag.

  // Sets a notice this Api can be fulfilled by.
  void CanBeFulfilledBy(Notice* notice);

  // TODO(crbug.com/392612108): Implement a function to check whether the Api
  // requirement is fulfilled. This should check eligibility & if a notice was
  // found to successfully fulfill this api's requirements.

 private:
  std::vector<Notice*> linked_notices_;
};

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

  // Template implementation needs to be inline to bypass linkage issues, other
  // classes need access to the template implementation source.
  // Registers a new notice.
  template <typename T>
  Notice* RegisterAndRetrieveNewNotice(NoticeId notice_id,
                                       const base::Feature* feature) {
    notices_.emplace(notice_id, std::make_unique<T>(T(notice_id, feature)));
    return notices_[notice_id].get();
  }

  // Registers a group of notices with the same requirements to be shown (for
  // ex. Topics can have TopicsClankBrApp, TopicsDesktop and TopicsClankCCT)
  template <typename T>
  void RegisterNoticeGroup(
      std::vector<std::pair<NoticeId, const base::Feature*>>&& notice_ids,
      std::vector<NoticeApi*>&& target_apis,
      std::vector<NoticeApi*>&& pre_req_apis = {}) {
    for (auto notice_id : notice_ids) {
      RegisterAndRetrieveNewNotice<T>(notice_id.first, notice_id.second)
          ->SetTargetApis(target_apis)
          ->SetPreReqApis(pre_req_apis);
    }
  }

 private:
  std::vector<std::unique_ptr<NoticeApi>> apis_;
  NoticeMap notices_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_MODEL_H_
