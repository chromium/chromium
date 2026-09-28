// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/dangerous_download_dialog_bridge.h"

#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/core/common/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::ReturnRef;

class DangerousDownloadDialogBridgeTest : public testing::Test {
 public:
  DangerousDownloadDialogBridgeTest() {
    feature_list_.InitAndEnableFeature(
        safe_browsing::kMaliciousApkDownloadCheck);
  }
  ~DangerousDownloadDialogBridgeTest() override = default;

 protected:
  base::test::ScopedFeatureList feature_list_;
  download::MockDownloadItem item_;
};

TEST_F(DangerousDownloadDialogBridgeTest, GetDownloadDomain_PrefersTabUrl) {
  GURL tab_url("https://tab.example.com/path");
  std::optional<url::Origin> initiator =
      url::Origin::Create(GURL("https://initiator.example.com"));
  GURL original_url("https://original.example.com/file.apk");
  GURL download_url("https://cdn.example.com/file.apk");

  EXPECT_CALL(item_, GetTabUrl()).WillRepeatedly(ReturnRef(tab_url));
  EXPECT_CALL(item_, GetRequestInitiator())
      .WillRepeatedly(ReturnRef(initiator));
  EXPECT_CALL(item_, GetOriginalUrl()).WillRepeatedly(ReturnRef(original_url));
  EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRef(download_url));

  EXPECT_EQ(u"tab.example.com",
            DangerousDownloadDialogBridge::GetDownloadDomainForTesting(&item_));
}

TEST_F(DangerousDownloadDialogBridgeTest,
       GetDownloadDomain_FallsBackToRequestInitiator) {
  GURL empty_tab_url;
  std::optional<url::Origin> initiator =
      url::Origin::Create(GURL("https://initiator.example.com"));
  GURL original_url("https://original.example.com/file.apk");
  GURL download_url("https://cdn.example.com/file.apk");

  EXPECT_CALL(item_, GetTabUrl()).WillRepeatedly(ReturnRef(empty_tab_url));
  EXPECT_CALL(item_, GetRequestInitiator())
      .WillRepeatedly(ReturnRef(initiator));
  EXPECT_CALL(item_, GetOriginalUrl()).WillRepeatedly(ReturnRef(original_url));
  EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRef(download_url));

  EXPECT_EQ(u"initiator.example.com",
            DangerousDownloadDialogBridge::GetDownloadDomainForTesting(&item_));
}

TEST_F(DangerousDownloadDialogBridgeTest,
       GetDownloadDomain_FallsBackToOriginalUrl) {
  GURL empty_tab_url;
  std::optional<url::Origin> null_initiator = std::nullopt;
  GURL original_url("https://original.example.com/file.apk");
  GURL download_url("https://cdn.example.com/file.apk");

  EXPECT_CALL(item_, GetTabUrl()).WillRepeatedly(ReturnRef(empty_tab_url));
  EXPECT_CALL(item_, GetRequestInitiator())
      .WillRepeatedly(ReturnRef(null_initiator));
  EXPECT_CALL(item_, GetOriginalUrl()).WillRepeatedly(ReturnRef(original_url));
  EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRef(download_url));

  EXPECT_EQ(u"original.example.com",
            DangerousDownloadDialogBridge::GetDownloadDomainForTesting(&item_));
}

TEST_F(DangerousDownloadDialogBridgeTest, GetDownloadDomain_FallsBackToUrl) {
  GURL empty_tab_url;
  std::optional<url::Origin> null_initiator = std::nullopt;
  GURL empty_original_url;
  GURL download_url("https://cdn.example.com/file.apk");

  EXPECT_CALL(item_, GetTabUrl()).WillRepeatedly(ReturnRef(empty_tab_url));
  EXPECT_CALL(item_, GetRequestInitiator())
      .WillRepeatedly(ReturnRef(null_initiator));
  EXPECT_CALL(item_, GetOriginalUrl())
      .WillRepeatedly(ReturnRef(empty_original_url));
  EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRef(download_url));

  EXPECT_EQ(u"cdn.example.com",
            DangerousDownloadDialogBridge::GetDownloadDomainForTesting(&item_));
}

TEST_F(DangerousDownloadDialogBridgeTest,
       GetDownloadDomain_BlobUrlExtractsInnerOrigin) {
  GURL empty_tab_url;
  std::optional<url::Origin> null_initiator = std::nullopt;
  GURL empty_original_url;
  GURL blob_url("blob:https://blob.example.com/1234-5678");

  EXPECT_CALL(item_, GetTabUrl()).WillRepeatedly(ReturnRef(empty_tab_url));
  EXPECT_CALL(item_, GetRequestInitiator())
      .WillRepeatedly(ReturnRef(null_initiator));
  EXPECT_CALL(item_, GetOriginalUrl())
      .WillRepeatedly(ReturnRef(empty_original_url));
  EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRef(blob_url));

  EXPECT_EQ(u"blob.example.com",
            DangerousDownloadDialogBridge::GetDownloadDomainForTesting(&item_));
}

TEST_F(DangerousDownloadDialogBridgeTest,
       GetDownloadDomain_OpaqueOriginReturnsEmpty) {
  GURL empty_tab_url;
  std::optional<url::Origin> null_initiator = std::nullopt;
  GURL empty_original_url;
  GURL data_url("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==");

  EXPECT_CALL(item_, GetTabUrl()).WillRepeatedly(ReturnRef(empty_tab_url));
  EXPECT_CALL(item_, GetRequestInitiator())
      .WillRepeatedly(ReturnRef(null_initiator));
  EXPECT_CALL(item_, GetOriginalUrl())
      .WillRepeatedly(ReturnRef(empty_original_url));
  EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRef(data_url));

  EXPECT_EQ(u"",
            DangerousDownloadDialogBridge::GetDownloadDomainForTesting(&item_));
}

TEST_F(DangerousDownloadDialogBridgeTest,
       GetDownloadDomain_FeatureDisabled_FallsBackToDownloadUrl) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(safe_browsing::kMaliciousApkDownloadCheck);

  GURL tab_url("https://tab.example.com/path");
  std::optional<url::Origin> initiator =
      url::Origin::Create(GURL("https://initiator.example.com"));
  GURL original_url("https://original.example.com/file.apk");
  GURL download_url("https://cdn.example.com/file.apk");

  EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRef(download_url));

  EXPECT_EQ(u"cdn.example.com",
            DangerousDownloadDialogBridge::GetDownloadDomainForTesting(&item_));
}

TEST_F(DangerousDownloadDialogBridgeTest,
       GetDownloadDomain_FeatureDisabled_OpaqueOriginReturnsEmpty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(safe_browsing::kMaliciousApkDownloadCheck);

  GURL data_url("data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==");
  EXPECT_CALL(item_, GetURL()).WillRepeatedly(ReturnRef(data_url));

  EXPECT_EQ(u"",
            DangerousDownloadDialogBridge::GetDownloadDomainForTesting(&item_));
}
