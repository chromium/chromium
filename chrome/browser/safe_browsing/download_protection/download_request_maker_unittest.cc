// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"

#include "base/notreached.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/mock_binary_feature_extractor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/content/common/proto/download_file_types.pb.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/file_system_access_write_item.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom-test-utils.h"
#endif

namespace safe_browsing {

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

#if BUILDFLAG(IS_WIN)
class FakeUtilWin : public chrome::mojom::UtilWinInterceptorForTesting {
 public:
  chrome::mojom::UtilWin* GetForwardingInterface() override { NOTREACHED(); }

  void SetProducts(
      std::vector<metrics::SystemProfileProto::AntiVirusProduct> products) {
    products_ = std::move(products);
  }

  void GetAntiVirusProducts(bool report_full_names,
                            GetAntiVirusProductsCallback callback) override {
    std::move(callback).Run(products_);
  }

 private:
  std::vector<metrics::SystemProfileProto::AntiVirusProduct> products_;
};
#endif

class DownloadRequestMakerTest : public testing::Test {
 public:
  DownloadRequestMakerTest()
      : mock_feature_extractor_(
            new testing::StrictMock<MockBinaryFeatureExtractor>()) {}

  void RunRequestMaker(DownloadRequestMaker& request_maker) {
    base::RunLoop run_loop;
    request_maker.Start(base::BindLambdaForTesting(
        [&](DownloadRequestMaker::RequestCreationDetails details,
            std::unique_ptr<ClientDownloadRequest> request) {
          request_ = std::move(request);
          details_ = details;
          run_loop.Quit();
        }));
    run_loop.Run();
  }

#if BUILDFLAG(IS_WIN)
  void PopulateAntivirusProducts(
      std::vector<metrics::SystemProfileProto::AntiVirusProduct> products) {
    FakeUtilWin fake_util_win;
    fake_util_win.SetProducts(std::move(products));
    mojo::Receiver<chrome::mojom::UtilWin> receiver{&fake_util_win};

    AntiVirusMetricsProvider provider;
    provider.SetRemoteUtilWinForTesting(receiver.BindNewPipeAndPassRemote());
    base::RunLoop run_loop;
    provider.AsyncInit(run_loop.QuitClosure());
    run_loop.Run();
  }
#endif

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  scoped_refptr<MockBinaryFeatureExtractor> mock_feature_extractor_;

