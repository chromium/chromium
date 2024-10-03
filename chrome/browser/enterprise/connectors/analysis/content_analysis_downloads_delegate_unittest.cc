// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_downloads_delegate.h"

#include <gtest/gtest.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "components/download/public/common/mock_download_item.h"

namespace enterprise_connectors {

namespace {

constexpr char kTestUrl[] = "http://example.com/";
constexpr char kTestUrl2[] = "http://google.com/";
constexpr char kTestInvalidUrl[] = "example.com";
constexpr char16_t kTestMessage[] = u"Message";
constexpr char16_t kTestMessage2[] = u"Rule message";
constexpr char16_t kTestFile[] = u"foo.txt";

}  // namespace

class ContentAnalysisDownloadsDelegateTest : public testing::Test {
 public:
  void OpenCallback() {
    ++times_open_called_;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void DiscardCallback() { ++times_discard_called_; }

  int times_open_called_ = 0;
  int times_discard_called_ = 0;
  download::MockDownloadItem mock_download_item;
  base::OnceClosure quit_closure_;
};

TEST_F(ContentAnalysisDownloadsDelegateTest, TestOpenFile) {
  ContentAnalysisDownloadsDelegate delegate(
      u"", u"", GURL(), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      &mock_download_item, CreateSampleCustomRuleMessage(u"", ""));

  delegate.BypassWarnings(u"User's justification");
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  // Attempting any action after one has been performed is a no-op.
  delegate.BypassWarnings(u"User's justification");
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  delegate.Cancel(true);
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  delegate.Cancel(false);
  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestDiscardFileWarning) {
  ContentAnalysisDownloadsDelegate delegate(
      u"", u"", GURL(), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      &mock_download_item, CreateSampleCustomRuleMessage(u"", ""));

  delegate.Cancel(true);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  // Attempting any action after one has been performed is a no-op.
  delegate.Cancel(true);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  delegate.Cancel(false);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  delegate.BypassWarnings(std::nullopt);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestDiscardFileBlock) {
  ContentAnalysisDownloadsDelegate delegate(
      u"", u"", GURL(), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      &mock_download_item, CreateSampleCustomRuleMessage(u"", ""));

  delegate.Cancel(false);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  // Attempting any action after one has been performed is a no-op.
  delegate.Cancel(true);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  delegate.Cancel(false);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);

  delegate.BypassWarnings(std::nullopt);
  EXPECT_EQ(0, times_open_called_);
  EXPECT_EQ(1, times_discard_called_);
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestNoMessageOrUrlReturnsNullOpt) {
  ContentAnalysisDownloadsDelegate delegate(
      u"", u"", GURL(), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      &mock_download_item, CreateSampleCustomRuleMessage(u"", ""));

  EXPECT_FALSE(delegate.GetCustomMessage());
  EXPECT_FALSE(delegate.GetCustomLearnMoreUrl());
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestGetMessageAndUrl) {
  ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
      empty_custom_rule_msg;
  ContentAnalysisDownloadsDelegate delegate(
      kTestFile, kTestMessage, GURL(kTestUrl), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      nullptr, empty_custom_rule_msg);

  EXPECT_TRUE(delegate.GetCustomMessage());
  EXPECT_TRUE(delegate.GetCustomLearnMoreUrl());

  EXPECT_EQ(base::StrCat({kTestFile,
                          u" has sensitive or dangerous data. Your "
                          u"administrator says: \"",
                          kTestMessage, u"\""}),
            *(delegate.GetCustomMessage()));
  EXPECT_EQ(GURL(kTestUrl), *(delegate.GetCustomLearnMoreUrl()));
}

TEST_F(ContentAnalysisDownloadsDelegateTest,
       TestCustomRuleMessageAndCustomMessage) {
  ContentAnalysisDownloadsDelegate delegate(
      kTestFile, kTestMessage, GURL(kTestUrl), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      nullptr, CreateSampleCustomRuleMessage(kTestMessage2, kTestUrl2));

  EXPECT_TRUE(delegate.GetCustomMessage());
  EXPECT_FALSE(delegate.GetCustomLearnMoreUrl());
  EXPECT_TRUE(delegate.GetCustomRuleMessageRanges());

  EXPECT_EQ(base::StrCat({kTestFile,
                          u" has sensitive or dangerous data. Your "
                          u"administrator says: \"",
                          kTestMessage2, u"\""}),
            *(delegate.GetCustomMessage()));
}

TEST_F(ContentAnalysisDownloadsDelegateTest,
       TestCustomRuleMessageAndCustomMessageInvalidUrl) {
  ContentAnalysisDownloadsDelegate delegate(
      u"foo.txt", kTestMessage, GURL(kTestUrl), true,
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                     base::Unretained(this)),
      base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                     base::Unretained(this)),
      nullptr, CreateSampleCustomRuleMessage(kTestMessage2, kTestInvalidUrl));

  EXPECT_TRUE(delegate.GetCustomMessage());
  EXPECT_FALSE(delegate.GetCustomLearnMoreUrl());
  EXPECT_FALSE(delegate.GetCustomRuleMessageRanges());

  EXPECT_EQ(base::StrCat({kTestFile,
                          u" has sensitive or dangerous data. Your "
                          u"administrator says: \"",
                          kTestMessage2, u"\""}),
            *(delegate.GetCustomMessage()));
}

TEST_F(ContentAnalysisDownloadsDelegateTest, TestDeobfuscationOnBypass) {
  base::test::ScopedFeatureList enable_feature(
      enterprise_obfuscation::kEnterpriseFileObfuscation);

  // Setup obfuscated file with dummy data.
  // TODO(b/368630027): Refactor common dummy obfuscated data creation.
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::vector<uint8_t> original_contents(5000, 'a');
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("obfuscated");

  enterprise_obfuscation::DownloadObfuscator obfuscator;
  auto obfuscation_result =
      obfuscator.ObfuscateChunk(base::span(original_contents), true);

  ASSERT_TRUE(obfuscation_result.has_value());
  ASSERT_TRUE(base::WriteFile(file_path, obfuscation_result.value()));

  // Setup mock download item.
  EXPECT_CALL(mock_download_item, GetFullPath())
      .WillRepeatedly(testing::ReturnRefOfCopy(file_path));

  auto obfuscation_data =
      std::make_unique<enterprise_obfuscation::DownloadObfuscationData>(true);
  mock_download_item.SetUserData(
      enterprise_obfuscation::DownloadObfuscationData::kUserDataKey,
      std::move(obfuscation_data));

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  // Simulate scenario where the delegate is destroyed immediately after
  // bypassing warnings.
  {
    ContentAnalysisDownloadsDelegate delegate(
        kTestFile, u"", GURL(), true,
        base::BindOnce(&ContentAnalysisDownloadsDelegateTest::OpenCallback,
                       base::Unretained(this)),
        base::BindOnce(&ContentAnalysisDownloadsDelegateTest::DiscardCallback,
                       base::Unretained(this)),
        &mock_download_item, CreateSampleCustomRuleMessage(u"", ""));

    // Bypass warnings should trigger deobfuscation and open callback.
    delegate.BypassWarnings(u"User's justification");
  }

  run_loop.Run();

  EXPECT_EQ(1, times_open_called_);
  EXPECT_EQ(0, times_discard_called_);

  // Verify that the file has been deobfuscated correctly.
  std::string deobfuscated_content;
  ASSERT_TRUE(base::ReadFileToString(file_path, &deobfuscated_content));
  EXPECT_EQ(original_contents,
            std::vector<uint8_t>(deobfuscated_content.begin(),
                                 deobfuscated_content.end()));
}

}  // namespace enterprise_connectors
