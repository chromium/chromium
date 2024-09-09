// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/paste_allowed_request.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate_base.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"

namespace enterprise_data_protection {

namespace {

enterprise_connectors::ContentAnalysisDelegate* test_delegate_ = nullptr;

constexpr char kScanId[] = "scan_id";

enterprise_connectors::ContentAnalysisResponse::Result CreateResult(
    enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        Action action) {
  enterprise_connectors::ContentAnalysisResponse::Result result;
  result.set_tag("dlp");
  result.set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

  if (action != enterprise_connectors::ContentAnalysisResponse::Result::
                    TriggeredRule::ACTION_UNSPECIFIED) {
    auto* rule = result.add_triggered_rules();
    rule->set_rule_name("paste_rule_name");
    rule->set_action(action);
  }
  return result;
}

enterprise_connectors::ContentAnalysisResponse CreateResponse(
    enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
        Action action) {
  enterprise_connectors::ContentAnalysisResponse response;
  response.set_request_token(kScanId);

  auto* result = response.add_results();
  *result = CreateResult(action);
  return response;
}

class PasteTestContentAnalysisDelegate
    : public enterprise_connectors::ContentAnalysisDelegate {
 public:
  PasteTestContentAnalysisDelegate(
      enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
          Action action,
      content::WebContents* contents,
      ContentAnalysisDelegate::Data data,
      ContentAnalysisDelegate::CompletionCallback callback)
      : ContentAnalysisDelegate(contents,
                                std::move(data),
                                std::move(callback),
                                safe_browsing::DeepScanAccessPoint::PASTE),
        action_(action) {}

  static std::unique_ptr<enterprise_connectors::ContentAnalysisDelegate> Create(
      enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
          Action action,
      content::WebContents* contents,
      ContentAnalysisDelegate::Data data,
      ContentAnalysisDelegate::CompletionCallback callback) {
    auto delegate = std::make_unique<PasteTestContentAnalysisDelegate>(
        action, contents, std::move(data), std::move(callback));
    test_delegate_ = delegate.get();
    return delegate;
  }

 private:
  void UploadTextForDeepScanning(
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request)
      override {
    StringRequestCallback(safe_browsing::BinaryUploadService::Result::SUCCESS,
                          CreateResponse(action_));
  }

  enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::Action
      action_;
};

class PasteAllowedRequestTest : public testing::Test {
 public:
  PasteAllowedRequestTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user-1");
    scoped_features_.InitAndEnableFeature(
        data_controls::kEnableDesktopDataControls);
  }

  void SetUp() override {
    PasteAllowedRequest::CleanupRequestsForTesting();
    ui::TestClipboard::CreateForCurrentThread();
  }

  content::WebContents* main_web_contents() {
    if (!main_web_contents_) {
      content::WebContents::CreateParams params(profile_);
      main_web_contents_ = content::WebContents::Create(params);
    }
    return main_web_contents_.get();
  }

  content::RenderFrameHost& main_rfh() {
    return *main_web_contents()->GetPrimaryMainFrame();
  }

  content::ClipboardEndpoint main_endpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(GURL("https://google.com")),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(profile_);
        }),
        main_rfh());
  }

  content::WebContents* secondary_web_contents() {
    if (!secondary_web_contents_) {
      content::WebContents::CreateParams params(profile_);
      secondary_web_contents_ = content::WebContents::Create(params);
    }
    return secondary_web_contents_.get();
  }

  content::RenderFrameHost& secondary_rfh() {
    return *secondary_web_contents()->GetPrimaryMainFrame();
  }

  content::ClipboardEndpoint secondary_endpoint() {
    return content::ClipboardEndpoint(
        ui::DataTransferEndpoint(GURL("https://google.com")),
        base::BindLambdaForTesting([this]() -> content::BrowserContext* {
          return static_cast<content::BrowserContext*>(profile_);
        }),
        secondary_rfh());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_features_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> main_web_contents_;
  std::unique_ptr<content::WebContents> secondary_web_contents_;
};

class PasteAllowedRequestScanningTest : public PasteAllowedRequestTest {
 public:
  PasteAllowedRequestScanningTest() {
    enterprise_connectors::ContentAnalysisDelegate::DisableUIForTesting();
  }

