// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"
#include "chrome/common/media/webrtc_logging.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/renderer/media/webrtc_logging_agent_impl.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

// Other platforms have no-op implementations of WebRTC Diagnostic Logging.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
namespace {

const char kTestUploadUrl[] = "https://upload.com/webrtc_upload";
const char kTestReportId[] = "test_report_id";

bool IsValidUuid(const std::string& uuid) {
  return base::Uuid::ParseCaseInsensitive(uuid).is_valid();
}

class RTCDiagnosticLoggingTest : public ChromeRenderViewHostTestHarness {
 public:
  enum class StopAction { kFinish, kCancel, kNothing };

  RTCDiagnosticLoggingTest()
      : test_shared_url_loader_factory_(
            test_url_loader_factory_.GetSafeWeakWrapper()) {}

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);
#endif
    ChromeRenderViewHostTestHarness::SetUp();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_url_loader_factory_);
    auto uploader = std::make_unique<WebRtcLogUploader>();
    TestingBrowserProcess::GetGlobal()->SetWebRtcLogUploader(
        std::move(uploader));
    webrtc_log_uploader()->SetUploadUrlForTesting(GURL(kTestUploadUrl));
    process()->OverrideBinderForTesting(
        chrome::mojom::WebRtcLoggingAgent::Name_,
        base::BindLambdaForTesting([&](mojo::ScopedMessagePipeHandle handle) {
          agent_->AddReceiver(
              mojo::PendingReceiver<chrome::mojom::WebRtcLoggingAgent>(
                  std::move(handle)));
        }));
    WebRtcLoggingController::AttachToRenderProcessHost(
        main_rfh()->GetProcess());

    profile()->GetPrefs()->SetBoolean(prefs::kWebRtcTextLogCollectionAllowed,
                                      true);
    base::ListValue allowed_origins;
    allowed_origins.Append("https://example.com");
    allowed_origins.Append("https://example.upload.com");
    profile()->GetPrefs()->SetList(
        prefs::kWebRTCDiagnosticLogCollectionAllowedForOrigins,
        std::move(allowed_origins));
  }

  void TearDown() override {
    agent_.reset();
    task_environment()->RunUntilIdle();
    if (auto* uploader =
            TestingBrowserProcess::GetGlobal()->webrtc_log_uploader()) {
      base::RunLoop run_loop;
      uploader->background_task_runner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
      run_loop.Run();
      uploader->Shutdown();
    }
    TestingBrowserProcess::GetGlobal()->SetWebRtcLogUploader(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
#if BUILDFLAG(IS_CHROMEOS)
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
#endif
  }

  WebRtcLogUploader* webrtc_log_uploader() {
    return TestingBrowserProcess::GetGlobal()->webrtc_log_uploader();
  }

  WebRtcLoggingController* GetControllerForProcess(
      content::RenderProcessHost* rph) {
    return WebRtcLoggingController::FromRenderProcessHost(rph);
  }

  WebRtcLoggingController* controller() {
    return GetControllerForProcess(main_rfh()->GetProcess());
  }

  std::tuple<std::string, std::string, std::string> StartAndStopLogging(
      bool upload,
      StopAction stop_action,
      const base::flat_map<std::string, std::string>& metadata,
      const GURL& url = GURL("https://example.com")) {
    NavigateAndCommit(url);
    content::RenderProcessHost* rph = main_rfh()->GetProcess();

    // Start logging first.
    base::test::TestFuture<const std::string&> future;
    content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
        *main_rfh(), upload, metadata, future.GetCallback());
    std::string uuid = future.Get();
    EXPECT_TRUE(IsValidUuid(uuid));

    // Wait for it to transition to STARTED.
    task_environment()->RunUntilIdle();

    // Add a message if logging started.
    auto* controller = GetControllerForProcess(rph);
    if (controller && controller->web_api_settings().has_value()) {
      std::vector<chrome::mojom::WebRtcLoggingMessagePtr> messages;
      messages.push_back(chrome::mojom::WebRtcLoggingMessage::New(
          base::Time::Now(), "test message in StartAndStopLogging"));
      controller->OnAddMessages(std::move(messages));
    }

    if (stop_action == StopAction::kFinish) {
      base::test::TestFuture<void> stop_future;
      content::GetContentClientForTesting()
          ->browser()
          ->FinishRtcDiagnosticLogging(*main_rfh(), stop_future.GetCallback());
      EXPECT_TRUE(stop_future.Wait());
    } else if (stop_action == StopAction::kCancel) {
      base::test::TestFuture<void> cancel_future;
      content::GetContentClientForTesting()
          ->browser()
          ->CancelRtcDiagnosticLogging(*main_rfh(),
                                       cancel_future.GetCallback());
      EXPECT_TRUE(cancel_future.Wait());
    }

    // Wait for subsequent internal controller state changes.
    task_environment()->RunUntilIdle();

    // Simulate process closure to trigger the upload or storage.
    agent_.reset();
    task_environment()->RunUntilIdle();

    // WebRtcLogUploader now has background tasks to write the log and prepare
    // the upload. Wait for those background tasks.
    if (auto* uploader = webrtc_log_uploader()) {
      base::test::TestFuture<void> uploader_future;
      uploader->background_task_runner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), uploader_future.GetCallback());
      EXPECT_TRUE(uploader_future.Wait());
    }

    std::string upload_data;
    if (upload && stop_action != StopAction::kCancel) {
      // Now the request should be in the TestURLLoaderFactory.
      const GURL upload_url(kTestUploadUrl);
      if (test_url_loader_factory_.total_requests() == 1u) {
        auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
        if (pending_request) {
          EXPECT_EQ(pending_request->request.url, GURL(upload_url));

          // Verify uploaded data.
          upload_data = network::GetUploadData(pending_request->request);
          EXPECT_FALSE(upload_data.empty());

          // Simulate response.
          test_url_loader_factory_.SimulateResponseForPendingRequest(
              upload_url.spec(), kTestReportId);

          // Wait for background task to update the log list.
          if (auto* uploader = webrtc_log_uploader()) {
            base::test::TestFuture<void> uploader_future;
            uploader->background_task_runner()->PostTaskAndReply(
                FROM_HERE, base::DoNothing(), uploader_future.GetCallback());
            EXPECT_TRUE(uploader_future.Wait());
          }
        }
      }
    }

    // Check if log and list files were created.
    base::FilePath log_dir =
        webrtc_logging::TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
            profile()->GetPath(), webrtc_logging::ApiType::kWeb);
    base::FilePath log_list_path =
        webrtc_logging::TextLogList::GetWebRtcLogListFileForDirectory(log_dir);

    std::string uncompressed_log;
    base::FileEnumerator enumerator(log_dir, /*recursive=*/false,
                                    base::FileEnumerator::FILES,
                                    FILE_PATH_LITERAL("*.gz"));
    base::FilePath log_file = enumerator.Next();
    bool log_file_exists = !log_file.empty();

    if (stop_action == StopAction::kCancel) {
      EXPECT_FALSE(log_file_exists);
      EXPECT_FALSE(base::PathExists(log_list_path));
    } else {
      // If upload=false and stop_action=kNothing, it might not be stored.
      if (log_file_exists) {
        std::string compressed;
        if (base::ReadFileToString(log_file, &compressed)) {
          compression::GzipUncompress(compressed, &uncompressed_log);
        }

        EXPECT_TRUE(base::PathExists(log_list_path));
        std::string log_list_content;
        EXPECT_TRUE(base::ReadFileToString(log_list_path, &log_list_content));
        if (upload) {
          EXPECT_THAT(log_list_content, testing::HasSubstr(kTestReportId));
        } else {
          EXPECT_THAT(log_list_content,
                      testing::Not(testing::HasSubstr(kTestReportId)));
        }
      }
    }

    // Clean up the log directory to ensure test isolation.
    base::DeletePathRecursively(log_dir);

    return {uuid, upload_data, uncompressed_log};
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  ash::system::FakeStatisticsProvider fake_statistics_provider_;
#endif
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<chrome::WebRtcLoggingAgentImpl> agent_ =
      std::make_unique<chrome::WebRtcLoggingAgentImpl>();
};

