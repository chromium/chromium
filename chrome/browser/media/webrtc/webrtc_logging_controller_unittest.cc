// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"

#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/media/webrtc_logging.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_store.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

namespace webrtc_text_log {

using ::testing::_;

using BrowserContext = content::BrowserContext;
using MockRenderProcessHost = content::MockRenderProcessHost;
using RenderProcessHost = content::RenderProcessHost;

class FakeWebRtcLoggingAgent : public chrome::mojom::WebRtcLoggingAgent {
 public:
  FakeWebRtcLoggingAgent() = default;
  ~FakeWebRtcLoggingAgent() override = default;

  void Start(
      mojo::PendingRemote<chrome::mojom::WebRtcLoggingClient> client) override {
    client_.reset();
    client_.Bind(std::move(client));
  }

  void Stop() override {
    if (client_.is_bound()) {
      client_->OnStopped();
    }
  }

  void Bind(mojo::ScopedMessagePipeHandle pipe) {
    receivers_.Add(this,
                   mojo::PendingReceiver<chrome::mojom::WebRtcLoggingAgent>(
                       std::move(pipe)));
  }

  void Disconnect() {
    receivers_.Clear();
    client_.reset();
  }

 private:
  mojo::ReceiverSet<chrome::mojom::WebRtcLoggingAgent> receivers_;
  mojo::Remote<chrome::mojom::WebRtcLoggingClient> client_;
};

class WebRtcLoggingControllerTest : public ::testing::Test {
 public:
  WebRtcLoggingControllerTest()
      : browser_context_(nullptr),
        test_shared_url_loader_factory_(
            test_url_loader_factory_.GetSafeWeakWrapper()) {
#if BUILDFLAG(IS_CHROMEOS)
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);
#endif
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_shared_url_loader_factory_);

    EXPECT_TRUE(profiles_dir_.CreateUniqueTempDir());
  }

  WebRtcLoggingControllerTest(const WebRtcLoggingControllerTest&) = delete;
  WebRtcLoggingControllerTest& operator=(const WebRtcLoggingControllerTest&) =
      delete;

  ~WebRtcLoggingControllerTest() override {
    webrtc_logging_controller_ = nullptr;
    if (browser_context_) {
      UnloadMainTestProfile();
    }
#if BUILDFLAG(IS_CHROMEOS)
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
#endif
  }

  void LoadMainTestProfile(std::optional<bool> text_log_collection_allowed) {
    browser_context_ = CreateBrowserContext("browser_context_", true,
                                            text_log_collection_allowed);
    CreateRenderHost();
  }

  void UnloadMainTestProfile() {
    TestingBrowserProcess::GetGlobal()->webrtc_log_uploader()->Shutdown();
    TestingBrowserProcess::GetGlobal()->SetWebRtcLogUploader(nullptr);
    rph_.reset();
    browser_context_.reset();
  }

  void CreateRenderHost() {
    rph_ = std::make_unique<MockRenderProcessHost>(browser_context_.get());
    rph_->OverrideBinderForTesting(
        chrome::mojom::WebRtcLoggingAgent::Name_,
        base::BindRepeating(&FakeWebRtcLoggingAgent::Bind,
                            base::Unretained(&fake_agent_)));
    auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
    TestingBrowserProcess::GetGlobal()->SetWebRtcLogUploader(
        std::move(webrtc_log_uploader));
    WebRtcLoggingController::AttachToRenderProcessHost(rph_.get());
    webrtc_logging_controller_ =
        WebRtcLoggingController::FromRenderProcessHost(rph_.get());
  }

  void CreateUnmanagedProfile() {
    browser_context_ = CreateBrowserContext(
        "browser_context_", /*is_managed_profile=*/false,
        /*text_log_collection_allowed_by_global_policy=*/std::nullopt);
    CreateRenderHost();
  }

  std::unique_ptr<TestingProfile> CreateBrowserContext(
      std::string profile_name,
      bool is_managed_profile,
      std::optional<bool> text_log_collection_allowed_by_global_policy) {
    // If profile name not specified, select a unique name.
    if (profile_name.empty()) {
      static size_t index = 0;
      profile_name = base::NumberToString(++index);
    }

    // Set a directory for the profile, derived from its name, so that
    // recreating the profile will get the same directory.
    const base::FilePath profile_path =
        profiles_dir_.GetPath().AppendASCII(profile_name);
    if (base::PathExists(profile_path)) {
      EXPECT_TRUE(base::DirectoryExists(profile_path));
    } else {
      EXPECT_TRUE(base::CreateDirectory(profile_path));
    }

    // Prepare to specify preferences for the profile.
    sync_preferences::PrefServiceMockFactory factory;
    factory.set_user_prefs(base::WrapRefCounted(new TestingPrefStore()));
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    sync_preferences::PrefServiceSyncable* regular_prefs =
        factory.CreateSyncable(registry.get()).release();

    // Set the preference associated with the policy for
    // WebRtcTextLogCollectionAllowed
    RegisterUserProfilePrefs(registry.get());
    if (text_log_collection_allowed_by_global_policy.has_value()) {
      regular_prefs->SetBoolean(
          prefs::kWebRtcTextLogCollectionAllowed,
          text_log_collection_allowed_by_global_policy.value());
    }

    // Build the profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(profile_name);
    profile_builder.SetPath(profile_path);
    profile_builder.SetPrefService(base::WrapUnique(regular_prefs));
    profile_builder.OverridePolicyConnectorIsManagedForTesting(
        is_managed_profile);

    std::unique_ptr<TestingProfile> profile = profile_builder.Build();

    return profile;
  }

  // The directory which will contain all profiles.
  base::ScopedTempDir profiles_dir_;

  // Default BrowserContext
  std::unique_ptr<TestingProfile> browser_context_;
  std::unique_ptr<MockRenderProcessHost> rph_;
  FakeWebRtcLoggingAgent fake_agent_;

  // Class under test.
  raw_ptr<WebRtcLoggingController> webrtc_logging_controller_ = nullptr;

  // Testing utilities.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

