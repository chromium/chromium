// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_http.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/printing/cloud_print/privet_http_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/pwg_raster_converter.h"
#include "printing/mojom/print.mojom.h"
#include "printing/pwg_raster_settings.h"
#endif

namespace cloud_print {

namespace {

using content::BrowserThread;
using net::EmbeddedTestServer;
using testing::Mock;
using testing::NiceMock;
using testing::StrictMock;
using testing::TestWithParam;
using testing::ValuesIn;
using testing::_;

const char kSampleInfoResponse[] =
    R"({
         "version": "1.0",
         "name": "Common printer",
         "description": "Printer connected through Chrome connector",
         "url": "https://www.google.com/cloudprint",
         "type": [ "printer" ],
         "id": "",
         "device_state": "idle",
         "connection_state": "online",
         "manufacturer": "Google",
         "model": "Google Chrome",
         "serial_number": "1111-22222-33333-4444",
         "firmware": "24.0.1312.52",
         "uptime": 600,
         "setup_url": "http://support.google.com/",
         "support_url": "http://support.google.com/cloudprint/?hl=en",
         "update_url": "http://support.google.com/cloudprint/?hl=en",
         "x-privet-token": "SampleTokenForTesting",
         "api": [
           "/privet/accesstoken",
           "/privet/capabilities",
           "/privet/printer/submitdoc",
         ]
       })";

const char kSampleInfoResponseRegistered[] =
    R"({
         "version": "1.0",
         "name": "Common printer",
         "description": "Printer connected through Chrome connector",
         "url": "https://www.google.com/cloudprint",
         "type": [ "printer" ],
         "id": "MyDeviceID",
         "device_state": "idle",
         "connection_state": "online",
         "manufacturer": "Google",
         "model": "Google Chrome",
         "serial_number": "1111-22222-33333-4444",
         "firmware": "24.0.1312.52",
         "uptime": 600,
         "setup_url": "http://support.google.com/",
         "support_url": "http://support.google.com/cloudprint/?hl=en",
         "update_url": "http://support.google.com/cloudprint/?hl=en",
         "x-privet-token": "SampleTokenForTesting",
         "api": [
           "/privet/accesstoken",
           "/privet/capabilities",
           "/privet/printer/submitdoc",
         ]
       })";

const char kSampleRegisterStartResponse[] =
    R"({
         "user": "example@google.com",
         "action": "start"
       })";

const char kSampleRegisterGetClaimTokenResponse[] =
    R"({
         "action": "getClaimToken",
         "user": "example@google.com",
         "token": "MySampleToken",
         "claim_url": "https://domain.com/SoMeUrL"
       })";

const char kSampleRegisterCompleteResponse[] =
    R"({
         "user": "example@google.com",
         "action": "complete",
         "device_id": "MyDeviceID"
       })";

const char kSampleXPrivetErrorResponse[] =
    R"({ "error": "invalid_x_privet_token" })";

const char kSampleRegisterErrorTransient[] =
    R"({ "error": "device_busy", "timeout": 1})";

const char kSampleRegisterErrorPermanent[] =
    R"({ "error": "user_cancel" })";

const char kSampleInfoResponseBadJson[] = "{";

const char kSampleRegisterCancelResponse[] =
    R"({
         "user": "example@google.com",
         "action": "cancel"
       })";

const char kSampleCapabilitiesResponse[] =
    R"({
         "version" : "1.0",
         "printer" : {
           "supported_content_type" : [
             { "content_type" : "application/pdf" },
             { "content_type" : "image/pwg-raster" }
           ]
         }
       })";

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
const char kSampleInfoResponseWithCreatejob[] =
    R"({
         "version": "1.0",
         "name": "Common printer",
         "description": "Printer connected through Chrome connector",
         "url": "https://www.google.com/cloudprint",
         "type": [ "printer" ],
         "id": "",
         "device_state": "idle",
         "connection_state": "online",
         "manufacturer": "Google",
         "model": "Google Chrome",
         "serial_number": "1111-22222-33333-4444",
         "firmware": "24.0.1312.52",
         "uptime": 600,
         "setup_url": "http://support.google.com/",
         "support_url": "http://support.google.com/cloudprint/?hl=en",
         "update_url": "http://support.google.com/cloudprint/?hl=en",
         "x-privet-token": "SampleTokenForTesting",
         "api": [
           "/privet/accesstoken",
           "/privet/capabilities",
           "/privet/printer/createjob",
           "/privet/printer/submitdoc",
         ]
       })";