TEST_F(RTCDiagnosticLoggingTest, StartFinishAndUploadDiagnosticLogging) {
  base::flat_map<std::string, std::string> metadata;
  constexpr char test_key[] = "test_key";
  constexpr char test_value[] = "test_value";
  metadata[test_key] = test_value;
  auto [uuid, uploaded, uncompressed_log] = StartAndStopLogging(
      /*upload=*/true, StopAction::kFinish, metadata);

  EXPECT_THAT(uncompressed_log,
              testing::HasSubstr("test message in StartAndStopLogging"));

  // Example of uploaded data:
  //
  // ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
  // Content-Disposition: form-data; name="prod"
  //
  // Chrome_Linux_webrtc
  // ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
  // Content-Disposition: form-data; name="ver"
  //
  // 147.0.7684.0
  // ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
  // Content-Disposition: form-data; name="guid"
  //
  // 0
  // ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
  // Content-Disposition: form-data; name="type"
  //
  // webrtc_log
  // ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
  // Content-Disposition: form-data; name="__uuid__"
  //
  // ce5397bd-7e92-4ecb-b78d-476bca84d0b9
  // ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
  // Content-Disposition: form-data; name="test_key"
  //
  // test_value
  // ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
  // Content-Disposition: form-data; name="other_webrtc_log";
  // filename="other_webrtc_log.gz" Content-Type: application/gzip
  //
  // <gzipped_content>
  // ------**--yradnuoBgoLtrapitluMklaTelgooG--**----

  constexpr char boundary[] =
      "------**--yradnuoBgoLtrapitluMklaTelgooG--**----";
  constexpr char gzip_content_type[] = "Content-Type: application/gzip";

  size_t gzipped_section_index = uploaded.find(gzip_content_type);
  ASSERT_NE(gzipped_section_index, std::string::npos);
  // Gzipped content starts after "Content-Type: application/gzip\r\n\r\n", -1
  // to remove \0 and +4 for \r\n\r\n
  size_t gzipped_content_start_index =
      gzipped_section_index + sizeof(gzip_content_type) + 3;

  size_t gzipped_section_end_index =
      uploaded.find(boundary, gzipped_content_start_index);
  ASSERT_NE(gzipped_section_end_index, std::string::npos);
  // -2 to remove \r\n before the boundary
  size_t gzipped_length =
      gzipped_section_end_index - gzipped_content_start_index - 2;

  std::string uncompressed;
  ASSERT_TRUE(compression::GzipUncompress(
      uploaded.substr(gzipped_content_start_index, gzipped_length),
      &uncompressed));
  EXPECT_THAT(uncompressed, testing::HasSubstr(base::StringPrintf(
                                "__uuid__: %s", uuid.c_str())));
  EXPECT_THAT(uncompressed, testing::HasSubstr(base::StringPrintf(
                                "%s: %s", test_key, test_value)));

  // Remove the binary gzipped section from `uploaded` and check the rest of the
  // multipart text-only data.
  uploaded.erase(gzipped_section_index);
  std::vector<std::string> multipart_data = base::SplitStringUsingSubstr(
      uploaded, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  ASSERT_EQ(multipart_data.size(), 27u);

  EXPECT_EQ(multipart_data[0], boundary);
  EXPECT_EQ(multipart_data[1], "Content-Disposition: form-data; name=\"prod\"");
  EXPECT_TRUE(multipart_data[2].empty());
  EXPECT_THAT(multipart_data[3], testing::HasSubstr("Chrome"));
  EXPECT_THAT(multipart_data[3], testing::HasSubstr("webrtc"));

  EXPECT_EQ(multipart_data[4], boundary);
  EXPECT_EQ(multipart_data[5], "Content-Disposition: form-data; name=\"ver\"");
  EXPECT_TRUE(multipart_data[6].empty());
  EXPECT_FALSE(multipart_data[7].empty());

  EXPECT_EQ(multipart_data[8], boundary);
  EXPECT_EQ(multipart_data[9], "Content-Disposition: form-data; name=\"guid\"");
  EXPECT_TRUE(multipart_data[10].empty());
  EXPECT_FALSE(multipart_data[11].empty());

  EXPECT_EQ(multipart_data[12], boundary);
  EXPECT_EQ(multipart_data[13],
            "Content-Disposition: form-data; name=\"type\"");
  EXPECT_TRUE(multipart_data[14].empty());
  EXPECT_EQ(multipart_data[15], "webrtc_log");

  EXPECT_EQ(multipart_data[16], boundary);
  EXPECT_EQ(multipart_data[17],
            "Content-Disposition: form-data; name=\"__uuid__\"");
  EXPECT_TRUE(multipart_data[18].empty());
  EXPECT_TRUE(IsValidUuid(multipart_data[19]));

  EXPECT_EQ(multipart_data[20], boundary);
  EXPECT_EQ(multipart_data[21],
            base::StrCat(
                {"Content-Disposition: form-data; name=\"", test_key, "\""}));
  EXPECT_TRUE(multipart_data[22].empty());
  EXPECT_EQ(multipart_data[23], test_value);

  EXPECT_EQ(multipart_data[24], boundary);
  EXPECT_EQ(multipart_data[25],
            "Content-Disposition: form-data; name=\"other_webrtc_log\"; "
            "filename=\"other_webrtc_log.gz\"");
  EXPECT_TRUE(multipart_data[26].empty());
}

TEST_F(RTCDiagnosticLoggingTest, UploadSiteOriginLogging) {
  auto [uuid, uploaded, uncompressed_log] = StartAndStopLogging(
      /*upload=*/true, StopAction::kFinish, {},
      GURL("https://example.upload.com"));
  EXPECT_THAT(
      uploaded,
      testing::HasSubstr("Content-Disposition: form-data; name=\"webrtc_log\"; "
                         "filename=\"webrtc_log.gz\""));
  EXPECT_THAT(uncompressed_log,
              testing::HasSubstr("test message in StartAndStopLogging"));
}

TEST_F(RTCDiagnosticLoggingTest, StoreDiagnosticLogging) {
  auto [uuid, uploaded, uncompressed_log] =
      StartAndStopLogging(/*upload=*/false, StopAction::kFinish, {});
  EXPECT_TRUE(uploaded.empty());
  EXPECT_THAT(uncompressed_log,
              testing::HasSubstr("test message in StartAndStopLogging"));
}

TEST_F(RTCDiagnosticLoggingTest, StoreDiagnosticLoggingWithoutStop) {
  auto [uuid, uploaded, uncompressed_log] =
      StartAndStopLogging(/*upload=*/false, StopAction::kNothing, {});
  EXPECT_TRUE(uploaded.empty());
  // It might not be stored if not explicitly finished and upload=false.
}

TEST_F(RTCDiagnosticLoggingTest, CancelDiagnosticLogging) {
  auto [uuid, uploaded, uncompressed_log] =
      StartAndStopLogging(/*upload=*/true, StopAction::kCancel, {});
  EXPECT_TRUE(uploaded.empty());
  EXPECT_TRUE(uncompressed_log.empty());
}

TEST_F(RTCDiagnosticLoggingTest, UploadDiagnosticLoggingWithoutStop) {
  auto [uuid, uploaded, uncompressed_log] =
      StartAndStopLogging(/*upload=*/true, StopAction::kNothing, {});
  EXPECT_FALSE(uploaded.empty());
  EXPECT_THAT(uncompressed_log,
              testing::HasSubstr("test message in StartAndStopLogging"));
}

TEST_F(RTCDiagnosticLoggingTest, GlobalPolicyDisabled) {
  NavigateAndCommit(GURL("https://example.com"));
  profile()->GetPrefs()->SetBoolean(prefs::kWebRtcTextLogCollectionAllowed,
                                    false);
  base::ListValue allowed_origins;
  allowed_origins.Append("https://example.com");
  profile()->GetPrefs()->SetList(
      prefs::kWebRTCDiagnosticLogCollectionAllowedForOrigins,
      std::move(allowed_origins));

  base::test::TestFuture<const std::string&> future;
  content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
      *main_rfh(),
      /*should_upload_on_stop=*/true, {}, future.GetCallback());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_FALSE(controller()->web_api_settings().has_value());
}

