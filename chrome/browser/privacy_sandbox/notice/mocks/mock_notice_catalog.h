// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_NOTICE_CATALOG_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_NOTICE_CATALOG_H_

#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace privacy_sandbox {

class MockNoticeCatalog : public NoticeCatalog {
 public:
  MockNoticeCatalog();
  ~MockNoticeCatalog() override;

  MOCK_METHOD(const std::vector<std::unique_ptr<NoticeApi>>&,
              GetNoticeApis,
              (),
              (override));
  MOCK_METHOD(const NoticeMap&, GetNoticeMap, (), (override));
  MOCK_METHOD(Notice*, GetNotice, (NoticeId), (override));
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_NOTICE_CATALOG_H_