#if BUILDFLAG(IS_CHROMEOS)
  ash::system::FakeStatisticsProvider fake_statistics_provider_;
#endif
};

TEST_F(WebRtcLoggingControllerTest, ManagedProfileWithTruePolicy) {
  LoadMainTestProfile(true);
  EXPECT_TRUE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get()));
}

TEST_F(WebRtcLoggingControllerTest, ManagedProfileWithFalsePolicy) {
  LoadMainTestProfile(false);
  EXPECT_FALSE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get()));
}

TEST_F(WebRtcLoggingControllerTest, ManagedProfileWithUnsetPolicy) {
  LoadMainTestProfile(std::nullopt);
  EXPECT_TRUE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get()));
}

TEST_F(WebRtcLoggingControllerTest, EnterpriseIsolatedModeWithUnsetPolicy) {
  LoadMainTestProfile(std::nullopt);
  Profile* isolated_profile =
      browser_context_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  profile_metrics::SetBrowserProfileType(
      isolated_profile,
      profile_metrics::BrowserProfileType::kEnterpriseIsolated);

  EXPECT_FALSE(
      webrtc_logging_controller_->IsWebRtcTextLogAllowed(isolated_profile));

  browser_context_->DestroyOffTheRecordProfile(isolated_profile);
}

TEST_F(WebRtcLoggingControllerTest, IncognitoWithUnsetPolicy) {
  LoadMainTestProfile(std::nullopt);
  Profile* incognito_profile =
      browser_context_->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_FALSE(
      webrtc_logging_controller_->IsWebRtcTextLogAllowed(incognito_profile));

  browser_context_->DestroyOffTheRecordProfile(incognito_profile);
}

TEST_F(WebRtcLoggingControllerTest, UnmanagedProfileWithUnsetPolicy) {
  CreateUnmanagedProfile();
  EXPECT_TRUE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get()));
}

TEST_F(WebRtcLoggingControllerTest, NullBrowserContext) {
  EXPECT_FALSE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(nullptr));
}

TEST_F(WebRtcLoggingControllerTest, NullBrowserContextWeb) {
  EXPECT_FALSE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      nullptr, webrtc_logging::ApiType::kWeb));
}