TEST_F(RTCDiagnosticLoggingTest, GlobalPolicyEnabled) {
  NavigateAndCommit(GURL("https://example.com"));
  profile()->GetPrefs()->SetBoolean(prefs::kWebRtcTextLogCollectionAllowed,
                                    true);
  base::ListValue allowed_origins;
  allowed_origins.Append("https://example.com");
  profile()->GetPrefs()->SetList(
      prefs::kWebRTCDiagnosticLogCollectionAllowedForOrigins,
      std::move(allowed_origins));

  base::test::TestFuture<const std::string&> future;
  content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
      *main_rfh(),
      /*should_upload_on_stop=*/true, {}, future.GetCallback());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_TRUE(controller()->web_api_settings().has_value());
}

TEST_F(RTCDiagnosticLoggingTest, OriginPolicyBlocked) {
  NavigateAndCommit(GURL("https://example.com"));
  profile()->GetPrefs()->SetBoolean(prefs::kWebRtcTextLogCollectionAllowed,
                                    true);
  base::ListValue allowed_origins;
  allowed_origins.Append("https://other-example.com");
  profile()->GetPrefs()->SetList(
      prefs::kWebRTCDiagnosticLogCollectionAllowedForOrigins,
      std::move(allowed_origins));

  base::test::TestFuture<const std::string&> future;
  content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
      *main_rfh(),
      /*should_upload_on_stop=*/true, {}, future.GetCallback());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_FALSE(controller()->web_api_settings().has_value());
}