const char kSampleLocalPrintResponse[] =
    R"({
         "job_id": "123",
         "expires_in": 500,
         "job_type": "application/pdf",
         "job_size": 16,
         "job_name": "Sample job name",
       })";

const char kSampleCapabilitiesResponsePWGOnly[] =
    R"({
         "version" : "1.0",
         "printer" : {
           "supported_content_type" : [
              { "content_type" : "image/pwg-raster" }
           ]
         }
       })";

const char kSampleErrorResponsePrinterBusy[] =
    R"({
         "error": "invalid_print_job",
         "timeout": 1
       })";

const char kSampleInvalidDocumentTypeResponse[] =
    R"({ "error" : "invalid_document_type" })";

const char kSampleCreateJobResponse[] = R"({ "job_id": "1234" })";

const char kSampleCapabilitiesResponseWithAnyMimetype[] =
    R"({
         "version" : "1.0",
         "printer" : {
           "supported_content_type" : [
             { "content_type" : "*/*" },
             { "content_type" : "image/pwg-raster" }
           ]
         }
       })";

const char kSampleCJT[] = R"({ "version" : "1.0" })";

const char kSampleCapabilitiesResponsePWGSettings[] =
    R"({
         "version" : "1.0",
         "printer" : {
           "pwg_raster_config" : {
             "document_sheet_back" : "MANUAL_TUMBLE",
             "reverse_order_streaming": true
           },
           "supported_content_type" : [
             { "content_type" : "image/pwg-raster" }
           ]
         }
       })";

const char kSampleCapabilitiesResponsePWGSettingsMono[] =
    R"({
         "version": "1.0",
         "printer": {
           "pwg_raster_config": {
             "document_type_supported": [ "SGRAY_8" ],
             "document_sheet_back": "ROTATED"
           }
         }
       })";

const char kSampleCJTDuplex[] =
    R"({
         "version" : "1.0",
         "print": { "duplex": {"type": "SHORT_EDGE"} }
       })";

const char kSampleCJTMono[] =
    R"({
         "version" : "1.0",
         "print": { "color": {"type": "STANDARD_MONOCHROME"} }
       })";
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

const char* const kTestParams[] = {"8.8.4.4", "2001:4860:4860::8888"};

// Returns the representation of the given JSON that would be outputted by
// JSONWriter. This ensures the same JSON values are represented by the same
// string.
std::string NormalizeJson(const std::string& json) {
  std::string result = json;
  base::Optional<base::Value> value = base::JSONReader::Read(result);
  DCHECK(value) << result;
  base::JSONWriter::Write(*value, &result);
  return result;
}

class PrivetHTTPTest : public TestWithParam<const char*> {
 public:
  PrivetHTTPTest()
      : kInfoURL(GetUrl("/privet/info")),
        kRegisterStartURL(
            GetUrl("/privet/register?action=start&user=example%40google.com")),
        kRegisterGetTokenURL(GetUrl(
            "/privet/register?action=getClaimToken&user=example%40google.com")),
        kRegisterCompleteURL(GetUrl(
            "/privet/register?action=complete&user=example%40google.com")),
        kCapabilitiesURL(GetUrl("/privet/capabilities")),
        kSubmitDocURL(GetUrl("/privet/printer/"
                             "submitdoc?client_name=Chrome&user_name=sample%"
                             "40gmail.com&job_name=Sample+job+name")),
        kSubmitDocWithJobIDURL(
            GetUrl("/privet/printer/"
                   "submitdoc?client_name=Chrome&user_name=sample%40gmail.com&"
                   "job_name=Sample+job+name&job_id=1234")),
        kCreateJobURL(GetUrl("/privet/printer/createjob")),
        test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    PrivetURLLoader::ResetTokenMapForTest();

    auto privet_http_client_impl = std::make_unique<PrivetHTTPClientImpl>(
        "sampleDevice._privet._tcp.local", net::HostPortPair(GetParam(), 6006),
        test_shared_url_loader_factory_);
    privet_client_ =
        PrivetV1HTTPClient::CreateDefault(std::move(privet_http_client_impl));

