// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_MOCK_DOWNLOAD_STATUS_UPDATER_CLIENT_H_
#define CHROME_BROWSER_ASH_CROSAPI_MOCK_DOWNLOAD_STATUS_UPDATER_CLIENT_H_

#include <string>

#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace crosapi {

// A mock download status updater client for testing.
class MockDownloadStatusUpdaterClient
    : public mojom::DownloadStatusUpdaterClient {
 public:
  MockDownloadStatusUpdaterClient();
  MockDownloadStatusUpdaterClient(const MockDownloadStatusUpdaterClient&) =
      delete;
  MockDownloadStatusUpdaterClient& operator=(
      const MockDownloadStatusUpdaterClient&) = delete;
  ~MockDownloadStatusUpdaterClient() override;

  // mojom::DownloadStatusUpdaterClient:
  MOCK_METHOD(void,
              Cancel,
              (const std::string& guid, CancelCallback callback),
              (override));
  MOCK_METHOD(void,
              Pause,
              (const std::string& guid, PauseCallback callback),
              (override));
  MOCK_METHOD(void,
              Resume,
              (const std::string& guid, ResumeCallback callback),
              (override));
  MOCK_METHOD(void,
              ShowInBrowser,
              (const std::string& guid, ShowInBrowserCallback callback),
              (override));
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_MOCK_DOWNLOAD_STATUS_UPDATER_CLIENT_H_