TEST_F(WebRtcLoggingControllerTest, WebApiOriginPolicy) {
  LoadMainTestProfile(true);
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));

  // No origins allowed by default.
  EXPECT_FALSE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get(), webrtc_logging::ApiType::kWeb, origin));

  // Allow origin.
  base::ListValue allowed_origins;
  allowed_origins.Append("https://example.com");
  browser_context_->GetPrefs()->SetList(
      prefs::kWebRTCDiagnosticLogCollectionAllowedForOrigins,
      std::move(allowed_origins));

  EXPECT_TRUE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get(), webrtc_logging::ApiType::kWeb, origin));

  // Different origin still blocked.
  url::Origin other_origin =
      url::Origin::Create(GURL("https://other-example.com"));
  EXPECT_FALSE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get(), webrtc_logging::ApiType::kWeb, other_origin));
}

TEST_F(WebRtcLoggingControllerTest, WebApiPatternPolicy) {
  LoadMainTestProfile(true);
  url::Origin origin = url::Origin::Create(GURL("https://sub.example.com"));

  base::ListValue allowed_origins;
  allowed_origins.Append("https://[*.]example.com");
  browser_context_->GetPrefs()->SetList(
      prefs::kWebRTCDiagnosticLogCollectionAllowedForOrigins,
      std::move(allowed_origins));

  EXPECT_TRUE(webrtc_logging_controller_->IsWebRtcTextLogAllowed(
      browser_context_.get(), webrtc_logging::ApiType::kWeb, origin));
}

TEST_F(WebRtcLoggingControllerTest, StartRtpDump_KWeb_ReturnsError) {
  LoadMainTestProfile(true);
  WebRtcLoggingController::WebApiSettings settings;
  settings.origin = url::Origin::Create(GURL("https://example.com"));

  // StartLogging with settings enters kWeb mode.
  base::test::TestFuture<bool, const std::string&> start_future;
  webrtc_logging_controller_->StartLogging(start_future.GetCallback(),
                                           settings);
  EXPECT_TRUE(start_future.Get<0>()) << start_future.Get<1>();
  if (!start_future.Get<0>()) {
    return;
  }

  // StartRtpDump is explicitly blocked when GetApiType() is kWeb.
  base::test::TestFuture<bool, const std::string&> rtp_future;
  webrtc_logging_controller_->StartRtpDump(RTP_DUMP_BOTH,
                                           rtp_future.GetCallback());
  EXPECT_FALSE(rtp_future.Get<0>());
  EXPECT_EQ(rtp_future.Get<1>(), "Not authorized");
}

TEST_F(WebRtcLoggingControllerTest, StopLogging_KWeb_NotAuthorized) {
  LoadMainTestProfile(true);
  WebRtcLoggingController::WebApiSettings settings;
  settings.origin = url::Origin::Create(GURL("https://example.com"));

  // StartLogging with settings enters kWeb mode.
  base::test::TestFuture<bool, const std::string&> start_future;
  webrtc_logging_controller_->StartLogging(start_future.GetCallback(),
                                           settings);
  EXPECT_TRUE(start_future.Get<0>()) << start_future.Get<1>();
  if (!start_future.Get<0>()) {
    return;
  }

  base::test::TestFuture<bool, const std::string&> stop_future;
  webrtc_logging_controller_->StopLogging(stop_future.GetCallback());
  EXPECT_FALSE(stop_future.Get<0>());
  EXPECT_EQ(stop_future.Get<1>(), "Not authorized");
}

TEST_F(WebRtcLoggingControllerTest, SetMetaData_KWeb_NotAuthorized) {
  LoadMainTestProfile(true);
  WebRtcLoggingController::WebApiSettings settings;
  settings.origin = url::Origin::Create(GURL("https://example.com"));

  base::test::TestFuture<bool, const std::string&> start_future;
  webrtc_logging_controller_->StartLogging(start_future.GetCallback(),
                                           settings);
  EXPECT_TRUE(start_future.Get<0>()) << start_future.Get<1>();
  if (!start_future.Get<0>()) {
    return;
  }

  base::test::TestFuture<bool, const std::string&> meta_future;
  webrtc_logging_controller_->SetMetaData(
      std::make_unique<WebRtcLogMetaDataMap>(), meta_future.GetCallback());
  EXPECT_FALSE(meta_future.Get<0>());
  EXPECT_EQ(meta_future.Get<1>(), "Not authorized");
}

