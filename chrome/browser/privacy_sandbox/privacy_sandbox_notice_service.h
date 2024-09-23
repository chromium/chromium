// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"

namespace privacy_sandbox {

// TODO(crbug.com/333408794): Add browser tests for this service once it's wired
// in to everything to test that prefs are migrated correctly on notice
// confirmation.
class PrivacySandboxNoticeService : public KeyedService {
 public:
  explicit PrivacySandboxNoticeService(PrefService* pref_service);
  ~PrivacySandboxNoticeService() override;

  PrivacySandboxNoticeStorage* GetNoticeStorage();

  // KeyedService:
  void Shutdown() override;

 private:
  // TODO(crbug.com/333406690): Remove this once the old privacy sandbox prefs
  // are migrated to the new data model.
  void MigratePrivacySandboxPrefsToDataModel();

  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<PrivacySandboxNoticeStorage> notice_storage_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_SERVICE_H_