TEST_F(RTCDiagnosticLoggingTest, OriginPolicyPatternMatch) {
  NavigateAndCommit(GURL("https://example.com"));
  profile()->GetPrefs()->SetBoolean(prefs::kWebRtcTextLogCollectionAllowed,
                                    true);
  base::ListValue allowed_origins;
  allowed_origins.Append("https://[*.]example.com");
  profile()->GetPrefs()->SetList(
      prefs::kWebRTCDiagnosticLogCollectionAllowedForOrigins,
      std::move(allowed_origins));

  base::test::TestFuture<const std::string&> future;
  content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
      *main_rfh(),
      /*should_upload_on_stop=*/true, {}, future.GetCallback());
  EXPECT_FALSE(future.Get().empty());
  EXPECT_TRUE(controller()->web_api_settings().has_value());
}

TEST_F(RTCDiagnosticLoggingTest, SameOriginSubframeLoggingAllowed) {
  NavigateAndCommit(GURL("https://example.com"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/subframe"), subframe);

  base::test::TestFuture<const std::string&> future;
  content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
      *subframe,
      /*should_upload_on_stop=*/true, {}, future.GetCallback());

  EXPECT_FALSE(future.Get().empty());
  EXPECT_TRUE(controller()->web_api_settings().has_value());
}

TEST_F(RTCDiagnosticLoggingTest, CrossOriginSubframeLoggingNotAllowed) {
  NavigateAndCommit(GURL("https://example.com"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other-example.com"), subframe);

  base::test::TestFuture<const std::string&> future;
  content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
      *subframe,
      /*should_upload_on_stop=*/true, {}, future.GetCallback());

  EXPECT_FALSE(future.Get().empty());
  EXPECT_FALSE(controller()->web_api_settings().has_value());
}

TEST_F(RTCDiagnosticLoggingTest, OriginChangeBlocksLogging) {
  NavigateAndCommit(GURL("https://example.com"));
  content::RenderProcessHost* rph = main_rfh()->GetProcess();

  base::test::TestFuture<const std::string&> future;
  content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
      *main_rfh(),
      /*should_upload_on_stop=*/true, {}, future.GetCallback());
  EXPECT_FALSE(future.Get().empty());
  auto* controller = GetControllerForProcess(rph);
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->web_api_settings().has_value());

  // Navigate to a different origin.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://other-example.com"));

  // Now, any operation should be unauthorized.
  base::test::TestFuture<void> stop_future;
  content::GetContentClientForTesting()->browser()->FinishRtcDiagnosticLogging(
      *main_rfh(), stop_future.GetCallback());
  EXPECT_TRUE(stop_future.Wait());

  // Logging should STILL be active because the Finish call was unauthorized.
  EXPECT_TRUE(controller->web_api_settings().has_value());
}

