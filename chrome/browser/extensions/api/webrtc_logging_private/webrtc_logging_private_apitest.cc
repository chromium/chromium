// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/zlib/google/compression_utils.h"

using compression::GzipUncompress;
using extensions::Extension;
using extensions::WebrtcLoggingPrivateDiscardFunction;
using extensions::WebrtcLoggingPrivateSetMetaDataFunction;
using extensions::WebrtcLoggingPrivateStartAudioDebugRecordingsFunction;
using extensions::WebrtcLoggingPrivateStartEventLoggingFunction;
using extensions::WebrtcLoggingPrivateStartFunction;
using extensions::WebrtcLoggingPrivateStartRtpDumpFunction;
using extensions::WebrtcLoggingPrivateStopAudioDebugRecordingsFunction;
using extensions::WebrtcLoggingPrivateStopFunction;
using extensions::WebrtcLoggingPrivateStopRtpDumpFunction;
using extensions::WebrtcLoggingPrivateStoreFunction;
using extensions::WebrtcLoggingPrivateUploadFunction;
using extensions::WebrtcLoggingPrivateUploadStoredFunction;
using webrtc_event_logging::kMaxOutputPeriodMs;
using webrtc_event_logging::kMaxRemoteLogFileSizeBytes;
using webrtc_event_logging::kStartRemoteLoggingFailureAlreadyLogging;
using webrtc_event_logging::kStartRemoteLoggingFailureFeatureDisabled;
using webrtc_event_logging::kStartRemoteLoggingFailureMaxSizeTooLarge;
using webrtc_event_logging::kStartRemoteLoggingFailureMaxSizeTooSmall;
using webrtc_event_logging::kStartRemoteLoggingFailureOutputPeriodMsTooLarge;
using webrtc_event_logging::
    kStartRemoteLoggingFailureUnknownOrInactivePeerConnection;
using webrtc_event_logging::kStartRemoteLoggingFailureUnlimitedSizeDisallowed;
using webrtc_event_logging::kWebRtcEventLogManagerUnlimitedFileSize;
using webrtc_event_logging::WebRtcEventLogManager;

namespace utils = extensions::api_test_utils;