    test_url_loader_factory_.SetInterceptor(base::BindRepeating(
        &PrivetHTTPTest::InterceptURL, base::Unretained(this)));
  }

  GURL GetUrl(const std::string& path) const {
    std::string host = GetParam();
    if (host.find(":") != std::string::npos)
      host = "[" + host + "]";
    return GURL("http://" + host + ":6006" + path);
  }

 protected:
  void InterceptURL(const network::ResourceRequest& request) {
    url_to_resource_requests_[request.url].push_back(request);
  }

  bool SuccessfulResponse(const GURL& request_url,
                          std::string content,
                          net::HttpStatusCode http_status = net::HTTP_OK) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        request_url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(http_status), content);
  }

  std::string GetUploadDataAsNormalizedJSON(const GURL& url) {
    std::string data = GetUploadData(url);
    if (data.empty())
      return data;
    return NormalizeJson(data);
  }

  std::string GetUploadData(const GURL& url) {
    auto it = url_to_resource_requests_.find(url);
    if (it == url_to_resource_requests_.end())
      return std::string();
    const std::vector<network::ResourceRequest>& resource_requests = it->second;
    DCHECK(!resource_requests.empty());

    const network::ResourceRequest& resource_request = resource_requests[0];
    return network::GetUploadData(resource_request);
  }

  const GURL kInfoURL;
  const GURL kRegisterStartURL;
  const GURL kRegisterGetTokenURL;
  const GURL kRegisterCompleteURL;
  const GURL kCapabilitiesURL;
  const GURL kSubmitDocURL;
  const GURL kSubmitDocWithJobIDURL;
  const GURL kCreateJobURL;

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::unique_ptr<PrivetV1HTTPClient> privet_client_;
  std::map<GURL, std::vector<network::ResourceRequest>>
      url_to_resource_requests_;
};

class MockJSONCallback{
 public:
  void OnPrivetJSONDone(const base::DictionaryValue* value) {
    value_.reset(value ? value->DeepCopy() : nullptr);
    OnPrivetJSONDoneInternal();
  }

  MOCK_METHOD0(OnPrivetJSONDoneInternal, void());

  const base::DictionaryValue* value() { return value_.get(); }
  PrivetJSONOperation::ResultCallback callback() {
    return base::BindOnce(&MockJSONCallback::OnPrivetJSONDone,
                          base::Unretained(this));
  }
 protected:
  std::unique_ptr<base::DictionaryValue> value_;
};

class MockRegisterDelegate : public PrivetRegisterOperation::Delegate {
 public:
  MOCK_METHOD3(OnPrivetRegisterClaimToken,
               void(PrivetRegisterOperation* operation,
                    const std::string& token,
                    const GURL& url));

  void OnPrivetRegisterError(
      PrivetRegisterOperation* operation,
      const std::string& action,
      PrivetRegisterOperation::FailureReason reason,
      int printer_http_code,
      const base::DictionaryValue* json) override {
    // TODO(noamsml): Save and test for JSON?
    OnPrivetRegisterErrorInternal(action, reason, printer_http_code);
  }

  MOCK_METHOD3(OnPrivetRegisterErrorInternal,
               void(const std::string& action,
                    PrivetRegisterOperation::FailureReason reason,
                    int printer_http_code));

  MOCK_METHOD2(OnPrivetRegisterDone,
               void(PrivetRegisterOperation* operation,
                    const std::string& device_id));
};

class MockLocalPrintDelegate : public PrivetLocalPrintOperation::Delegate {
 public:
  MOCK_METHOD1(OnPrivetPrintingDone, void(const PrivetLocalPrintOperation*));
  MOCK_METHOD2(OnPrivetPrintingError,
               void(const PrivetLocalPrintOperation* print_operation,
                    int http_code));
};

class PrivetInfoTest : public PrivetHTTPTest {
 public:
  void SetUp() override {
    info_operation_ = privet_client_->CreateInfoOperation(
        info_callback_.callback());
  }

 protected:
  std::unique_ptr<PrivetJSONOperation> info_operation_;
  StrictMock<MockJSONCallback> info_callback_;
};

INSTANTIATE_TEST_SUITE_P(PrivetTests, PrivetInfoTest, ValuesIn(kTestParams));

TEST_P(PrivetInfoTest, SuccessfulInfo) {
  info_operation_->Start();

  EXPECT_CALL(info_callback_, OnPrivetJSONDoneInternal());
  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));
}