TEST_F(RTCDiagnosticLoggingTest, AddMessagesAuthorized) {
  NavigateAndCommit(GURL("https://example.com"));

  base::test::TestFuture<const std::string&> future;
  content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
      *main_rfh(),
      /*should_upload_on_stop=*/true, {}, future.GetCallback());
  EXPECT_FALSE(future.Get().empty());

  task_environment()->RunUntilIdle();

  // Add a message.
  std::vector<chrome::mojom::WebRtcLoggingMessagePtr> messages;
  messages.push_back(chrome::mojom::WebRtcLoggingMessage::New(base::Time::Now(),
                                                              "test message"));
  ASSERT_TRUE(controller());
  controller()->OnAddMessages(std::move(messages));

  // Finish and verify.
  base::test::TestFuture<void> stop_future;
  content::GetContentClientForTesting()->browser()->FinishRtcDiagnosticLogging(
      *main_rfh(), stop_future.GetCallback());
  EXPECT_TRUE(stop_future.Wait());
  task_environment()->RunUntilIdle();
  agent_.reset();
  task_environment()->RunUntilIdle();

  // Verify the log contains our message.
  if (auto* uploader = webrtc_log_uploader()) {
    base::test::TestFuture<void> uploader_future;
    uploader->background_task_runner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), uploader_future.GetCallback());
    EXPECT_TRUE(uploader_future.Wait());
  }

  base::FilePath log_dir =
      webrtc_logging::TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
          profile()->GetPath(), webrtc_logging::ApiType::kWeb);
  base::FileEnumerator enumerator(log_dir, /*recursive=*/false,
                                  base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*.gz"));
  base::FilePath log_file = enumerator.Next();
  ASSERT_FALSE(log_file.empty());
  std::string compressed;
  ASSERT_TRUE(base::ReadFileToString(log_file, &compressed));
  std::string uncompressed;
  ASSERT_TRUE(compression::GzipUncompress(compressed, &uncompressed));

  EXPECT_THAT(uncompressed, testing::HasSubstr("test message"));

  base::DeletePathRecursively(log_dir);
}

