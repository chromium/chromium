// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_TEST_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_TEST_HELPER_H_

#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "components/download/public/common/download_item_rename_progress_update.h"
#include "content/public/test/fake_download_item.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {

// 1994-04-27 00:00:00.001 GMT+2 (South Africa Standard Time)
const base::Time::Exploded kTestDateTime = {1994, 4, 2, 27, 0, 0, 0, 1};

// TODO(b/256182367): This was kept to not break FileSystemRenameHandler tests.
// Remove when deleting the file_system/ directory.
constexpr char kWildcardSendDownloadToCloudPref[] = R"([
  {
    "service_provider": "box",
    "enterprise_id": "1234567890",
    "enable": [
      {
        "url_list": ["*"],
        "mime_types": ["*"]
      }
    ]
  }
])";

class DownloadItemForTest : public content::FakeDownloadItem {
 public:
  explicit DownloadItemForTest(base::FilePath::StringPieceType file_name,
                               base::Time::Exploded start_time = kTestDateTime);
  ~DownloadItemForTest() override;

  const base::FilePath& GetFullPath() const override;
  DownloadState GetState() const override;
  const DownloadItemRerouteInfo& GetRerouteInfo() const override;

  void SetState(DownloadState state);
  void SetRerouteInfo(DownloadItemRerouteInfo info);

  using content::FakeDownloadItem::ClearAllUserData;

 protected:
  base::ScopedTempDir tmp_dir_;
  base::FilePath path_;
  DownloadState state_ = DownloadState::IN_PROGRESS;
  DownloadItemRerouteInfo rerouted_info_;
};

class MockApiCallFlow : public OAuth2ApiCallFlow {
 public:
  MockApiCallFlow();
  ~MockApiCallFlow() override;

 protected:
  MOCK_METHOD0(CreateApiCallUrl, GURL());
  MOCK_METHOD0(CreateApiCallBody, std::string());
  MOCK_METHOD(void,
              ProcessApiCallSuccess,
              (const network::mojom::URLResponseHead*,
               std::unique_ptr<std::string>),
              (override));
  MOCK_METHOD(void,
              ProcessApiCallFailure,
              (int,
               const network::mojom::URLResponseHead*,
               std::unique_ptr<std::string>),
              (override));
  MOCK_METHOD(net::PartialNetworkTrafficAnnotationTag,
              GetNetworkTrafficAnnotationTag,
              (),
              (override));
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_TEST_HELPER_H_
