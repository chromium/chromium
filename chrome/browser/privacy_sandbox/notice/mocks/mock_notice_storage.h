// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_NOTICE_STORAGE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_NOTICE_STORAGE_H_

#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"
#include "testing/gmock/include/gmock/gmock.h"

class PrefService;

namespace privacy_sandbox {

class MockNoticeStorage : public NoticeStorage {
 public:
  MockNoticeStorage();
  ~MockNoticeStorage() override;

  MOCK_METHOD(void,
              RecordEvent,
              (PrefService * prefs,
               std::string_view notice,
               notice::mojom::PrivacySandboxNoticeEvent event,
               base::Time event_time),
              (override));

  MOCK_METHOD(std::optional<PrivacySandboxNoticeData>,
              ReadNoticeData,
              (PrefService * pref_service, std::string_view notice),
              (const, override));

  MOCK_METHOD(void,
              RecordHistogramsOnStartup,
              (PrefService * pref_service, std::string_view notice),
              (const, override));
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_NOTICE_STORAGE_H_