TEST_P(PrivetInfoTest, InfoFailureHTTP) {
  info_operation_->Start();

  EXPECT_CALL(info_callback_, OnPrivetJSONDoneInternal());
  EXPECT_TRUE(
      SuccessfulResponse(kInfoURL, kSampleInfoResponse, net::HTTP_NOT_FOUND));
}

class PrivetRegisterTest : public PrivetHTTPTest {
 protected:
  void SetUp() override {
    info_operation_ = privet_client_->CreateInfoOperation(
        info_callback_.callback());
    register_operation_ =
        privet_client_->CreateRegisterOperation("example@google.com",
                                                &register_delegate_);
  }

  std::unique_ptr<PrivetJSONOperation> info_operation_;
  NiceMock<MockJSONCallback> info_callback_;
  std::unique_ptr<PrivetRegisterOperation> register_operation_;
  StrictMock<MockRegisterDelegate> register_delegate_;
  PrivetURLLoader::RetryImmediatelyForTest retry_immediately_;
};

INSTANTIATE_TEST_SUITE_P(PrivetTests,
                         PrivetRegisterTest,
                         ValuesIn(kTestParams));

TEST_P(PrivetRegisterTest, RegisterSuccessSimple) {
  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponse(kRegisterStartURL, kSampleRegisterStartResponse));

  EXPECT_CALL(register_delegate_,
              OnPrivetRegisterClaimToken(_, "MySampleToken",
                                         GURL("https://domain.com/SoMeUrL")));

  EXPECT_TRUE(SuccessfulResponse(kRegisterGetTokenURL,
                                 kSampleRegisterGetClaimTokenResponse));

  register_operation_->CompleteRegistration();

  EXPECT_TRUE(SuccessfulResponse(kRegisterCompleteURL,
                                 kSampleRegisterCompleteResponse));

  EXPECT_CALL(register_delegate_, OnPrivetRegisterDone(_, "MyDeviceID"));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponseRegistered));
}

TEST_P(PrivetRegisterTest, RegisterXSRFFailure) {
  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponse(kRegisterStartURL, kSampleRegisterStartResponse));

  EXPECT_TRUE(
      SuccessfulResponse(kRegisterGetTokenURL, kSampleXPrivetErrorResponse));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_CALL(register_delegate_,
              OnPrivetRegisterClaimToken(_, "MySampleToken",
                                         GURL("https://domain.com/SoMeUrL")));

  EXPECT_TRUE(SuccessfulResponse(kRegisterGetTokenURL,
                                 kSampleRegisterGetClaimTokenResponse));
}

TEST_P(PrivetRegisterTest, TransientFailure) {
  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  // Make the registration request fail the first time and work after that.
  EXPECT_TRUE(
      SuccessfulResponse(kRegisterStartURL, kSampleRegisterErrorTransient));

  EXPECT_TRUE(
      SuccessfulResponse(kRegisterStartURL, kSampleRegisterStartResponse));
}

TEST_P(PrivetRegisterTest, PermanentFailure) {
  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponse(kRegisterStartURL, kSampleRegisterStartResponse));

  EXPECT_CALL(
      register_delegate_,
      OnPrivetRegisterErrorInternal(
          "getClaimToken", PrivetRegisterOperation::FAILURE_JSON_ERROR, 200));

  EXPECT_TRUE(
      SuccessfulResponse(kRegisterGetTokenURL, kSampleRegisterErrorPermanent));
}

TEST_P(PrivetRegisterTest, InfoFailure) {
  register_operation_->Start();

  EXPECT_CALL(register_delegate_,
              OnPrivetRegisterErrorInternal(
                  "start",
                  PrivetRegisterOperation::FAILURE_TOKEN,
                  -1));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponseBadJson));
}

TEST_P(PrivetRegisterTest, RegisterCancel) {
  register_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponse(kRegisterStartURL, kSampleRegisterStartResponse));

  register_operation_->Cancel();
  EXPECT_TRUE(SuccessfulResponse(
      GetUrl("/privet/register?action=cancel&user=example%40google.com"),
      kSampleRegisterCancelResponse));
}