  // Receives the result of RunRequestMaker().
  std::unique_ptr<ClientDownloadRequest> request_;
  DownloadRequestMaker::RequestCreationDetails details_;
};

TEST_F(DownloadRequestMakerTest, PopulatesUrl) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));
  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL("https://example.com/download"),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr,
      /*password=*/std::nullopt, /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->url(), "https://example.com/download");
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, PopulatesHash) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));
  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"sha256_hash",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->digests().sha256(), "sha256_hash");
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, PopulatesLength) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));
  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/123,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->length(), 123);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, PopulatesResources) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));

  std::vector<ClientDownloadRequest::Resource> resources;
  ClientDownloadRequest::Resource resource1;
  resource1.set_url("resource1_url");
  resource1.set_type(ClientDownloadRequest::DOWNLOAD_URL);
  resources.push_back(resource1);

  ClientDownloadRequest::Resource resource2;
  resource2.set_url("resource2_url");
  resource2.set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
  resources.push_back(resource2);

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/resources,
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  ASSERT_EQ(request_->resources_size(), 2);
  EXPECT_EQ(request_->resources(0).url(), "resource1_url");
  EXPECT_EQ(request_->resources(0).type(), ClientDownloadRequest::DOWNLOAD_URL);
  EXPECT_EQ(request_->resources(1).url(), "resource2_url");
  EXPECT_EQ(request_->resources(1).type(),
            ClientDownloadRequest::DOWNLOAD_REDIRECT);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, PopulatesUserInitiated) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->user_initiated(), true);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, PopulatesReferrerChain) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));

  auto referrer_chain = std::make_unique<ReferrerChain>();
  ReferrerChainEntry* entry1 = referrer_chain->Add();
  entry1->set_url("entry1_url");
  entry1->set_type(ReferrerChainEntry::EVENT_URL);
  ReferrerChainEntry* entry2 = referrer_chain->Add();
  entry2->set_url("entry2_url");
  entry2->set_type(ReferrerChainEntry::RECENT_NAVIGATION);
  ReferrerChainData referrer_chain_data(
      ReferrerChainProvider::AttributionResult::SUCCESS,
      std::move(referrer_chain),
      /*referrer_chain_length=*/2,
      /*recent_navigation_to_collect=*/1);

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/&referrer_chain_data,
      /*password=*/std::nullopt, /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  ASSERT_EQ(request_->referrer_chain_size(), 2);
  EXPECT_EQ(request_->referrer_chain(0).url(), "entry1_url");
  EXPECT_EQ(request_->referrer_chain(0).type(), ReferrerChainEntry::EVENT_URL);
  EXPECT_EQ(request_->referrer_chain(1).url(), "entry2_url");
  EXPECT_EQ(request_->referrer_chain(1).type(),
            ReferrerChainEntry::RECENT_NAVIGATION);
  EXPECT_EQ(request_->referrer_chain_options().recent_navigations_to_collect(),
            1);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest,
       PopulatesReferrerChain_DoesNotMutateReferrerChainData) {
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));

  auto referrer_chain = std::make_unique<
      google::protobuf::RepeatedPtrField<ReferrerChainEntry>>();
  ReferrerChainEntry* entry1 = referrer_chain->Add();
  entry1->set_url("https://example.com/landing");
  ReferrerChainEntry* entry2 = referrer_chain->Add();
  entry2->set_url("https://example.com/referrer");
  ReferrerChainData referrer_chain_data(ReferrerChainProvider::SUCCESS,
                                        std::move(referrer_chain),
                                        /*referrer_chain_length=*/2,
                                        /*recent_navigation_to_collect=*/5);

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true, &referrer_chain_data,
      /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->referrer_chain_size(), 2);
  EXPECT_EQ(request_->referrer_chain(0).url(), "https://example.com/landing");
  EXPECT_EQ(request_->referrer_chain(1).url(), "https://example.com/referrer");
  ASSERT_NE(referrer_chain_data.GetReferrerChain(), nullptr);
  EXPECT_EQ(referrer_chain_data.GetReferrerChain()->size(), 2);
  EXPECT_EQ(referrer_chain_data.GetReferrerChain()->at(0).url(),
            "https://example.com/landing");
  EXPECT_EQ(referrer_chain_data.GetReferrerChain()->at(1).url(),
            "https://example.com/referrer");
}

