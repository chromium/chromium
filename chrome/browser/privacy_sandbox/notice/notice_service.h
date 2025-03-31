// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/privacy_sandbox_notice.mojom.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

namespace privacy_sandbox {

// This class will:
// 1. Communicate with the Notice Storage Service
// 2. Communicate with the API Services to determine eligibility
// 3. Determine which views are required to get them to the latest API version
// 4. Keeps an internal registry to keep track of when notices were shown,
// what actions were taken on them and how
class PrivacySandboxNoticeService : public KeyedService {
 public:
  explicit PrivacySandboxNoticeService(Profile* profile);
  ~PrivacySandboxNoticeService() override;

  std::vector<notice::mojom::PrivacySandboxNotice> GetRequiredNotices(
      SurfaceType surface);

  void EventOccurred(NoticeId notice_id, NoticeEvent event);

  // Service Accessors.
  PrivacySandboxNoticeStorage* GetNoticeStorage();
  PrefService* GetPrefService();
  NoticeCatalog* GetCatalog();

  // KeyedService:
  void Shutdown() override;

 private:
  // TODO(crbug.com/392612108): Create eligibility and notice result callbacks.
  raw_ptr<Profile> profile_;
  std::unique_ptr<NoticeCatalog> catalog_;
  std::unique_ptr<PrivacySandboxNoticeStorage> notice_storage_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_SERVICE_H_