class PrivetCapabilitiesTest : public PrivetHTTPTest {
 public:
  void SetUp() override {
    capabilities_operation_ = privet_client_->CreateCapabilitiesOperation(
        capabilities_callback_.callback());
  }

 protected:
  std::unique_ptr<PrivetJSONOperation> capabilities_operation_;
  StrictMock<MockJSONCallback> capabilities_callback_;
};

INSTANTIATE_TEST_SUITE_P(PrivetTests,
                         PrivetCapabilitiesTest,
                         ValuesIn(kTestParams));

TEST_P(PrivetCapabilitiesTest, SuccessfulCapabilities) {
  capabilities_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_CALL(capabilities_callback_, OnPrivetJSONDoneInternal());

  EXPECT_TRUE(
      SuccessfulResponse(kCapabilitiesURL, kSampleCapabilitiesResponse));

  std::string version;
  EXPECT_TRUE(capabilities_callback_.value()->GetString("version", &version));
  EXPECT_EQ("1.0", version);
}

TEST_P(PrivetCapabilitiesTest, CacheToken) {
  capabilities_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_CALL(capabilities_callback_, OnPrivetJSONDoneInternal());

  EXPECT_TRUE(
      SuccessfulResponse(kCapabilitiesURL, kSampleCapabilitiesResponse));

  capabilities_operation_ = privet_client_->CreateCapabilitiesOperation(
      capabilities_callback_.callback());

  capabilities_operation_->Start();

  EXPECT_CALL(capabilities_callback_, OnPrivetJSONDoneInternal());

  EXPECT_TRUE(
      SuccessfulResponse(kCapabilitiesURL, kSampleCapabilitiesResponse));
}

TEST_P(PrivetCapabilitiesTest, BadToken) {
  capabilities_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(
      SuccessfulResponse(kCapabilitiesURL, kSampleXPrivetErrorResponse));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_CALL(capabilities_callback_, OnPrivetJSONDoneInternal());

  EXPECT_TRUE(
      SuccessfulResponse(kCapabilitiesURL, kSampleCapabilitiesResponse));
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// A note on PWG raster conversion: The fake PWG raster converter simply returns
// the input as the converted data. The output isn't checked anyway.
// converts strings to file paths based on them by appending "test.pdf", since
// it's easier to test that way. Instead of using a mock, we simply check if the
// request is uploading a file that is based on this pattern.
class FakePwgRasterConverter : public printing::PwgRasterConverter {
 public:
  void Start(const base::RefCountedMemory* data,
             const printing::PdfRenderSettings& conversion_settings,
             const printing::PwgRasterSettings& bitmap_settings,
             ResultCallback callback) override {
    base::MappedReadOnlyRegion memory =
        base::ReadOnlySharedMemoryRegion::Create(data->size());
    if (!memory.mapping.IsValid()) {
      ADD_FAILURE() << "Failed to create pwg raster shared memory.";
      std::move(callback).Run(base::ReadOnlySharedMemoryRegion());
      return;
    }

    memcpy(memory.mapping.memory(), data->front(), data->size());
    bitmap_settings_ = bitmap_settings;
    std::move(callback).Run(std::move(memory.region));
  }

  const printing::PwgRasterSettings& bitmap_settings() {
    return bitmap_settings_;
  }

 private:
  printing::PwgRasterSettings bitmap_settings_;
};

class PrivetLocalPrintTest : public PrivetHTTPTest {
 public:
  void SetUp() override {
    PrivetURLLoader::ResetTokenMapForTest();

    local_print_operation_ = privet_client_->CreateLocalPrintOperation(
        &local_print_delegate_);

    auto pwg_converter = std::make_unique<FakePwgRasterConverter>();
    pwg_converter_ = pwg_converter.get();
    local_print_operation_->SetPwgRasterConverterForTesting(
        std::move(pwg_converter));
  }

  scoped_refptr<base::RefCountedBytes> RefCountedBytesFromString(
      base::StringPiece str) {
    std::vector<unsigned char> str_vec;
    str_vec.insert(str_vec.begin(), str.begin(), str.end());
    return base::RefCountedBytes::TakeVector(&str_vec);
  }

 protected:
  std::unique_ptr<PrivetLocalPrintOperation> local_print_operation_;
  StrictMock<MockLocalPrintDelegate> local_print_delegate_;
  FakePwgRasterConverter* pwg_converter_;
  PrivetLocalPrintOperationImpl::RunTasksImmediatelyForTesting
      run_tasks_immediately_for_local_print_;
};

INSTANTIATE_TEST_SUITE_P(PrivetTests,
                         PrivetLocalPrintTest,
                         ValuesIn(kTestParams));

TEST_P(PrivetLocalPrintTest, SuccessfulLocalPrint) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(RefCountedBytesFromString(
      "Sample print data"));
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDone(_));

  EXPECT_TRUE(SuccessfulResponse(kSubmitDocURL, kSampleLocalPrintResponse));
  EXPECT_EQ("Sample print data", GetUploadData(kSubmitDocURL));
}