TEST_F(DownloadRequestMakerTest, PopulatesStandardProtection) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));

  SetSafeBrowsingState(profile_.GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->population().user_population(),
            ChromeUserPopulation::SAFE_BROWSING);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, PopulatesEnhancedProtection) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));

  SetSafeBrowsingState(profile_.GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->population().user_population(),
            ChromeUserPopulation::ENHANCED_PROTECTION);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, PopulateTailoredInfo) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _)).Times(1);
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->tailored_info().version(), 5);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, PopulatesFileBasename) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/
      base::FilePath(FILE_PATH_LITERAL("target_file_name.exe")), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->file_basename(), "target_file_name.exe");
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, CreatesFromDownloadItem) {
  download::MockDownloadItem mock_download_item;
  EXPECT_CALL(mock_download_item, GetUrlChain())
      .WillRepeatedly(ReturnRefOfCopy(
          std::vector<GURL>{GURL("https://example.com/redirect"),
                            GURL("https://example.com/download")}));
  EXPECT_CALL(mock_download_item, GetTabUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/tab_url")));
  EXPECT_CALL(mock_download_item, GetTabReferrerUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/tab_referrer_url")));
#if BUILDFLAG(IS_ANDROID)
  ON_CALL(mock_download_item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(
          base::FilePath(FILE_PATH_LITERAL("content://media/123"))));
  EXPECT_CALL(mock_download_item, GetFileNameToReportUser())
      .WillOnce(
          Return(base::FilePath(FILE_PATH_LITERAL("target_file_name.exe"))));
#else
  EXPECT_CALL(mock_download_item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(
          base::FilePath(FILE_PATH_LITERAL("target_file_name.exe"))));
#endif
  EXPECT_CALL(mock_download_item, GetFullPath())
      .WillOnce(
          ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("full_path.exe"))));
  EXPECT_CALL(mock_download_item, GetURL())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/url")));
  EXPECT_CALL(mock_download_item, GetReferrerUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/referrer_url")));
  EXPECT_CALL(mock_download_item, GetHash())
      .WillOnce(ReturnRefOfCopy(std::string("hash")));
  EXPECT_CALL(mock_download_item, GetReceivedBytes()).WillOnce(Return(123));
  EXPECT_CALL(mock_download_item, HasUserGesture()).WillOnce(Return(true));
  EXPECT_CALL(mock_download_item, GetRemoteAddress())
      .WillRepeatedly(Return(std::string("remote_ip")));
  content::DownloadItemUtils::AttachInfoForTesting(&mock_download_item, nullptr,
                                                   nullptr);

  base::FilePath tmp_path(FILE_PATH_LITERAL("full_path.exe"));

  std::unique_ptr<DownloadRequestMaker> request_maker =
      DownloadRequestMaker::CreateFromDownloadItem(mock_feature_extractor_,
                                                   &mock_download_item);

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(*request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->url(), "https://example.com/url");
  EXPECT_EQ(request_->file_basename(), "target_file_name.exe");
  EXPECT_EQ(request_->digests().sha256(), "hash");
  EXPECT_EQ(request_->resources_size(), 3);
  EXPECT_EQ(request_->length(), 123);
  EXPECT_EQ(request_->user_initiated(), true);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, CreatesFromFileSystemAccess) {
  content::FileSystemAccessWriteItem item;
  item.target_file_path =
      base::FilePath(FILE_PATH_LITERAL("target_file_name.exe"));
  item.full_path = base::FilePath(FILE_PATH_LITERAL("full_path.exe"));
  item.sha256_hash = "sha256_hash";
  item.size = 123;
  item.frame_url = GURL("https://example.com/frame_url");
  item.has_user_gesture = true;

  base::FilePath tmp_path(FILE_PATH_LITERAL("full_path.exe"));

  std::unique_ptr<DownloadRequestMaker> request_maker =
      DownloadRequestMaker::CreateFromFileSystemAccess(mock_feature_extractor_,
                                                       item);

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(*request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->url(),
            "blob:https://example.com/file-system-access-write");
  EXPECT_EQ(request_->digests().sha256(), "sha256_hash");
  EXPECT_EQ(request_->resources_size(), 1);
  EXPECT_EQ(request_->length(), 123);
  EXPECT_EQ(request_->user_initiated(), true);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
}

TEST_F(DownloadRequestMakerTest, NotifiesCallback) {
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));

  bool callback_ran = false;

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr,
      /*password=*/std::nullopt, /*previous_token=*/"",
      base::BindLambdaForTesting([&callback_ran](const FileAnalyzer::Results&) {
        callback_ran = true;
      }));

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_TRUE(callback_ran);
}