TEST_F(RTCDiagnosticLoggingTest, AddMessagesUnauthorized) {
  NavigateAndCommit(GURL("https://example.com"));
  content::RenderProcessHost* rph = main_rfh()->GetProcess();

  base::test::TestFuture<const std::string&> future;
  content::GetContentClientForTesting()->browser()->StartRtcDiagnosticLogging(
      *main_rfh(),
      /*should_upload_on_stop=*/true, {}, future.GetCallback());
  EXPECT_FALSE(future.Get().empty());
  task_environment()->RunUntilIdle();

  auto* controller = GetControllerForProcess(rph);
  ASSERT_TRUE(controller);

  // Add a message WHILE authorized.
  std::vector<chrome::mojom::WebRtcLoggingMessagePtr> messages;
  messages.push_back(chrome::mojom::WebRtcLoggingMessage::New(
      base::Time::Now(), "authorized message"));
  controller->OnAddMessages(std::move(messages));

  // Navigate away to become unauthorized.
  // We use NavigationSimulator to ensure a proper cross-origin navigation
  // that changes the last committed origin.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://other-example.com"));

  // The origin should have changed.
  ASSERT_NE(url::Origin::Create(GURL("https://example.com")),
            main_rfh()->GetLastCommittedOrigin());

  // Add a message WHILE UNAUTHORIZED.
  std::vector<chrome::mojom::WebRtcLoggingMessagePtr> messages2;
  messages2.push_back(chrome::mojom::WebRtcLoggingMessage::New(
      base::Time::Now(), "unauthorized message"));
  controller->OnAddMessages(std::move(messages2));

  agent_.reset();
  task_environment()->RunUntilIdle();

  if (auto* uploader = webrtc_log_uploader()) {
    base::test::TestFuture<void> uploader_future;
    uploader->background_task_runner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), uploader_future.GetCallback());
    EXPECT_TRUE(uploader_future.Wait());
  }

  base::FilePath log_dir =
      webrtc_logging::TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
          profile()->GetPath(), webrtc_logging::ApiType::kWeb);
  base::FileEnumerator enumerator(log_dir, /*recursive=*/false,
                                  base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*.gz"));
  base::FilePath log_file = enumerator.Next();
  ASSERT_FALSE(log_file.empty());
  std::string compressed;
  ASSERT_TRUE(base::ReadFileToString(log_file, &compressed));
  std::string uncompressed;
  ASSERT_TRUE(compression::GzipUncompress(compressed, &uncompressed));

  // The authorized message should be in the log.
  EXPECT_THAT(uncompressed, testing::HasSubstr("authorized message"));

  // The unauthorized message should NOT be in the log.
  EXPECT_THAT(uncompressed,
              testing::Not(testing::HasSubstr("unauthorized message")));

  base::DeletePathRecursively(log_dir);
}

}  // namespace
#endif