TEST_P(PrivetLocalPrintTest, SuccessfulLocalPrintWithAnyMimetype) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(
      RefCountedBytesFromString("Sample print data"));
  local_print_operation_->SetCapabilities(
      kSampleCapabilitiesResponseWithAnyMimetype);
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDone(_));

  EXPECT_TRUE(SuccessfulResponse(kSubmitDocURL, kSampleLocalPrintResponse));
  EXPECT_EQ("Sample print data", GetUploadData(kSubmitDocURL));
}

TEST_P(PrivetLocalPrintTest, SuccessfulPWGLocalPrint) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(RefCountedBytesFromString("foobar"));
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponsePWGOnly);
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDone(_));

  EXPECT_TRUE(SuccessfulResponse(kSubmitDocURL, kSampleLocalPrintResponse));
  EXPECT_EQ("foobar", GetUploadData(kSubmitDocURL));

  EXPECT_EQ(printing::mojom::DuplexMode::kSimplex,
            pwg_converter_->bitmap_settings().duplex_mode);
  EXPECT_EQ(printing::TRANSFORM_NORMAL,
            pwg_converter_->bitmap_settings().odd_page_transform);
  EXPECT_FALSE(pwg_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_FALSE(pwg_converter_->bitmap_settings().reverse_page_order);

  // Defaults to true when the color is not specified.
  EXPECT_TRUE(pwg_converter_->bitmap_settings().use_color);
}

TEST_P(PrivetLocalPrintTest, SuccessfulPWGLocalPrintDuplex) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(RefCountedBytesFromString("foobar"));
  base::Optional<base::Value> ticket = base::JSONReader::Read(kSampleCJTDuplex);
  ASSERT_TRUE(ticket);
  local_print_operation_->SetTicket(std::move(*ticket));
  local_print_operation_->SetCapabilities(
      kSampleCapabilitiesResponsePWGSettings);
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kCreateJobURL, kSampleCreateJobResponse));
  EXPECT_EQ(NormalizeJson(kSampleCJTDuplex),
            GetUploadDataAsNormalizedJSON(kCreateJobURL));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDone(_));

  EXPECT_TRUE(
      SuccessfulResponse(kSubmitDocWithJobIDURL, kSampleLocalPrintResponse));
  EXPECT_EQ("foobar", GetUploadData(kSubmitDocWithJobIDURL));

  EXPECT_EQ(printing::mojom::DuplexMode::kShortEdge,
            pwg_converter_->bitmap_settings().duplex_mode);
  EXPECT_EQ(printing::TRANSFORM_ROTATE_180,
            pwg_converter_->bitmap_settings().odd_page_transform);
  EXPECT_FALSE(pwg_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_TRUE(pwg_converter_->bitmap_settings().reverse_page_order);

  // Defaults to true when the color is not specified.
  EXPECT_TRUE(pwg_converter_->bitmap_settings().use_color);
}

TEST_P(PrivetLocalPrintTest, SuccessfulPWGLocalPrintMono) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(RefCountedBytesFromString("foobar"));
  base::Optional<base::Value> ticket = base::JSONReader::Read(kSampleCJTMono);
  ASSERT_TRUE(ticket);
  local_print_operation_->SetTicket(std::move(*ticket));
  local_print_operation_->SetCapabilities(
      kSampleCapabilitiesResponsePWGSettings);
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kCreateJobURL, kSampleCreateJobResponse));
  EXPECT_EQ(NormalizeJson(kSampleCJTMono),
            GetUploadDataAsNormalizedJSON(kCreateJobURL));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDone(_));

  EXPECT_TRUE(
      SuccessfulResponse(kSubmitDocWithJobIDURL, kSampleLocalPrintResponse));
  EXPECT_EQ("foobar", GetUploadData(kSubmitDocWithJobIDURL));

  EXPECT_EQ(printing::TRANSFORM_NORMAL,
            pwg_converter_->bitmap_settings().odd_page_transform);
  EXPECT_FALSE(pwg_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_TRUE(pwg_converter_->bitmap_settings().reverse_page_order);

  // Ticket specified mono, but no SGRAY_8 color capability.
  EXPECT_TRUE(pwg_converter_->bitmap_settings().use_color);
}