// Archive file analysis is not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(DownloadRequestMakerTest, SetsIsEncrypted) {
  content::InProcessUtilityThreadHelper utility_thread_helper;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip =
      test_zip.AppendASCII("safe_browsing/download_protection/encrypted.zip");

  download::MockDownloadItem mock_download_item;
  EXPECT_CALL(mock_download_item, GetUrlChain())
      .WillRepeatedly(ReturnRefOfCopy(
          std::vector<GURL>{GURL("https://example.com/redirect"),
                            GURL("https://example.com/download")}));
  EXPECT_CALL(mock_download_item, GetTabUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/tab_url")));
  EXPECT_CALL(mock_download_item, GetTabReferrerUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/tab_referrer_url")));
  EXPECT_CALL(mock_download_item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(
          base::FilePath(FILE_PATH_LITERAL("target_file_name.zip"))));
  EXPECT_CALL(mock_download_item, GetFullPath())
      .WillOnce(ReturnRefOfCopy(test_zip));
  EXPECT_CALL(mock_download_item, GetURL())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/url")));
  EXPECT_CALL(mock_download_item, GetReferrerUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/referrer_url")));
  EXPECT_CALL(mock_download_item, GetHash())
      .WillOnce(ReturnRefOfCopy(std::string("hash")));
  EXPECT_CALL(mock_download_item, GetReceivedBytes()).WillOnce(Return(123));
  EXPECT_CALL(mock_download_item, HasUserGesture()).WillOnce(Return(true));
  EXPECT_CALL(mock_download_item, GetRemoteAddress())
      .WillRepeatedly(Return(std::string("remote_ip")));
  content::DownloadItemUtils::AttachInfoForTesting(&mock_download_item, nullptr,
                                                   nullptr);

  base::FilePath tmp_path(FILE_PATH_LITERAL("full_path.exe"));

  std::unique_ptr<DownloadRequestMaker> request_maker =
      DownloadRequestMaker::CreateFromDownloadItem(mock_feature_extractor_,
                                                   &mock_download_item);

  RunRequestMaker(*request_maker);

  ASSERT_TRUE(request_);
  EXPECT_TRUE(
      DownloadItemWarningData::IsTopLevelEncryptedArchive(&mock_download_item));
  EXPECT_EQ(details_.inspection_type, DownloadFileType::ZIP);
}

TEST_F(DownloadRequestMakerTest, UsesPassword) {
  content::InProcessUtilityThreadHelper utility_thread_helper;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip =
      test_zip.AppendASCII("safe_browsing/download_protection/encrypted.zip");

  download::MockDownloadItem mock_download_item;
  EXPECT_CALL(mock_download_item, GetUrlChain())
      .WillRepeatedly(ReturnRefOfCopy(
          std::vector<GURL>{GURL("https://example.com/redirect"),
                            GURL("https://example.com/download")}));
  EXPECT_CALL(mock_download_item, GetTabUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/tab_url")));
  EXPECT_CALL(mock_download_item, GetTabReferrerUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/tab_referrer_url")));
  EXPECT_CALL(mock_download_item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(
          base::FilePath(FILE_PATH_LITERAL("target_file_name.zip"))));
  EXPECT_CALL(mock_download_item, GetFullPath())
      .WillOnce(ReturnRefOfCopy(test_zip));
  EXPECT_CALL(mock_download_item, GetURL())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/url")));
  EXPECT_CALL(mock_download_item, GetReferrerUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/referrer_url")));
  EXPECT_CALL(mock_download_item, GetHash())
      .WillOnce(ReturnRefOfCopy(std::string("hash")));
  EXPECT_CALL(mock_download_item, GetReceivedBytes()).WillOnce(Return(123));
  EXPECT_CALL(mock_download_item, HasUserGesture()).WillOnce(Return(true));
  EXPECT_CALL(mock_download_item, GetRemoteAddress())
      .WillRepeatedly(Return(std::string("remote_ip")));
  content::DownloadItemUtils::AttachInfoForTesting(&mock_download_item, nullptr,
                                                   nullptr);

  base::FilePath tmp_path(FILE_PATH_LITERAL("full_path.exe"));

  std::unique_ptr<DownloadRequestMaker> request_maker =
      DownloadRequestMaker::CreateFromDownloadItem(
          mock_feature_extractor_, &mock_download_item,
          /*password=*/std::string("12345"));

  RunRequestMaker(*request_maker);

  ASSERT_TRUE(request_);
  ASSERT_EQ(request_->archived_binary_size(), 1);
  std::string sha256 = request_->archived_binary(0).digests().sha256();
  EXPECT_EQ(base::HexEncode(sha256),
            "E11FFA0C9F25234453A9EDD1CB251D46107F34B536AD74642A8584ACA8C1A8CE");
  EXPECT_EQ(details_.inspection_type, DownloadFileType::ZIP);
}

TEST_F(DownloadRequestMakerTest, SetsFullyExtractedArchive) {
  content::InProcessUtilityThreadHelper utility_thread_helper;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII(
      "safe_browsing/download_protection/aes_encrypted_password_12345.zip");

  download::MockDownloadItem mock_download_item;
  EXPECT_CALL(mock_download_item, GetUrlChain())
      .WillRepeatedly(ReturnRefOfCopy(
          std::vector<GURL>{GURL("https://example.com/redirect"),
                            GURL("https://example.com/download")}));
  EXPECT_CALL(mock_download_item, GetTabUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/tab_url")));
  EXPECT_CALL(mock_download_item, GetTabReferrerUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/tab_referrer_url")));
  EXPECT_CALL(mock_download_item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(
          base::FilePath(FILE_PATH_LITERAL("target_file_name.zip"))));
  EXPECT_CALL(mock_download_item, GetFullPath())
      .WillOnce(ReturnRefOfCopy(test_zip));
  EXPECT_CALL(mock_download_item, GetURL())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/url")));
  EXPECT_CALL(mock_download_item, GetReferrerUrl())
      .WillOnce(ReturnRefOfCopy(GURL("https://example.com/referrer_url")));
  EXPECT_CALL(mock_download_item, GetHash())
      .WillOnce(ReturnRefOfCopy(std::string("hash")));
  EXPECT_CALL(mock_download_item, GetReceivedBytes()).WillOnce(Return(123));
  EXPECT_CALL(mock_download_item, HasUserGesture()).WillOnce(Return(true));
  EXPECT_CALL(mock_download_item, GetRemoteAddress())
      .WillRepeatedly(Return(std::string("remote_ip")));
  content::DownloadItemUtils::AttachInfoForTesting(&mock_download_item, nullptr,
                                                   nullptr);

  base::FilePath tmp_path(FILE_PATH_LITERAL("full_path.exe"));

  std::unique_ptr<DownloadRequestMaker> request_maker =
      DownloadRequestMaker::CreateFromDownloadItem(mock_feature_extractor_,
                                                   &mock_download_item);

  RunRequestMaker(*request_maker);

  ASSERT_TRUE(request_);
  EXPECT_FALSE(
      DownloadItemWarningData::IsFullyExtractedArchive(&mock_download_item));
  EXPECT_EQ(details_.inspection_type, DownloadFileType::ZIP);
}
#endif

#if BUILDFLAG(IS_WIN)
TEST_F(DownloadRequestMakerTest, PopulateAntivirusProducts) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kAntivirusTelemetryForDownloads},
      /*disabled_features=*/{});

  constexpr char kAvProductName1[] = "Windows Defender";
  constexpr char kAvProductName2[] = "McAfee";
  constexpr uint32_t kAvProductNameHash1 = 123456;
  constexpr uint32_t kAvProductNameHash2 = 789012;
  constexpr char kAvProductVersion1[] = "1.1";
  constexpr char kAvProductVersion2[] = "2.1";
  constexpr uint32_t kAvProductVersionHash1 = 1122;
  constexpr uint32_t kAvProductVersionHash2 = 2233;

  std::vector<metrics::SystemProfileProto::AntiVirusProduct> av_products(2);
  av_products[0].set_product_name(kAvProductName1);
  av_products[0].set_product_name_hash(kAvProductNameHash1);
  av_products[0].set_product_version(kAvProductVersion1);
  av_products[0].set_product_version_hash(kAvProductVersionHash1);
  av_products[0].set_product_state(metrics::SystemProfileProto::STATE_ON);
  av_products[1].set_product_name(kAvProductName2);
  av_products[1].set_product_name_hash(kAvProductNameHash2);
  av_products[1].set_product_version(kAvProductVersion2);
  av_products[1].set_product_version_hash(kAvProductVersionHash2);
  av_products[1].set_product_state(metrics::SystemProfileProto::STATE_OFF);
  PopulateAntivirusProducts(std::move(av_products));

  SetSafeBrowsingState(profile_.GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->population().user_population(),
            ChromeUserPopulation::ENHANCED_PROTECTION);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
  EXPECT_EQ(request_->antivirus_products_size(), 2);

  const ClientDownloadRequest::AntiVirusProduct& actual_av1 =
      request_->antivirus_products(0);
  EXPECT_EQ(actual_av1.product_name(), "");
  EXPECT_EQ(actual_av1.product_name_hash(), kAvProductNameHash1);
  EXPECT_EQ(actual_av1.product_version(), "");
  EXPECT_EQ(actual_av1.product_version_hash(), kAvProductVersionHash1);
  EXPECT_EQ(actual_av1.product_state(),
            ClientDownloadRequest::AntiVirusProduct::STATE_ON);

  const ClientDownloadRequest::AntiVirusProduct& actual_av2 =
      request_->antivirus_products(1);
  EXPECT_EQ(actual_av2.product_name(), "");
  EXPECT_EQ(actual_av2.product_name_hash(), kAvProductNameHash2);
  EXPECT_EQ(actual_av2.product_version(), "");
  EXPECT_EQ(actual_av2.product_version_hash(), kAvProductVersionHash2);
  EXPECT_EQ(actual_av2.product_state(),
            ClientDownloadRequest::AntiVirusProduct::STATE_OFF);
}

TEST_F(DownloadRequestMakerTest, PopulateAntivirusProduct_FeatureDisabled) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kAntivirusTelemetryForDownloads});

  constexpr char kAvProductName1[] = "Windows Defender";
  constexpr char kAvProductName2[] = "McAfee";
  constexpr uint32_t kAvProductNameHash1 = 123456;
  constexpr uint32_t kAvProductNameHash2 = 789012;
  constexpr char kAvProductVersion1[] = "1.1";
  constexpr char kAvProductVersion2[] = "2.1";
  constexpr uint32_t kAvProductVersionHash1 = 1122;
  constexpr uint32_t kAvProductVersionHash2 = 2233;

  std::vector<metrics::SystemProfileProto::AntiVirusProduct> av_products(2);
  av_products[0].set_product_name(kAvProductName1);
  av_products[0].set_product_name_hash(kAvProductNameHash1);
  av_products[0].set_product_version(kAvProductVersion1);
  av_products[0].set_product_version_hash(kAvProductVersionHash1);
  av_products[0].set_product_state(metrics::SystemProfileProto::STATE_ON);
  av_products[1].set_product_name(kAvProductName2);
  av_products[1].set_product_name_hash(kAvProductNameHash2);
  av_products[1].set_product_version(kAvProductVersion2);
  av_products[1].set_product_version_hash(kAvProductVersionHash2);
  av_products[1].set_product_state(metrics::SystemProfileProto::STATE_OFF);
  PopulateAntivirusProducts(std::move(av_products));

  SetSafeBrowsingState(profile_.GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->population().user_population(),
            ChromeUserPopulation::ENHANCED_PROTECTION);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
  EXPECT_EQ(request_->antivirus_products_size(), 0);
}

TEST_F(DownloadRequestMakerTest, PopulateAntivirusProduct_StandardProtection) {
  base::RunLoop run_loop;
  base::FilePath tmp_path(FILE_PATH_LITERAL("temp_path"));
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{kAntivirusTelemetryForDownloads},
      /*disabled_features=*/{});

  constexpr char kAvProductName1[] = "Windows Defender";
  constexpr char kAvProductName2[] = "McAfee";
  constexpr uint32_t kAvProductNameHash1 = 123456;
  constexpr uint32_t kAvProductNameHash2 = 789012;
  constexpr char kAvProductVersion1[] = "1.1";
  constexpr char kAvProductVersion2[] = "2.1";
  constexpr uint32_t kAvProductVersionHash1 = 1122;
  constexpr uint32_t kAvProductVersionHash2 = 2233;

  std::vector<metrics::SystemProfileProto::AntiVirusProduct> av_products(2);
  av_products[0].set_product_name(kAvProductName1);
  av_products[0].set_product_name_hash(kAvProductNameHash1);
  av_products[0].set_product_version(kAvProductVersion1);
  av_products[0].set_product_version_hash(kAvProductVersionHash1);
  av_products[0].set_product_state(metrics::SystemProfileProto::STATE_ON);
  av_products[1].set_product_name(kAvProductName2);
  av_products[1].set_product_name_hash(kAvProductNameHash2);
  av_products[1].set_product_version(kAvProductVersion2);
  av_products[1].set_product_version_hash(kAvProductVersionHash2);
  av_products[1].set_product_state(metrics::SystemProfileProto::STATE_OFF);
  PopulateAntivirusProducts(std::move(av_products));

  SetSafeBrowsingState(profile_.GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);

  DownloadRequestMaker request_maker(
      mock_feature_extractor_, &profile_, DownloadRequestMaker::TabUrls(),
      /*target_file_name=*/base::FilePath(), tmp_path,
      /*source_url=*/GURL(),
      /*sha256_hash=*/"",
      /*length=*/0,
      /*resources=*/std::vector<ClientDownloadRequest::Resource>(),
      /*is_user_initiated=*/true,
      /*referrer_chain_data=*/nullptr, /*password=*/std::nullopt,
      /*previous_token=*/"", base::DoNothing());

  EXPECT_CALL(*mock_feature_extractor_, CheckSignature(tmp_path, _))
      .WillOnce(Return());
  EXPECT_CALL(*mock_feature_extractor_, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  RunRequestMaker(request_maker);

  ASSERT_TRUE(request_);
  EXPECT_EQ(request_->population().user_population(),
            ChromeUserPopulation::SAFE_BROWSING);
  EXPECT_EQ(details_.inspection_type, DownloadFileType::NONE);
  EXPECT_EQ(request_->antivirus_products_size(), 0);
}
#endif

}  // namespace safe_browsing