namespace {

static const char kTestLoggingSessionIdKey[] = "app_session_id";
static const char kTestLoggingSessionIdValue[] = "0123456789abcdef";
static const char kTestLoggingUrl[] = "dummy url string";

constexpr int kWebAppId = 15;  // Arbitrary.

constexpr char kTestUploadUrlPath[] = "/upload_webrtc_log";
constexpr char kTestReportId[] = "report_id";

std::string ParamsToString(const base::Value::List& parameters) {
  std::string parameter_string;
  EXPECT_TRUE(base::JSONWriter::Write(parameters, &parameter_string));
  return parameter_string;
}

void InitializeTestMetaData(base::Value::List& parameters) {
  base::Value::Dict meta_data_entry;
  meta_data_entry.Set("key", kTestLoggingSessionIdKey);
  meta_data_entry.Set("value", kTestLoggingSessionIdValue);
  base::Value::List meta_data;
  meta_data.Append(std::move(meta_data_entry));
  base::Value::Dict meta_data_entry2;
  meta_data_entry2.Set("key", "url");
  meta_data_entry2.Set("value", kTestLoggingUrl);
  meta_data.Append(std::move(meta_data_entry2));
  parameters.Append(std::move(meta_data));
}

class WebrtcLoggingPrivateApiTest : public extensions::ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    extension_ = extensions::ExtensionBuilder("Test").Build();
  }

  template<typename T>
  scoped_refptr<T> CreateFunction() {
    scoped_refptr<T> function(new T());
    function->set_extension(extension_.get());
    function->set_has_callback(true);
    return function;
  }

  // Overriding can use incognito session instead, etc.
  virtual Browser* GetBrowser() { return browser(); }

  content::WebContents* web_contents() {
    return GetBrowser()->tab_strip_model()->GetActiveWebContents();
  }

  bool SetupTestServerLogUploading() {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&WebrtcLoggingPrivateApiTest::HandleServerRequest,
                            base::Unretained(this)));
    const bool start_result = StartEmbeddedTestServer();
    g_browser_process->webrtc_log_uploader()->SetUploadUrlForTesting(
        embedded_test_server()->GetURL(kTestUploadUrlPath));
    return start_result;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleServerRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url == kTestUploadUrlPath) {
      upload_request_content_ = request.content;
      std::unique_ptr<net::test_server::BasicHttpResponse> response(
          new net::test_server::BasicHttpResponse);
      response->set_code(net::HTTP_OK);
      response->set_content(kTestReportId);
      return std::move(response);
    }

    return nullptr;
  }

  void AppendTabIdAndUrl(base::Value::List& parameters) {
    base::Value::Dict request_info;
    request_info.Set("tabId",
                     extensions::ExtensionTabUtil::GetTabId(web_contents()));
    parameters.Append(std::move(request_info));
    parameters.Append(web_contents()
                          ->GetLastCommittedURL()
                          .DeprecatedGetOriginAsURL()
                          .spec());
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns the value (NOT whether it had succeeded or failed).
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  template <typename Function>
  std::optional<base::Value> RunFunction(const base::Value::List& parameters) {
    scoped_refptr<Function> function(CreateFunction<Function>());
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        function.get(), ParamsToString(parameters), GetBrowser()->profile());
    return result;
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns the value (NOT whether it had succeeded or failed).
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  template <typename Function>
  std::optional<base::Value> RunNoArgsFunction() {
    base::Value::List params;
    AppendTabIdAndUrl(params);
    scoped_refptr<Function> function(CreateFunction<Function>());
    std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
        function.get(), ParamsToString(params), GetBrowser()->profile());
    return result;
  }

  template <typename Function>
  void RunFunctionAndExpectError(const base::Value::List& parameters,
                                 const std::string& expected_error) {
    DCHECK(!expected_error.empty());
    scoped_refptr<Function> function(CreateFunction<Function>());
    const std::string error_message = utils::RunFunctionAndReturnError(
        function.get(), ParamsToString(parameters), GetBrowser()->profile());
    EXPECT_EQ(error_message, expected_error);
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool StartLogging() {
    constexpr bool value_expected = false;
    std::optional<base::Value> value =
        RunNoArgsFunction<WebrtcLoggingPrivateStartFunction>();
    return value_expected == value.has_value();
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool StopLogging() {
    constexpr bool value_expected = false;
    std::optional<base::Value> value =
        RunNoArgsFunction<WebrtcLoggingPrivateStopFunction>();
    return value_expected == value.has_value();
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool DiscardLog() {
    constexpr bool value_expected = false;
    std::optional<base::Value> value =
        RunNoArgsFunction<WebrtcLoggingPrivateDiscardFunction>();
    return value_expected == value.has_value();
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool UploadLog(std::string* report_id) {
    constexpr bool value_expected = true;
    std::optional<base::Value> value =
        RunNoArgsFunction<WebrtcLoggingPrivateUploadFunction>();
    const bool value_returned = value.has_value();
    if (value_returned) {
      *report_id = *value->GetDict().FindString("reportId");
    }
    return value_expected == value_returned;
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool SetMetaData(const base::Value::List& data) {
    constexpr bool value_expected = false;
    std::optional<base::Value> value =
        RunFunction<WebrtcLoggingPrivateSetMetaDataFunction>(data);
    return value_expected == value.has_value();
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool StartRtpDump(bool incoming, bool outgoing) {
    base::Value::List params;
    AppendTabIdAndUrl(params);
    params.Append(incoming);
    params.Append(outgoing);
    constexpr bool value_expected = false;
    std::optional<base::Value> value =
        RunFunction<WebrtcLoggingPrivateStartRtpDumpFunction>(params);
    return value_expected == value.has_value();
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool StopRtpDump(bool incoming, bool outgoing) {
    base::Value::List params;
    AppendTabIdAndUrl(params);
    params.Append(incoming);
    params.Append(outgoing);
    constexpr bool value_expected = false;
    std::optional<base::Value> value =
        RunFunction<WebrtcLoggingPrivateStopRtpDumpFunction>(params);
    return value_expected == value.has_value();
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool StoreLog(const std::string& log_id) {
    base::Value::List params;
    AppendTabIdAndUrl(params);
    params.Append(log_id);
    constexpr bool value_expected = false;
    std::optional<base::Value> value =
        RunFunction<WebrtcLoggingPrivateStoreFunction>(params);
    return value_expected == value.has_value();
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool UploadStoredLog(const std::string& log_id, std::string* report_id) {
    base::Value::List params;
    AppendTabIdAndUrl(params);
    params.Append(log_id);
    constexpr bool value_expected = true;
    std::optional<base::Value> value =
        RunFunction<WebrtcLoggingPrivateUploadStoredFunction>(params);
    const bool value_returned = value.has_value();
    if (value_returned) {
      *report_id = *value->GetDict().FindString("reportId");
    }
    return value_expected == value_returned;
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool StartAudioDebugRecordings(int seconds) {
    base::Value::List params;
    AppendTabIdAndUrl(params);
    params.Append(seconds);
    constexpr bool value_expected = true;
    std::optional<base::Value> value =
        RunFunction<WebrtcLoggingPrivateStartAudioDebugRecordingsFunction>(
            params);
    return value_expected == value.has_value();
  }

  // This function implicitly expects the function to succeed (test failure
  // initiated otherwise).
  // Returns whether the function that was run returned a value, or avoided
  // returning a value, according to expectation.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  bool StopAudioDebugRecordings() {
    base::Value::List params;
    AppendTabIdAndUrl(params);
    constexpr bool value_expected = true;
    std::optional<base::Value> value =
        RunFunction<WebrtcLoggingPrivateStopAudioDebugRecordingsFunction>(
            params);
    return value_expected == value.has_value();
  }

  // This function expects the function to succeed or fail according to
  // |expect_success| (test failure initiated otherwise). It also implicitly
  // expects that no value would be returned.
  // TODO(crbug.com/41381060): Return success/failure of the executed function.
  void StartEventLogging(const std::string& session_id,
                         int max_log_size_bytes,
                         int output_period_ms,
                         int web_app_id,
                         bool expect_success,
                         const std::string& expected_error = std::string()) {
    DCHECK_EQ(expect_success, expected_error.empty());

    base::Value::List params;
    AppendTabIdAndUrl(params);
    params.Append(session_id);
    params.Append(max_log_size_bytes);
    params.Append(output_period_ms);
    params.Append(web_app_id);

    if (expect_success) {
      scoped_refptr<WebrtcLoggingPrivateStartEventLoggingFunction> function(
          CreateFunction<WebrtcLoggingPrivateStartEventLoggingFunction>());

      std::optional<base::Value> result =
          utils::RunFunctionAndReturnSingleResult(
              function.get(), ParamsToString(params), GetBrowser()->profile());

      ASSERT_TRUE(result);
      ASSERT_TRUE(result->is_dict());
      const base::Value::Dict& result_dict = result->GetDict();
      ASSERT_EQ(result_dict.size(), 1u);

      const std::string* log_id = result_dict.FindString("logId");
      ASSERT_TRUE(log_id);
      EXPECT_EQ(log_id->size(), 32u);
      EXPECT_EQ(log_id->find_first_not_of("0123456789ABCDEF"),
                std::string::npos);
    } else {
      RunFunctionAndExpectError<WebrtcLoggingPrivateStartEventLoggingFunction>(
          params, expected_error);
    }
  }

  void SetUpPeerConnection(const std::string& session_id = "") {
    auto* manager = WebRtcEventLogManager::GetInstance();

    content::RenderFrameHost* render_frame_host =
        web_contents()->GetPrimaryMainFrame();
    const content::GlobalRenderFrameHostId frame_id =
        render_frame_host->GetGlobalId();
    const base::ProcessId pid =
        render_frame_host->GetProcess()->GetProcess().Pid();
    const int lid = 0;

    manager->OnPeerConnectionAdded(frame_id, lid, pid, /*url=*/std::string(),
                                   /*rtc_configuration=*/std::string());

    if (!session_id.empty()) {
      manager->OnPeerConnectionSessionIdSet(frame_id, lid, session_id);
    }
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  scoped_refptr<const Extension> extension_;

  // The content of the upload request that reached the test server.
  std::string upload_request_content_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopDiscard) {
  EXPECT_TRUE(StartLogging());
  EXPECT_TRUE(StopLogging());
  EXPECT_TRUE(DiscardLog());
}

// Tests WebRTC diagnostic logging. Sets up the browser to save the multipart
// contents to a buffer instead of uploading it, then verifies it after a calls.
// Example of multipart contents:
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="prod"
//
// Chrome_Linux
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="ver"
//
// 30.0.1554.0
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="guid"
//
// 0
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="type"
//
// webrtc_log
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="app_session_id"
//
// 0123456789abcdef
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="url"
//
// http://127.0.0.1:43213/webrtc/webrtc_jsep01_test.html
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**----
// Content-Disposition: form-data; name="webrtc_log"; filename="webrtc_log.gz"
// Content-Type: application/gzip
//
// <compressed data (zip)>
// ------**--yradnuoBgoLtrapitluMklaTelgooG--**------
//
IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopUpload) {
  ASSERT_TRUE(SetupTestServerLogUploading());

  base::Value::List parameters;
  AppendTabIdAndUrl(parameters);
  InitializeTestMetaData(parameters);

  std::string report_id;

  SetMetaData(parameters);
  StartLogging();
  StopLogging();
  UploadLog(&report_id);

  ASSERT_FALSE(upload_request_content_.empty());
  EXPECT_STREQ(kTestReportId, report_id.c_str());

  // Check multipart data.

  const char boundary[] = "------**--yradnuoBgoLtrapitluMklaTelgooG--**----";

  // Move the compressed data to its own string, since it may contain "\r\n" and
  // it makes the tests below easier.
  const char zip_content_type[] = "Content-Type: application/gzip";
  size_t zip_pos = upload_request_content_.find(&zip_content_type[0]);
  ASSERT_NE(std::string::npos, zip_pos);
  // Move pos to where the zip begins. - 1 to remove '\0', + 4 for two "\r\n".
  zip_pos += sizeof(zip_content_type) + 3;
  size_t zip_length = upload_request_content_.find(boundary, zip_pos);
  ASSERT_NE(std::string::npos, zip_length);
  // Calculate length, adjust for a "\r\n".
  zip_length -= zip_pos + 2;
  ASSERT_GT(zip_length, 0u);
  std::string log_part = upload_request_content_.substr(zip_pos, zip_length);
  upload_request_content_.erase(zip_pos, zip_length);

  // Uncompress log and verify contents.
  EXPECT_TRUE(GzipUncompress(log_part, &log_part));
  EXPECT_GT(log_part.length(), 0u);
  // Verify that meta data exists.
  EXPECT_NE(std::string::npos, log_part.find(base::StringPrintf("%s: %s",
      kTestLoggingSessionIdKey, kTestLoggingSessionIdValue)));
  // Verify that the basic info generated at logging startup exists.
  EXPECT_NE(std::string::npos, log_part.find("Chrome version:"));
  EXPECT_NE(std::string::npos, log_part.find("Cpu brand:"));

  // Check the multipart contents.
  std::vector<std::string> multipart_lines =
      base::SplitStringUsingSubstr(upload_request_content_, "\r\n",
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(31, static_cast<int>(multipart_lines.size()));

  EXPECT_STREQ(&boundary[0], multipart_lines[0].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"prod\"",
               multipart_lines[1].c_str());
  EXPECT_TRUE(multipart_lines[2].empty());
  EXPECT_NE(std::string::npos, multipart_lines[3].find("Chrome"));

  EXPECT_STREQ(&boundary[0], multipart_lines[4].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"ver\"",
               multipart_lines[5].c_str());
  EXPECT_TRUE(multipart_lines[6].empty());
  // Just check that the version contains a dot.
  EXPECT_NE(std::string::npos, multipart_lines[7].find('.'));

  EXPECT_STREQ(&boundary[0], multipart_lines[8].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"guid\"",
               multipart_lines[9].c_str());
  EXPECT_TRUE(multipart_lines[10].empty());
  EXPECT_STREQ("0", multipart_lines[11].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[12].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"type\"",
               multipart_lines[13].c_str());
  EXPECT_TRUE(multipart_lines[14].empty());
  EXPECT_STREQ("webrtc_log", multipart_lines[15].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[16].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"app_session_id\"",
               multipart_lines[17].c_str());
  EXPECT_TRUE(multipart_lines[18].empty());
  EXPECT_STREQ(kTestLoggingSessionIdValue, multipart_lines[19].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[20].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"url\"",
               multipart_lines[21].c_str());
  EXPECT_TRUE(multipart_lines[22].empty());
  EXPECT_STREQ(kTestLoggingUrl, multipart_lines[23].c_str());

  EXPECT_STREQ(&boundary[0], multipart_lines[24].c_str());
  EXPECT_STREQ("Content-Disposition: form-data; name=\"webrtc_log\";"
               " filename=\"webrtc_log.gz\"",
               multipart_lines[25].c_str());
  EXPECT_STREQ("Content-Type: application/gzip",
               multipart_lines[26].c_str());
  EXPECT_TRUE(multipart_lines[27].empty());
  EXPECT_TRUE(multipart_lines[28].empty());  // The removed zip part.
  std::string final_delimiter = boundary;
  final_delimiter += "--";
  EXPECT_STREQ(final_delimiter.c_str(), multipart_lines[29].c_str());
  EXPECT_TRUE(multipart_lines[30].empty());
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopRtpDump) {
  // TODO(tommi): As is, these tests are missing verification of the actual
  // RTP dump data.  We should fix that, e.g. use SetDumpWriterForTesting.
  StartRtpDump(true, true);
  StopRtpDump(true, true);
}

// Tests trying to store a log when a log is not being captured.
// We should get a failure callback in this case.
IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStoreWithoutLog) {
  base::Value::List parameters;
  AppendTabIdAndUrl(parameters);
  parameters.Append("MyLogId");
  scoped_refptr<WebrtcLoggingPrivateStoreFunction> store(
      CreateFunction<WebrtcLoggingPrivateStoreFunction>());
  const std::string error = utils::RunFunctionAndReturnError(
      store.get(), ParamsToString(parameters), GetBrowser()->profile());
  ASSERT_FALSE(error.empty());
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest, TestStartStopStore) {
  ASSERT_TRUE(StartLogging());
  ASSERT_TRUE(StopLogging());
  EXPECT_TRUE(StoreLog("MyLogID"));
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartStopStoreAndUpload) {
  ASSERT_TRUE(SetupTestServerLogUploading());

  static const char kLogId[] = "TestStartStopStoreAndUpload";
  ASSERT_TRUE(StartLogging());
  ASSERT_TRUE(StopLogging());
  ASSERT_TRUE(StoreLog(kLogId));

  std::string report_id;
  EXPECT_TRUE(UploadStoredLog(kLogId, &report_id));
  EXPECT_NE(std::string::npos,
            upload_request_content_.find("filename=\"webrtc_log.gz\""));
  EXPECT_STREQ(kTestReportId, report_id.c_str());
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartStopStoreAndUploadWithRtp) {
  ASSERT_TRUE(SetupTestServerLogUploading());

  static const char kLogId[] = "TestStartStopStoreAndUploadWithRtp";
  ASSERT_TRUE(StartLogging());
  ASSERT_TRUE(StartRtpDump(true, true));
  ASSERT_TRUE(StopLogging());
  ASSERT_TRUE(StopRtpDump(true, true));
  ASSERT_TRUE(StoreLog(kLogId));

  std::string report_id;
  EXPECT_TRUE(UploadStoredLog(kLogId, &report_id));
  EXPECT_NE(std::string::npos,
            upload_request_content_.find("filename=\"webrtc_log.gz\""));
  EXPECT_STREQ(kTestReportId, report_id.c_str());
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartStopStoreAndUploadWithMetaData) {
  ASSERT_TRUE(SetupTestServerLogUploading());

  static const char kLogId[] = "TestStartStopStoreAndUploadWithRtp";
  ASSERT_TRUE(StartLogging());

  base::Value::List parameters;
  AppendTabIdAndUrl(parameters);
  InitializeTestMetaData(parameters);
  SetMetaData(parameters);

  ASSERT_TRUE(StopLogging());
  ASSERT_TRUE(StoreLog(kLogId));

  std::string report_id;
  EXPECT_TRUE(UploadStoredLog(kLogId, &report_id));
  EXPECT_NE(std::string::npos,
            upload_request_content_.find("filename=\"webrtc_log.gz\""));
  EXPECT_NE(std::string::npos, upload_request_content_.find(kTestLoggingUrl));
  EXPECT_STREQ(kTestReportId, report_id.c_str());
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartStopAudioDebugRecordings) {
  // TODO(guidou): These tests are missing verification of the actual AEC dump
  // data. This will be fixed with a separate browser test.
  // See crbug.com/569957.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAudioDebugRecordingsFromExtension);
  ASSERT_TRUE(StartAudioDebugRecordings(0));
  ASSERT_TRUE(StopAudioDebugRecordings());
}

IN_PROC_BROWSER_TEST_F(WebrtcLoggingPrivateApiTest,
                       TestStartTimedAudioDebugRecordings) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAudioDebugRecordingsFromExtension);
  ASSERT_TRUE(StartAudioDebugRecordings(1));
}

#if !BUILDFLAG(IS_ANDROID)

// Fixture for various tests over StartEventLogging. Intended to be sub-classed
// to test different scenarios.
class WebrtcLoggingPrivateApiStartEventLoggingTestBase
    : public WebrtcLoggingPrivateApiTest {
 public:
  ~WebrtcLoggingPrivateApiStartEventLoggingTestBase() override = default;

 protected:
  void SetUp() override {
    SetUpFeatures();
    WebrtcLoggingPrivateApiTest::SetUp();
  }

  void SetUpFeatures() {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;

    if (WebRtcEventLogCollectionFeature()) {
      enabled.push_back(features::kWebRtcRemoteEventLog);
    } else {
      disabled.push_back(features::kWebRtcRemoteEventLog);
    }

    enabled.push_back(features::kWebRtcRemoteEventLogGzipped);

    scoped_feature_list_.InitWithFeatures(enabled, disabled);
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    policy::PolicyMap values;

    values.Set(policy::key::kWebRtcEventLogCollectionAllowed,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::Value(WebRtcEventLogCollectionPolicy()), nullptr);

    provider_.UpdateChromePolicy(values);
  }

  // Whether the test should have WebRTC remote-bound event logging generally
  // enabled (default behavior), or disabled (Finch kill-switch engaged).
  virtual bool WebRtcEventLogCollectionFeature() const = 0;

  // Whether the test should simulate running on a user profile which
  // has the kWebRtcEventLogCollectionAllowed policy configured or not.
  virtual bool WebRtcEventLogCollectionPolicy() const = 0;

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

// Test StartEventLogging's behavior when the feature is active (kill-switch
// from Finch *not* engaged, working in a profile where the policy is
// configured).
class WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled
    : public WebrtcLoggingPrivateApiStartEventLoggingTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ~WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled()
      override = default;

  bool WebRtcEventLogCollectionFeature() const override { return true; }

  bool WebRtcEventLogCollectionPolicy() const override { return true; }
};

// Also covers StartEventLoggingForLegalWebAppIdSucceeds scenario.
IN_PROC_BROWSER_TEST_P(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingForKnownPeerConnectionSucceeds) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  constexpr bool expect_success = true;
  const int output_period_ms = GetParam() ? kMaxOutputPeriodMs : 0;
  StartEventLogging(session_id, max_size_bytes, output_period_ms, kWebAppId,
                    expect_success);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingWithUnlimitedSizeFails) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);
  const int max_size_bytes = kWebRtcEventLogManagerUnlimitedFileSize;
  constexpr bool expect_success = false;
  const std::string error_message =
      kStartRemoteLoggingFailureUnlimitedSizeDisallowed;
  StartEventLogging(session_id, max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingWithTooSmallMaxSize) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);
  const int max_size_bytes = 1;
  constexpr bool expect_success = false;
  const std::string error_message = kStartRemoteLoggingFailureMaxSizeTooSmall;
  StartEventLogging(session_id, max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingWithExcessiveMaxSizeFails) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes + 1;
  constexpr bool expect_success = false;
  const std::string error_message = kStartRemoteLoggingFailureMaxSizeTooLarge;
  StartEventLogging(session_id, max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingWithTooLargeOutputPeriodMsFails) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);
  const int output_period_ms = kMaxOutputPeriodMs + 1;
  constexpr bool expect_success = false;
  const std::string error_message =
      kStartRemoteLoggingFailureOutputPeriodMsTooLarge;
  StartEventLogging(session_id, kMaxRemoteLogFileSizeBytes, output_period_ms,
                    kWebAppId, expect_success, error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingForNeverAddedPeerConnectionFails) {
  // Note that manager->OnPeerConnectionAdded() is not called.
  const std::string session_id = "id";
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  constexpr bool expect_success = false;
  const std::string error_message =
      kStartRemoteLoggingFailureUnknownOrInactivePeerConnection;
  StartEventLogging(session_id, max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingForWrongSessionIdFails) {
  const std::string session_id_1 = "id1";
  const std::string session_id_2 = "id2";

  SetUpPeerConnection(session_id_1);
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  constexpr bool expect_success = false;
  const std::string error_message =
      kStartRemoteLoggingFailureUnknownOrInactivePeerConnection;
  StartEventLogging(session_id_2, max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingIfSessionIdNeverSetFails) {
  SetUpPeerConnection();  // Note lack of session ID.
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  constexpr bool expect_success = false;
  const std::string error_message =
      kStartRemoteLoggingFailureUnknownOrInactivePeerConnection;
  StartEventLogging("session_id", max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingIfSessionIdNeverSetFailsForEmptySessionId) {
  SetUpPeerConnection();  // Note lack of session ID.
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  constexpr bool expect_success = false;
  const std::string error_message =
      kStartRemoteLoggingFailureUnknownOrInactivePeerConnection;
  StartEventLogging("", max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLogginWithEmptySessionIdFails) {
  SetUpPeerConnection("session_id");
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  constexpr bool expect_success = false;
  const std::string error_message =
      kStartRemoteLoggingFailureUnknownOrInactivePeerConnection;
  StartEventLogging("", max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingForAlreadyLoggedPeerConnectionFails) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);

  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;

  // First call succeeds.
  {
    constexpr bool expect_success = true;
    StartEventLogging(session_id, max_size_bytes, 0, kWebAppId, expect_success);
  }

  // Second call fails.
  {
    constexpr bool expect_success = false;
    const std::string error_message = kStartRemoteLoggingFailureAlreadyLogging;
    StartEventLogging(session_id, max_size_bytes, 0, kWebAppId, expect_success,
                      error_message);
  }
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingForTooLowWebAppIdFails) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  const size_t web_app_id =
      webrtc_event_logging::kMinWebRtcEventLogWebAppId - 1;
  ASSERT_LT(web_app_id, webrtc_event_logging::kMinWebRtcEventLogWebAppId);
  constexpr bool expect_success = false;
  const std::string error_message =
      webrtc_event_logging::kStartRemoteLoggingFailureIllegalWebAppId;
  StartEventLogging(session_id, max_size_bytes, 0, web_app_id, expect_success,
                    error_message);
}

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    StartEventLoggingForTooHighWebAppIdFails) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  const size_t web_app_id =
      webrtc_event_logging::kMaxWebRtcEventLogWebAppId + 1;
  ASSERT_GT(web_app_id, webrtc_event_logging::kMaxWebRtcEventLogWebAppId);
  constexpr bool expect_success = false;
  const std::string error_message =
      webrtc_event_logging::kStartRemoteLoggingFailureIllegalWebAppId;
  StartEventLogging(session_id, max_size_bytes, 0, web_app_id, expect_success,
                    error_message);
}

INSTANTIATE_TEST_SUITE_P(
    _,
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureAndPolicyEnabled,
    ::testing::Bool());

// Testing with either the feature or the policy disabled (not both).
class WebrtcLoggingPrivateApiStartEventLoggingTestFeatureOrPolicyDisabled
    : public WebrtcLoggingPrivateApiStartEventLoggingTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  WebrtcLoggingPrivateApiStartEventLoggingTestFeatureOrPolicyDisabled()
      : feature_enabled_(GetParam()), policy_enabled_(!feature_enabled_) {}

  ~WebrtcLoggingPrivateApiStartEventLoggingTestFeatureOrPolicyDisabled()
      override = default;

 protected:
  bool WebRtcEventLogCollectionFeature() const override {
    return feature_enabled_;
  }

  bool WebRtcEventLogCollectionPolicy() const override {
    return policy_enabled_;
  }

 private:
  const bool feature_enabled_;
  const bool policy_enabled_;
};

IN_PROC_BROWSER_TEST_P(
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureOrPolicyDisabled,
    StartEventLoggingFails) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  constexpr bool expect_success = false;
  const std::string error_message = kStartRemoteLoggingFailureFeatureDisabled;
  StartEventLogging(session_id, max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

INSTANTIATE_TEST_SUITE_P(
    FeatureEnabled,
    WebrtcLoggingPrivateApiStartEventLoggingTestFeatureOrPolicyDisabled,
    ::testing::Bool());

// Make sure that, even if both the feature and the policy enable remote-bound
// event logging, it will be blocked for incognito sessions.
class WebrtcLoggingPrivateApiStartEventLoggingTestInIncognitoMode
    : public WebrtcLoggingPrivateApiStartEventLoggingTestBase {
 public:
  ~WebrtcLoggingPrivateApiStartEventLoggingTestInIncognitoMode() override =
      default;

 protected:
  Browser* GetBrowser() override {
    if (!browser_) {
      browser_ = CreateIncognitoBrowser();
    }
    return browser_;
  }

  bool WebRtcEventLogCollectionFeature() const override { return true; }

  bool WebRtcEventLogCollectionPolicy() const override { return true; }

 private:
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser_{
      nullptr};  // Does not own the object.
};

IN_PROC_BROWSER_TEST_F(
    WebrtcLoggingPrivateApiStartEventLoggingTestInIncognitoMode,
    StartEventLoggingFails) {
  const std::string session_id = "id";
  SetUpPeerConnection(session_id);
  const int max_size_bytes = kMaxRemoteLogFileSizeBytes;
  constexpr bool expect_success = false;
  const std::string error_message = kStartRemoteLoggingFailureFeatureDisabled;
  StartEventLogging(session_id, max_size_bytes, 0, kWebAppId, expect_success,
                    error_message);
}

#endif  // !BUILDFLAG(IS_ANDROID)