TEST_P(PrivetLocalPrintTest, SuccessfulPWGLocalPrintMonoToGRAY8Printer) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  local_print_operation_->SetData(RefCountedBytesFromString("foobar"));
  base::Optional<base::Value> ticket = base::JSONReader::Read(kSampleCJTMono);
  ASSERT_TRUE(ticket);
  local_print_operation_->SetTicket(std::move(*ticket));
  local_print_operation_->SetCapabilities(
      kSampleCapabilitiesResponsePWGSettingsMono);
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kCreateJobURL, kSampleCreateJobResponse));
  EXPECT_EQ(NormalizeJson(kSampleCJTMono),
            GetUploadDataAsNormalizedJSON(kCreateJobURL));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDone(_));

  EXPECT_TRUE(
      SuccessfulResponse(kSubmitDocWithJobIDURL, kSampleLocalPrintResponse));
  EXPECT_EQ("foobar", GetUploadData(kSubmitDocWithJobIDURL));

  EXPECT_EQ(printing::TRANSFORM_NORMAL,
            pwg_converter_->bitmap_settings().odd_page_transform);
  EXPECT_FALSE(pwg_converter_->bitmap_settings().rotate_all_pages);
  EXPECT_FALSE(pwg_converter_->bitmap_settings().reverse_page_order);

  // Ticket specified mono, and SGRAY_8 color capability exists.
  EXPECT_FALSE(pwg_converter_->bitmap_settings().use_color);
}

TEST_P(PrivetLocalPrintTest, SuccessfulLocalPrintWithCreatejob) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  base::Optional<base::Value> ticket = base::JSONReader::Read(kSampleCJT);
  ASSERT_TRUE(ticket);
  local_print_operation_->SetTicket(std::move(*ticket));
  local_print_operation_->SetData(
      RefCountedBytesFromString("Sample print data"));
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kCreateJobURL, kSampleCreateJobResponse));
  EXPECT_EQ(NormalizeJson(kSampleCJT),
            GetUploadDataAsNormalizedJSON(kCreateJobURL));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDone(_));

  EXPECT_TRUE(
      SuccessfulResponse(kSubmitDocWithJobIDURL, kSampleLocalPrintResponse));
  EXPECT_EQ("Sample print data", GetUploadData(kSubmitDocWithJobIDURL));
}

TEST_P(PrivetLocalPrintTest, SuccessfulLocalPrintWithOverlongName) {
  const GURL kSubmitDocURL = GetUrl(
      "/privet/printer/"
      "submitdoc?client_name=Chrome&user_name=sample%40gmail.com&job_name="
      "123456789%3A123456789%3A123456789%3A1...123456789%3A123456789%"
      "3A123456789%3A&job_id=1234");

  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname(
      "123456789:123456789:123456789:123456789:123456789:123456789:123456789:");
  base::Optional<base::Value> ticket = base::JSONReader::Read(kSampleCJT);
  ASSERT_TRUE(ticket);
  local_print_operation_->SetTicket(std::move(*ticket));
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->SetData(
      RefCountedBytesFromString("Sample print data"));
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kCreateJobURL, kSampleCreateJobResponse));
  EXPECT_EQ(NormalizeJson(kSampleCJT),
            GetUploadDataAsNormalizedJSON(kCreateJobURL));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDone(_));

  EXPECT_TRUE(SuccessfulResponse(kSubmitDocURL, kSampleLocalPrintResponse));
  EXPECT_EQ("Sample print data", GetUploadData(kSubmitDocURL));
}