  void SetUp() override {
    PasteAllowedRequestTest::SetUp();

    helper_ = std::make_unique<
        enterprise_connectors::test::EventReportValidatorHelper>(profile_);

    enterprise_connectors::test::SetAnalysisConnector(
        profile_->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
        R"({
          "service_provider": "google",
          "block_until_verdict": 1,
          "minimum_data_size": 1,
          "enable": [
            {
              "url_list": ["*"],
              "tags": ["dlp"]
            }
          ]
        })");
  }

 protected:
  std::unique_ptr<enterprise_connectors::test::EventReportValidatorHelper>
      helper_;
};

}  // namespace

TEST_F(PasteAllowedRequestTest, AddCallbacksAndComplete) {
  PasteAllowedRequest request;
  content::ClipboardPasteData clipboard_paste_data_1;
  clipboard_paste_data_1.text = u"text";
  clipboard_paste_data_1.png = {1, 2, 3, 4, 5};
  content::ClipboardPasteData clipboard_paste_data_2;
  clipboard_paste_data_2.text = u"other text";
  clipboard_paste_data_2.png = {6, 7, 8, 9, 10};

  int count = 0;

  // Add a callback.  It should not fire right away.
  request.AddCallback(base::BindLambdaForTesting(
      [&count, clipboard_paste_data_1](
          std::optional<content::ClipboardPasteData> clipboard_paste_data) {
        ++count;
        ASSERT_EQ(clipboard_paste_data->text, clipboard_paste_data_1.text);
        ASSERT_EQ(clipboard_paste_data->png, clipboard_paste_data_1.png);
      }));
  EXPECT_EQ(0, count);

  // Complete the request.  Callback should fire.  Whether paste is allowed
  // or not is not important.
  request.Complete(clipboard_paste_data_1);
  EXPECT_EQ(1, count);

  // Add a second callback.  It should not fire right away.
  request.AddCallback(base::BindLambdaForTesting(
      [&count, clipboard_paste_data_2](
          std::optional<content::ClipboardPasteData> clipboard_paste_data) {
        ++count;
        ASSERT_EQ(clipboard_paste_data->text, clipboard_paste_data_2.text);
        ASSERT_EQ(clipboard_paste_data->png, clipboard_paste_data_2.png);
      }));
  EXPECT_EQ(1, count);

  // Calling `Complete()` again will call the second callback.
  request.Complete(clipboard_paste_data_2);
  EXPECT_EQ(2, count);
}

TEST_F(PasteAllowedRequestTest, IsObsolete) {
  PasteAllowedRequest request;
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = u"data";

  // A request is obsolete once it is too old and completed.
  // Whether paste is allowed or not is not important.
  request.Complete(clipboard_paste_data);
  EXPECT_TRUE(
      request.IsObsolete(request.completed_time() +
                         PasteAllowedRequest::kIsPasteAllowedRequestTooOld +
                         base::Microseconds(1)));
}

