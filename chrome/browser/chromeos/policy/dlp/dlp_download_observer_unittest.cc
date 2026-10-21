// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_download_observer.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_download_observer_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_save_item_data.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/file_access/scoped_file_access.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using testing::_;

namespace {
constexpr char kFilePath[] = "/path/to/file";
constexpr char kTabUrl[] = "https://example.com/tab";
constexpr char kDataUrl[] = "data:text/plain;charset=UTF-8,test";
constexpr char kHttpsUrl[] = "https://example.com/https";
constexpr char kReferrerUrl[] = "https://referrer.example.com";
constexpr char kBlobUrl[] = "blob:https://example.com/uuid";
constexpr char kOriginUrl[] = "https://example.com/";
}  // namespace

class DlpDownloadObserverTest : public testing::Test {
 public:
  void SetUp() override {
    chromeos::DlpClient::Get()->InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
  }
  void TearDown() override {
    profile_.reset();
    chromeos::DlpClient::Get()->Shutdown();
  }

  DlpDownloadObserver* observer() {
    return DlpDownloadObserverFactory::GetForProfile(profile_.get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// Tests if we correctly request dlp file access for a data: url with the tab
// url.
TEST_F(DlpDownloadObserverTest, TestDataSchemeRewrite) {
  base::MockRepeatingCallback<void(const dlp::AddFilesRequest,
                                   chromeos::DlpClient::AddFilesCallback)>
      add_files_cb;
  EXPECT_CALL(
      add_files_cb,
      Run(testing::Property(&dlp::AddFilesRequest::add_file_requests,
                            testing::ElementsAre(testing::Property(
                                &dlp::AddFileRequest::source_url, kTabUrl))),
          _))
      .WillOnce(base::test::RunOnceCallback<1>(
          dlp::AddFilesResponse::default_instance()));
  auto* dlp_client = chromeos::DlpClient::Get()->GetTestInterface();
  dlp_client->SetAddFilesMock(add_files_cb.Get());

  testing::NiceMock<download::MockDownloadItem> item;
  base::FilePath file_path(kFilePath);
  GURL data_url(kDataUrl);
  GURL referrer_url(kReferrerUrl);
  GURL tab_url(kTabUrl);

  ON_CALL(item, IsSavePackageDownload).WillByDefault(testing::Return(false));
  ON_CALL(item, GetState)
      .WillByDefault(
          testing::Return(download::DownloadItem::DownloadState::COMPLETE));
  ON_CALL(item, GetFullPath).WillByDefault(testing::ReturnRef(file_path));
  ON_CALL(item, GetURL).WillByDefault(testing::ReturnRef(data_url));
  ON_CALL(item, GetReferrerUrl).WillByDefault(testing::ReturnRef(referrer_url));
  ON_CALL(item, GetTabUrl).WillByDefault(testing::ReturnRef(tab_url));

  observer()->OnDownloadUpdated(&item);
}

// Test if we request file access with the original url in the default case.
TEST_F(DlpDownloadObserverTest, TestNoSchemeRewrite) {
  base::MockRepeatingCallback<void(const dlp::AddFilesRequest,
                                   chromeos::DlpClient::AddFilesCallback)>
      add_files_cb;
  EXPECT_CALL(
      add_files_cb,
      Run(testing::Property(&dlp::AddFilesRequest::add_file_requests,
                            testing::ElementsAre(testing::Property(
                                &dlp::AddFileRequest::source_url, kHttpsUrl))),
          _))
      .WillOnce(base::test::RunOnceCallback<1>(
          dlp::AddFilesResponse::default_instance()));
  auto* dlp_client = chromeos::DlpClient::Get()->GetTestInterface();
  dlp_client->SetAddFilesMock(add_files_cb.Get());

  testing::NiceMock<download::MockDownloadItem> item;
  base::FilePath file_path(kFilePath);
  GURL https_url(kHttpsUrl);
  GURL referrer_url(kReferrerUrl);
  GURL tab_url(kTabUrl);

  ON_CALL(item, IsSavePackageDownload).WillByDefault(testing::Return(false));
  ON_CALL(item, GetState)
      .WillByDefault(
          testing::Return(download::DownloadItem::DownloadState::COMPLETE));
  ON_CALL(item, GetFullPath).WillByDefault(testing::ReturnRef(file_path));
  ON_CALL(item, GetURL).WillByDefault(testing::ReturnRef(https_url));
  ON_CALL(item, GetReferrerUrl).WillByDefault(testing::ReturnRef(referrer_url));
  ON_CALL(item, GetTabUrl).WillByDefault(testing::ReturnRef(tab_url));
  observer()->OnDownloadUpdated(&item);
}

// Test if we request the file access for a blob: url with its origin. We do
// this as the url matcher would not recognize the host in the blob: url.
TEST_F(DlpDownloadObserverTest, TestBlobSchemeRewrite) {
  base::MockRepeatingCallback<void(const dlp::AddFilesRequest,
                                   chromeos::DlpClient::AddFilesCallback)>
      add_files_cb;
  EXPECT_CALL(
      add_files_cb,
      Run(testing::Property(&dlp::AddFilesRequest::add_file_requests,
                            testing::ElementsAre(testing::Property(
                                &dlp::AddFileRequest::source_url, kOriginUrl))),
          _))
      .WillOnce(base::test::RunOnceCallback<1>(
          dlp::AddFilesResponse::default_instance()));
  auto* dlp_client = chromeos::DlpClient::Get()->GetTestInterface();
  dlp_client->SetAddFilesMock(add_files_cb.Get());

  testing::NiceMock<download::MockDownloadItem> item;
  base::FilePath file_path(kFilePath);
  GURL blob_url(kBlobUrl);
  GURL referrer_url(kReferrerUrl);
  GURL tab_url(kTabUrl);

  ON_CALL(item, IsSavePackageDownload).WillByDefault(testing::Return(false));
  ON_CALL(item, GetState)
      .WillByDefault(
          testing::Return(download::DownloadItem::DownloadState::COMPLETE));
  ON_CALL(item, GetFullPath).WillByDefault(testing::ReturnRef(file_path));
  ON_CALL(item, GetURL).WillByDefault(testing::ReturnRef(blob_url));
  ON_CALL(item, GetReferrerUrl).WillByDefault(testing::ReturnRef(referrer_url));
  ON_CALL(item, GetTabUrl).WillByDefault(testing::ReturnRef(tab_url));

  observer()->OnDownloadUpdated(&item);
}

// Test if we request the file access for a data: url, while the tab is a blob:
// url, the origin of the tab is used as source.
TEST_F(DlpDownloadObserverTest, TestDataSchemeInBlobTabRewrite) {
  base::MockRepeatingCallback<void(const dlp::AddFilesRequest,
                                   chromeos::DlpClient::AddFilesCallback)>
      add_files_cb;
  EXPECT_CALL(
      add_files_cb,
      Run(testing::Property(&dlp::AddFilesRequest::add_file_requests,
                            testing::ElementsAre(testing::Property(
                                &dlp::AddFileRequest::source_url, kOriginUrl))),
          _))
      .WillOnce(base::test::RunOnceCallback<1>(
          dlp::AddFilesResponse::default_instance()));
  auto* dlp_client = chromeos::DlpClient::Get()->GetTestInterface();
  dlp_client->SetAddFilesMock(add_files_cb.Get());

  testing::NiceMock<download::MockDownloadItem> item;
  base::FilePath file_path(kFilePath);
  GURL blob_url(kBlobUrl);
  GURL referrer_url(kReferrerUrl);
  GURL data_url(kDataUrl);

  ON_CALL(item, IsSavePackageDownload).WillByDefault(testing::Return(false));
  ON_CALL(item, GetState)
      .WillByDefault(
          testing::Return(download::DownloadItem::DownloadState::COMPLETE));
  ON_CALL(item, GetFullPath).WillByDefault(testing::ReturnRef(file_path));
  ON_CALL(item, GetURL).WillByDefault(testing::ReturnRef(data_url));
  ON_CALL(item, GetReferrerUrl).WillByDefault(testing::ReturnRef(referrer_url));
  ON_CALL(item, GetTabUrl).WillByDefault(testing::ReturnRef(blob_url));

  observer()->OnDownloadUpdated(&item);
}

// This test proves that if the daemon is unavailable at the moment the
// download is created, the download is still registered for DLP tracking
// after it finishes if the daemon is back up.
TEST_F(DlpDownloadObserverTest, DaemonDownAtCreation_FileIsTracked) {
  base::MockRepeatingCallback<void(const dlp::AddFilesRequest,
                                   chromeos::DlpClient::AddFilesCallback)>
      add_files_cb;

  // We expect add_files_cb to be called once when the download completes.
  EXPECT_CALL(add_files_cb, Run(_, _))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<1>(
          dlp::AddFilesResponse::default_instance()));

  auto* dlp_client = chromeos::DlpClient::Get()->GetTestInterface();
  dlp_client->SetAddFilesMock(add_files_cb.Get());

  // 1. Daemon is DOWN at creation time
  dlp_client->SetIsAlive(false);

  testing::NiceMock<download::MockDownloadItem> item;

  // OnDownloadCreated is called when the download starts.
  observer()->OnDownloadCreated(&item);

  // 2. Daemon comes back up
  dlp_client->SetIsAlive(true);

  // 3. Download finishes
  ON_CALL(item, IsSavePackageDownload).WillByDefault(testing::Return(false));
  ON_CALL(item, GetState)
      .WillByDefault(
          testing::Return(download::DownloadItem::DownloadState::COMPLETE));
  base::FilePath file_path(kFilePath);
  GURL https_url(kHttpsUrl);
  GURL referrer_url(kReferrerUrl);
  GURL tab_url(kTabUrl);

  ON_CALL(item, GetFullPath).WillByDefault(testing::ReturnRef(file_path));
  ON_CALL(item, GetURL).WillByDefault(testing::ReturnRef(https_url));
  ON_CALL(item, GetReferrerUrl).WillByDefault(testing::ReturnRef(referrer_url));
  ON_CALL(item, GetTabUrl).WillByDefault(testing::ReturnRef(tab_url));

  // Trigger the notification that normally comes from DownloadManager calling
  // observers
  item.NotifyObserversDownloadUpdated();
}

// Control test: shows it works when daemon is alive at creation
TEST_F(DlpDownloadObserverTest, DaemonAliveAtCreation_FileIsTracked) {
  base::MockRepeatingCallback<void(const dlp::AddFilesRequest,
                                   chromeos::DlpClient::AddFilesCallback)>
      add_files_cb;

  EXPECT_CALL(add_files_cb, Run(_, _))
      .Times(1)
      .WillOnce(base::test::RunOnceCallback<1>(
          dlp::AddFilesResponse::default_instance()));

  auto* dlp_client = chromeos::DlpClient::Get()->GetTestInterface();
  dlp_client->SetAddFilesMock(add_files_cb.Get());

  // 1. Daemon is UP at creation time
  dlp_client->SetIsAlive(true);

  testing::NiceMock<download::MockDownloadItem> item;

  // OnDownloadCreated is called when the download starts.
  observer()->OnDownloadCreated(&item);

  // 3. Download finishes
  ON_CALL(item, IsSavePackageDownload).WillByDefault(testing::Return(false));
  ON_CALL(item, GetState)
      .WillByDefault(
          testing::Return(download::DownloadItem::DownloadState::COMPLETE));
  base::FilePath file_path(kFilePath);
  GURL https_url(kHttpsUrl);
  GURL referrer_url(kReferrerUrl);
  GURL tab_url(kTabUrl);

  ON_CALL(item, GetFullPath).WillByDefault(testing::ReturnRef(file_path));
  ON_CALL(item, GetURL).WillByDefault(testing::ReturnRef(https_url));
  ON_CALL(item, GetReferrerUrl).WillByDefault(testing::ReturnRef(referrer_url));
  ON_CALL(item, GetTabUrl).WillByDefault(testing::ReturnRef(tab_url));

  // Trigger the notification that normally comes from DownloadManager calling
  // observers
  item.NotifyObserversDownloadUpdated();
}

}  // namespace policy