TEST_P(PrivetLocalPrintTest, PDFPrintInvalidDocumentTypeRetry) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  base::Optional<base::Value> ticket = base::JSONReader::Read(kSampleCJT);
  ASSERT_TRUE(ticket);
  local_print_operation_->SetTicket(std::move(*ticket));
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->SetData(RefCountedBytesFromString("sample_data"));
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kCreateJobURL, kSampleCreateJobResponse));
  EXPECT_EQ(NormalizeJson(kSampleCJT),
            GetUploadDataAsNormalizedJSON(kCreateJobURL));

  EXPECT_TRUE(SuccessfulResponse(kSubmitDocWithJobIDURL,
                                 kSampleInvalidDocumentTypeResponse));
  EXPECT_EQ("sample_data", GetUploadData(kSubmitDocWithJobIDURL));

  EXPECT_CALL(local_print_delegate_, OnPrivetPrintingDone(_));

  EXPECT_TRUE(
      SuccessfulResponse(kSubmitDocWithJobIDURL, kSampleLocalPrintResponse));
  EXPECT_EQ("sample_data", GetUploadData(kSubmitDocWithJobIDURL));
}

TEST_P(PrivetLocalPrintTest, LocalPrintRetryOnInvalidJobID) {
  local_print_operation_->SetUsername("sample@gmail.com");
  local_print_operation_->SetJobname("Sample job name");
  base::Optional<base::Value> ticket = base::JSONReader::Read(kSampleCJT);
  ASSERT_TRUE(ticket);
  local_print_operation_->SetTicket(std::move(*ticket));
  local_print_operation_->SetCapabilities(kSampleCapabilitiesResponse);
  local_print_operation_->SetData(
      RefCountedBytesFromString("Sample print data"));
  local_print_operation_->Start();

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponseWithCreatejob));

  EXPECT_TRUE(SuccessfulResponse(kInfoURL, kSampleInfoResponse));

  EXPECT_TRUE(SuccessfulResponse(kCreateJobURL, kSampleCreateJobResponse));
  EXPECT_EQ(NormalizeJson(kSampleCJT),
            GetUploadDataAsNormalizedJSON(kCreateJobURL));

  EXPECT_TRUE(SuccessfulResponse(kSubmitDocWithJobIDURL,
                                 kSampleErrorResponsePrinterBusy));
  EXPECT_EQ("Sample print data", GetUploadData(kSubmitDocWithJobIDURL));

  EXPECT_TRUE(SuccessfulResponse(kCreateJobURL, kSampleCreateJobResponse));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

class PrivetHttpWithServerTest : public ::testing::Test {
 protected:
  PrivetHttpWithServerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()) {}

  void SetUp() override {
    server_ =
        std::make_unique<EmbeddedTestServer>(EmbeddedTestServer::TYPE_HTTP);

    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    server_->ServeFilesFromDirectory(
        test_data_dir.Append(FILE_PATH_LITERAL("chrome/test/data")));
    ASSERT_TRUE(server_->Start());

    client_ = std::make_unique<PrivetHTTPClientImpl>(
        "test", server_->host_port_pair(), shared_url_loader_factory_);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedTestServer> server_;
  std::unique_ptr<PrivetHTTPClientImpl> client_;
  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory_;
};

class MockPrivetURLLoaderDelegate : public PrivetURLLoader::Delegate {
 public:
  // GMock does not like mocking methods with movable parameters.
  void OnNeedPrivetToken(PrivetURLLoader::TokenCallback callback) override {
    std::move(callback).Run("abc");
  }
  MOCK_METHOD2(OnError,
               void(int response_code, PrivetURLLoader::ErrorType error));
  MOCK_METHOD3(OnParsedJson,
               void(int response_code,
                    const base::DictionaryValue& value,
                    bool has_error));
  MOCK_METHOD3(OnRawData,
               bool(bool response_is_file,
                    const std::string& data_string,
                    const base::FilePath& data_file));
};

TEST_F(PrivetHttpWithServerTest, HttpServer) {
  StrictMock<MockPrivetURLLoaderDelegate> delegate_;

  std::unique_ptr<PrivetURLLoader> url_loader = client_->CreateURLLoader(
      server_->GetURL("/simple.html"), "GET", &delegate_);
  url_loader->SetMaxRetriesForTest(1);
  url_loader->Start();

  base::RunLoop run_loop;
  EXPECT_CALL(delegate_, OnRawData(_, _, _))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        run_loop.Quit();
        return true;
      }));
  run_loop.Run();
}

}  // namespace

}  // namespace cloud_print