TEST_F(PasteAllowedRequestTest, SameDestinationSource) {
  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  main_rfh().MarkClipboardOwner(seqno);

  const std::u16string kText = u"text";
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = kText;

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ main_endpoint(), /*destination*/ main_endpoint(),
      {.seqno = seqno}, clipboard_paste_data, future.GetCallback());

  ASSERT_TRUE(future.Get());
  ASSERT_EQ(future.Get()->text, kText);

  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestTest, SameDestinationSource_AfterReplacement) {
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "sources": {
                      "urls": ["google.com"]
                    },
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  const std::u16string kText = u"text";
  content::ClipboardPasteData copied_data;
  copied_data.text = kText;
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;

  IsClipboardCopyAllowedByPolicy(main_endpoint(), {}, copied_data,
                                 copy_future.GetCallback());
  auto replacement = copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(replacement);
  EXPECT_EQ(*replacement,
            u"Pasting this content here is blocked by your administrator.");

  // This triggers the clipboard observer started by the
  // `IsClipboardCopyAllowedByPolicy` calls so that they're aware of the new
  // seqno.
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  main_rfh().MarkClipboardOwner(seqno);

  // After the data was replaced when initially copied, it should be put back
  // when pasting in the same tab.
  content::ClipboardPasteData pasted_data;
  pasted_data.html = u"to be replaced";
  base::test::TestFuture<std::optional<content::ClipboardPasteData>>
      paste_future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ main_endpoint(), /*destination*/ main_endpoint(),
      {.seqno = seqno}, pasted_data, paste_future.GetCallback());

  ASSERT_TRUE(paste_future.Get());
  ASSERT_EQ(paste_future.Get()->text, kText);
  ASSERT_TRUE(paste_future.Get()->html.empty());

  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestTest, DifferentDestinationSource) {
  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  secondary_rfh().MarkClipboardOwner(seqno);

  const std::u16string kText = u"text";
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = kText;

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ secondary_endpoint(), /*destination*/ main_endpoint(),
      {.seqno = seqno}, clipboard_paste_data, future.GetCallback());

  ASSERT_TRUE(future.Get());
  ASSERT_EQ(future.Get()->text, kText);

  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestTest,
       DifferentDestinationSource_AllowedWithCachedRequest) {
  const std::u16string kText = u"text";
  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  secondary_rfh().MarkClipboardOwner(seqno);

  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = kText;
  PasteAllowedRequest request;
  request.Complete(clipboard_paste_data);
  PasteAllowedRequest::AddRequestToCacheForTesting(main_rfh().GetGlobalId(),
                                                   seqno, std::move(request));
  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());

  // Attempting to start a new request when there is an allowed cached request
  // with the same seqno should allow again, but not by making a new request.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ secondary_endpoint(), /*destination*/ main_endpoint(),
      {.seqno = seqno}, clipboard_paste_data, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get());
  ASSERT_EQ(future.Get()->text, kText);

  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestTest,
       DifferentDestinationSource_BlockedWithCachedRequest) {
  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  secondary_rfh().MarkClipboardOwner(seqno);

  PasteAllowedRequest request;
  request.Complete(std::nullopt);
  PasteAllowedRequest::AddRequestToCacheForTesting(main_rfh().GetGlobalId(),
                                                   seqno, std::move(request));
  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());

  const std::u16string kText = u"text";
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = kText;

  // Attempting to start a new request when there is a blocked cached request
  // with the same seqno should block again, but not by making a new request.
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ secondary_endpoint(), /*destination*/ main_endpoint(),
      {.seqno = seqno}, clipboard_paste_data, future.GetCallback());

  ASSERT_TRUE(future.Wait());
  ASSERT_FALSE(future.Get());

  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestTest, UnknownSource) {
  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  secondary_rfh().MarkClipboardOwner(seqno);

  const std::u16string kText = u"text";
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = kText;

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ content::ClipboardEndpoint(std::nullopt),
      /*destination*/ main_endpoint(), {.seqno = seqno}, clipboard_paste_data,
      future.GetCallback());

  ASSERT_TRUE(future.Get());
  ASSERT_EQ(future.Get()->text, kText);

  // When a document reads from the clipboard, but the clipboard was written
  // from an unknown source, content checks should not be skipped.
  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestTest, EmptyData) {
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ secondary_endpoint(),
      /*destination*/ main_endpoint(),
      {
          .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
              ui::ClipboardBuffer::kCopyPaste),
      },
      content::ClipboardPasteData(), future.GetCallback());

  ASSERT_TRUE(future.Get());
  ASSERT_TRUE(future.Get()->empty());

  // When data is empty, a request is still made in case the data needs to be
  // replaced later.
  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestTest, EmptyData_SameSourceReplaced) {
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  const std::u16string kText = u"text";
  content::ClipboardPasteData copied_data;
  copied_data.text = kText;
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;

  IsClipboardCopyAllowedByPolicy(main_endpoint(), {}, copied_data,
                                 copy_future.GetCallback());
  auto replacement = copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(replacement);
  EXPECT_EQ(*replacement,
            u"Pasting this content here is blocked by your administrator.");

  // This triggers the clipboard observer started by the
  // `IsClipboardCopyAllowedByPolicy` calls so that they're aware of the new
  // seqno.
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  main_rfh().MarkClipboardOwner(seqno);
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ secondary_endpoint(),
      /*destination*/ main_endpoint(),
      {
          .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
              ui::ClipboardBuffer::kCopyPaste),
      },
      content::ClipboardPasteData(), future.GetCallback());

  ASSERT_TRUE(future.Get());
  ASSERT_EQ(future.Get()->text, kText);

  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestTest, EmptyData_DifferentSourceReplaced) {
  data_controls::SetDataControls(profile_->GetPrefs(), {
                                                           R"({
                    "destinations": {
                      "os_clipboard": true
                    },
                    "restrictions": [
                      {"class": "CLIPBOARD", "level": "BLOCK"}
                    ]
                  })"});

  const std::u16string kText = u"text";
  content::ClipboardPasteData copied_data;
  copied_data.text = kText;
  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      copy_future;

  IsClipboardCopyAllowedByPolicy(main_endpoint(), {}, copied_data,
                                 copy_future.GetCallback());
  auto replacement = copy_future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(replacement);
  EXPECT_EQ(*replacement,
            u"Pasting this content here is blocked by your administrator.");

  // This triggers the clipboard observer started by the
  // `IsClipboardCopyAllowedByPolicy` calls so that they're aware of the new
  // seqno.
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  main_rfh().MarkClipboardOwner(seqno);
  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ main_endpoint(),
      /*destination*/ secondary_endpoint(),
      {
          .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
              ui::ClipboardBuffer::kCopyPaste),
      },
      content::ClipboardPasteData(), future.GetCallback());

  ASSERT_TRUE(future.Get());
  ASSERT_EQ(future.Get()->text, kText);

  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestTest, CleanupObsoleteScanRequests) {
  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  secondary_rfh().MarkClipboardOwner(seqno);

  const std::u16string kText = u"text";
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = kText;

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ secondary_endpoint(), /*destination*/ main_endpoint(),
      {.seqno = seqno}, clipboard_paste_data, future.GetCallback());

  ASSERT_TRUE(future.Get());
  ASSERT_EQ(future.Get()->text, kText);
  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());

  // Make sure an appropriate amount of time passes to make the request old.
  // It should be cleaned up.
  task_environment_.FastForwardBy(
      PasteAllowedRequest::kIsPasteAllowedRequestTooOld +
      base::Microseconds(1));
  PasteAllowedRequest::CleanupObsoleteRequests();
  EXPECT_EQ(0u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestScanningTest, SameDestinationSource) {
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&PasteTestContentAnalysisDelegate::Create,
                          enterprise_connectors::ContentAnalysisResponse::
                              Result::TriggeredRule::BLOCK));

  auto validator = helper_->CreateValidator();
  validator.ExpectNoReport();

  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  main_rfh().MarkClipboardOwner(seqno);

  const std::u16string kText = u"text";
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = kText;

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ main_endpoint(), /*destination*/ main_endpoint(),
      {.seqno = seqno}, clipboard_paste_data, future.GetCallback());

  ASSERT_TRUE(future.Get());
  ASSERT_EQ(future.Get()->text, kText);

  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