TEST_F(WebRtcLoggingControllerTest, UploadLog_KWeb_NotAuthorized) {
  LoadMainTestProfile(true);
  WebRtcLoggingController::WebApiSettings settings;
  settings.origin = url::Origin::Create(GURL("https://example.com"));

  base::test::TestFuture<bool, const std::string&> start_future;
  webrtc_logging_controller_->StartLogging(start_future.GetCallback(),
                                           settings);
  EXPECT_TRUE(start_future.Get<0>()) << start_future.Get<1>();
  if (!start_future.Get<0>()) {
    return;
  }

  base::test::TestFuture<bool, const std::string&, const std::string&>
      upload_future;
  webrtc_logging_controller_->UploadLog(upload_future.GetCallback());
  EXPECT_FALSE(upload_future.Get<0>());
  EXPECT_EQ(upload_future.Get<2>(), "Not authorized");
}

TEST_F(WebRtcLoggingControllerTest, StartEventLogging_KWeb_NotAuthorized) {
  LoadMainTestProfile(true);

  base::test::TestFuture<bool, const std::string&, const std::string&>
      event_log_future;
  webrtc_logging_controller_->StartEventLogging(webrtc_logging::ApiType::kWeb,
                                                "session_id", 1024, 1000, 123,
                                                event_log_future.GetCallback());
  EXPECT_FALSE(event_log_future.Get<0>());
}

TEST_F(WebRtcLoggingControllerTest, DestroyWhileUploadOnRenderClosePending) {
  LoadMainTestProfile(true);

  base::test::TestFuture<bool, const std::string&> start_future;
  webrtc_logging_controller_->StartLogging(start_future.GetCallback());
  ASSERT_TRUE(start_future.Get<0>()) << start_future.Get<1>();
  webrtc_logging_controller_->set_upload_log_on_render_close(true);

  // Disconnect the renderer agent and run only the UI thread task that handles
  // OnAgentDisconnected(), leaving the background PostTaskAndReplyWithResult
  // pending when the test environment tears down.
  fake_agent_.Disconnect();
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  webrtc_logging_controller_ = nullptr;
  rph_.reset();
}