TEST_F(PasteAllowedRequestScanningTest, DifferentDestinationSource) {
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&PasteTestContentAnalysisDelegate::Create,
                          enterprise_connectors::ContentAnalysisResponse::
                              Result::TriggeredRule::BLOCK));

  auto validator = helper_->CreateValidator();
  validator.ExpectSensitiveDataEvent(
      /*url*/
      "",
      /*tab_url*/ "",
      /*source*/ "https://google.com/",
      /*destination*/ "",
      /*filename*/ "Text data",
      /*sha*/ "",
      /*trigger*/ "WEB_CONTENT_UPLOAD",
      /*dlp_verdict*/
      CreateResult(enterprise_connectors::ContentAnalysisResponse::Result::
                       TriggeredRule::BLOCK),
      /*mimetype*/
      []() {
        static std::set<std::string> set = {"text/plain"};
        return &set;
      }(),
      /*size*/ 4,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ "test-user@chromium.org",
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ kScanId,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  auto seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
      ui::ClipboardBuffer::kCopyPaste);
  secondary_rfh().MarkClipboardOwner(seqno);

  const std::u16string kText = u"text";
  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = kText;

  base::test::TestFuture<std::optional<content::ClipboardPasteData>> future;
  PasteAllowedRequest::StartPasteAllowedRequest(
      /*source*/ secondary_endpoint(), /*destination*/ main_endpoint(),
      {.seqno = seqno}, clipboard_paste_data, future.GetCallback());

  ASSERT_TRUE(future.Get());
  ASSERT_EQ(future.Get()->text, kText);

  EXPECT_EQ(1u, PasteAllowedRequest::requests_count_for_testing());
}

}  // namespace enterprise_data_protection