TEST_F(WebRtcLoggingControllerTest, UploadLog_ReportIdValidationAndMaxLength) {
  LoadMainTestProfile(true);
  WebRtcLogUploader* uploader =
      TestingBrowserProcess::GetGlobal()->webrtc_log_uploader();
  const base::FilePath log_dir =
      webrtc_logging::TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
          browser_context_->GetPath(), webrtc_logging::ApiType::kExtension);
  const base::FilePath log_list_path =
      webrtc_logging::TextLogList::GetWebRtcLogListFileForDirectory(log_dir);

  auto run_upload_with_response = [&](const std::string& response_body) {
    base::test::TestFuture<bool, const std::string&> start_future;
    webrtc_logging_controller_->StartLogging(start_future.GetCallback());
    EXPECT_TRUE(start_future.Get<0>()) << start_future.Get<1>();

    base::test::TestFuture<bool, const std::string&> stop_future;
    webrtc_logging_controller_->StopLogging(stop_future.GetCallback());
    EXPECT_TRUE(stop_future.Get<0>()) << stop_future.Get<1>();

    base::test::TestFuture<bool, const std::string&, const std::string&>
        upload_future;
    webrtc_logging_controller_->UploadLog(upload_future.GetCallback());

    base::RunLoop wait_for_request;
    test_url_loader_factory_.SetInterceptor(
        base::BindRepeating([](base::RepeatingClosure quit,
                               const network::ResourceRequest&) { quit.Run(); },
                            wait_for_request.QuitClosure()));
    wait_for_request.Run();
    test_url_loader_factory_.SetInterceptor(
        network::TestURLLoaderFactory::Interceptor());

    EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
        uploader->upload_url().spec(), response_body));
    auto result = upload_future.Take();

    base::RunLoop flush_bg;
    uploader->background_task_runner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), flush_bg.QuitClosure());
    flush_bg.Run();
    return result;
  };

  auto read_log_list_lines = [&]() {
    std::string log_list;
    EXPECT_TRUE(base::ReadFileToString(log_list_path, &log_list));
    return base::SplitString(log_list, "\n", base::KEEP_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  };
  auto expect_uploaded_line = [](const std::string& line,
                                 const std::string& expected_report_id) {
    std::vector<std::string> fields = base::SplitString(
        line, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    ASSERT_EQ(4u, fields.size());
    EXPECT_FALSE(fields[0].empty());           // upload time filled in
    EXPECT_EQ(expected_report_id, fields[1]);  // report ID
    EXPECT_FALSE(fields[2].empty());  // local ID kept: updated in place
  };
  auto expect_failure_histograms = [](const base::HistogramTester& tester) {
    tester.ExpectTotalCount("WebRtcTextLogging.UploadSuccessful", 0);
    tester.ExpectTotalCount("WebRtcTextLogging.UploadFailed", 1);
    tester.ExpectUniqueSample("WebRtcTextLogging.UploadFailureReason",
                              WebRtcLogUploadFailureReason::kInvalidReportId,
                              1);
    tester.ExpectTotalCount("WebRtcTextLogging.UploadFailureNetErrorCode", 0);
  };
  auto expect_success_histograms = [](const base::HistogramTester& tester) {
    tester.ExpectTotalCount("WebRtcTextLogging.UploadSuccessful", 1);
    tester.ExpectTotalCount("WebRtcTextLogging.UploadFailed", 0);
    tester.ExpectTotalCount("WebRtcTextLogging.UploadFailureReason", 0);
    tester.ExpectTotalCount("WebRtcTextLogging.UploadFailureNetErrorCode", 0);
  };
  const std::string valid_256(256, 'b');

  // (a) A 257-character body exceeds kMaxReportIdLength (256) and must fail.
  {
    base::HistogramTester histogram_tester;
    auto [success, report_id, error] =
        run_upload_with_response(std::string(257, 'a'));
    EXPECT_FALSE(success);
    EXPECT_EQ("", report_id);
    EXPECT_EQ("Uploading failed, invalid report ID", error);
    expect_failure_histograms(histogram_tester);

    std::vector<std::string> lines = read_log_list_lines();
    ASSERT_EQ(1u, lines.size());
    EXPECT_TRUE(lines[0].starts_with(",,"));
  }

  // (b) A body with an embedded newline fails IsValidReportId() and must fail.
  {
    base::HistogramTester histogram_tester;
    auto [success, report_id, error] = run_upload_with_response("bad\nid");
    EXPECT_FALSE(success);
    EXPECT_EQ("", report_id);
    EXPECT_EQ("Uploading failed, invalid report ID", error);
    expect_failure_histograms(histogram_tester);

    std::vector<std::string> lines = read_log_list_lines();
    ASSERT_EQ(2u, lines.size());
    EXPECT_TRUE(lines[1].starts_with(",,"));
  }

  // (c) Exactly 256 valid characters succeeds and updates Log List in place.
  {
    base::HistogramTester histogram_tester;
    auto [success, report_id, error] = run_upload_with_response(valid_256);
    EXPECT_TRUE(success);
    EXPECT_EQ(valid_256, report_id);
    EXPECT_EQ("", error);
    expect_success_histograms(histogram_tester);

    std::vector<std::string> lines = read_log_list_lines();
    ASSERT_EQ(3u, lines.size());
    EXPECT_TRUE(lines[0].starts_with(",,"));  // (a) untouched
    EXPECT_TRUE(lines[1].starts_with(",,"));  // (b) untouched
    expect_uploaded_line(lines[2], valid_256);
  }

  // (d) A trailing "\r\n" is trimmed, so a 256-character ID followed by a line
  // break (258 bytes) still succeeds.
  {
    base::HistogramTester histogram_tester;
    auto [success, report_id, error] =
        run_upload_with_response(valid_256 + "\r\n");
    EXPECT_TRUE(success);
    EXPECT_EQ(valid_256, report_id);
    EXPECT_EQ("", error);
    expect_success_histograms(histogram_tester);

    std::vector<std::string> lines = read_log_list_lines();
    ASSERT_EQ(4u, lines.size());
    expect_uploaded_line(lines[3], valid_256);
  }

  // (e) A body larger than the download cap makes DownloadToString() fail with
  // ERR_INSUFFICIENT_RESOURCES, which must also be reported as an invalid
  // report ID.
  {
    base::HistogramTester histogram_tester;
    auto [success, report_id, error] =
        run_upload_with_response(std::string(300, 'c'));
    EXPECT_FALSE(success);
    EXPECT_EQ("", report_id);
    EXPECT_EQ("Uploading failed, invalid report ID", error);
    expect_failure_histograms(histogram_tester);

    std::vector<std::string> lines = read_log_list_lines();
    ASSERT_EQ(5u, lines.size());
    EXPECT_TRUE(lines[4].starts_with(",,"));
  }
}

// If no local copy of the log was stored (empty local_log_id), a successful
// upload must not touch Log List. Previously the report ID was written into
// whichever unrelated entry matched first (and a CHECK fired, see
// crbug.com/581840776).
TEST_F(WebRtcLoggingControllerTest, UploadLog_NoLocalCopyDoesNotTouchLogList) {
  LoadMainTestProfile(true);
  WebRtcLogUploader* uploader =
      TestingBrowserProcess::GetGlobal()->webrtc_log_uploader();
  const base::FilePath log_dir =
      webrtc_logging::TextLogList::GetWebRtcLogDirectoryForBrowserContextPath(
          browser_context_->GetPath(), webrtc_logging::ApiType::kExtension);
  const base::FilePath log_list_path =
      webrtc_logging::TextLogList::GetWebRtcLogListFileForDirectory(log_dir);

  base::test::TestFuture<bool, const std::string&> start_future;
  webrtc_logging_controller_->StartLogging(start_future.GetCallback());
  ASSERT_TRUE(start_future.Get<0>()) << start_future.Get<1>();

  base::test::TestFuture<bool, const std::string&> stop_future;
  webrtc_logging_controller_->StopLogging(stop_future.GetCallback());
  ASSERT_TRUE(stop_future.Get<0>()) << stop_future.Get<1>();

  base::test::TestFuture<bool, const std::string&, const std::string&>
      upload_future;
  webrtc_logging_controller_->UploadLog(upload_future.GetCallback());
  // The directory getter and WebRtcLogUploader::OnLoggingStopped() both run on
  // the uploader's background sequence, so this runs after the directory is
  // created and before OnLoggingStopped() checks it: no local copy is stored
  // and local_log_id stays empty.
  uploader->background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                     log_dir));

  base::RunLoop wait_for_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindRepeating([](base::RepeatingClosure quit,
                             const network::ResourceRequest&) { quit.Run(); },
                          wait_for_request.QuitClosure()));
  wait_for_request.Run();
  test_url_loader_factory_.SetInterceptor(
      network::TestURLLoaderFactory::Interceptor());
  ASSERT_FALSE(base::PathExists(log_dir));

  // Before the server responds, plant a Log List with one unrelated,
  // not-yet-uploaded entry.
  ASSERT_TRUE(base::CreateDirectory(log_dir));
  constexpr char kUnrelatedLine[] = ",,other-local-id,1700000000\n";
  ASSERT_TRUE(base::WriteFile(log_list_path, kUnrelatedLine));

  ASSERT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      uploader->upload_url().spec(), "valid-report-id"));
  auto [success, report_id, error] = upload_future.Take();
  EXPECT_TRUE(success);
  EXPECT_EQ("valid-report-id", report_id);

  base::RunLoop flush_bg;
  uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), flush_bg.QuitClosure());
  flush_bg.Run();

  std::string log_list;
  ASSERT_TRUE(base::ReadFileToString(log_list_path, &log_list));
  EXPECT_EQ(kUnrelatedLine, log_list);
}

}  // namespace webrtc_text_log
