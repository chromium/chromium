// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_impl.h"

#include <map>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_constants.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace enhanced_network_tts {
namespace {

// Template for a request.
constexpr char kTemplateRequest[] =
    R"({
        "text": {"text_parts": ["%s"]}
      })";

// Template for a server response.
constexpr char kTemplateResponse[] =
    R"([
        {"metadata": {}},
        {"text": {
          "timingInfo": [
            {
              "text": "test1",
              "location": {
                "textLocation": {"length": 5},
                "timeLocation": {
                  "timeOffset": "0.01s",
                  "duration": "0.14s"
                }
              }
            },
            {
              "text": "test2",
              "location": {
                "textLocation": {"length": 5, "offset": 6},
                "timeLocation": {
                  "timeOffset": "0.16s",
                  "duration": "0.17s"
                }
              }
            }
          ]}
        },
        {"audio": {"bytes": "%s"}}
      ])";

// A fake server that supports test URL loading.
class TestServerURLLoaderFactory {
 public:
  TestServerURLLoaderFactory()
      : shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &loader_factory_)) {}
  TestServerURLLoaderFactory(const TestServerURLLoaderFactory&) = delete;
  TestServerURLLoaderFactory& operator=(const TestServerURLLoaderFactory&) =
      delete;
  ~TestServerURLLoaderFactory() = default;

  const std::vector<network::TestURLLoaderFactory::PendingRequest>& requests() {
    return *loader_factory_.pending_requests();
  }

  // Expects that the earliest received request has the given URL, headers and
  // body, and replies with the given response.
  //
  // |expected_headers| is a map from header key string to either:
  //   a) a null optional, if the given header should not be present, or
  //   b) a non-null optional, if the given header should be present and match
  //      the optional value.
  //
  // Consumes the earliest received request (i.e. a subsequent call will apply
  // to the second-earliest received request and so on).
  void ExpectRequestAndSimulateResponse(
      const std::string& expected_url,
      const std::map<std::string, absl::optional<std::string>>&
          expected_headers,
      const std::string& expected_body,
      const std::string& response,
      const net::HttpStatusCode response_code) {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>&
        pending_requests = *loader_factory_.pending_requests();

    ASSERT_FALSE(pending_requests.empty());
    const network::ResourceRequest& request = pending_requests.front().request;

    // Assert that the earliest request is for the given URL.
    EXPECT_EQ(request.url, GURL(expected_url));

    // Expect that specified headers are accurate.
    for (const auto& kv : expected_headers) {
      if (kv.second.has_value()) {
        std::string actual_value;
        EXPECT_TRUE(request.headers.GetHeader(kv.first, &actual_value));
        EXPECT_EQ(actual_value, *kv.second);
      } else {
        EXPECT_FALSE(request.headers.HasHeader(kv.first));
      }
    }

    // Extract request body.
    std::string actual_body;
    if (request.request_body) {
      const std::vector<network::DataElement>* const elements =
          request.request_body->elements();

      // We only support the simplest body structure.
      if (elements && elements->size() == 1 &&
          (*elements)[0].type() ==
              network::mojom::DataElementDataView::Tag::kBytes) {
        actual_body = std::string(
            (*elements)[0].As<network::DataElementBytes>().AsStringPiece());
      }
    }

    EXPECT_EQ(actual_body, expected_body);

    // Guaranteed to match the first request based on URL.
    loader_factory_.SimulateResponseForPendingRequest(expected_url, response,
                                                      response_code);
  }

  scoped_refptr<network::SharedURLLoaderFactory> AsSharedURLLoaderFactory() {
    return shared_loader_factory_;
  }

 private:
  network::TestURLLoaderFactory loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_loader_factory_;
};

// Returns a JSON string of a formatted request for the |input_text|. The
// request is based on a template and is guaranteed to be correct.
std::string CreateCorrectRequest(const std::string& input_text) {
  // Fills the template and then parse and rewrite it to a canonically
  // formatted version.
  const std::unique_ptr<base::Value> json = base::JSONReader::ReadDeprecated(
      base::StringPrintf(kTemplateRequest, input_text.c_str()));
  EXPECT_FALSE(json->DictEmpty());

  std::string out;
  base::JSONWriter::Write(*json, &out);

  return out;
}

// Receives the result of a request and writes the result data into the given
// variables.
void ReportResult(absl::optional<mojom::TtsRequestError>* const error,
                  std::vector<uint8_t>* const audio_data,
                  std::vector<mojom::TimingInfo>* const timing_data,
                  mojom::TtsResponsePtr result) {
  if (result->which() == mojom::TtsResponse::Tag::ERROR_CODE) {
    *error = result->get_error_code();
  } else {
    // Copy audio data.
    for (const auto audio_data_pt : result->get_data()->audio)
      audio_data->push_back(audio_data_pt);

    // Copy timing data.
    for (const auto& timing_ptr : result->get_data()->time_info)
      timing_data->push_back(*timing_ptr);
  }
}

}  // namespace

class EnhancedNetworkTtsImplTest : public testing::Test {
 protected:
  void SetUp() override {
    in_process_data_decoder_ =
        std::make_unique<data_decoder::test::InProcessDataDecoder>();
    enhanced_network_tts_impl_ = new EnhancedNetworkTtsImpl();
    enhanced_network_tts_impl_->BindReceiverAndURLFactory(
        remote_.BindNewPipeAndPassReceiver(),
        test_url_factory_.AsSharedURLLoaderFactory());
  }

  EnhancedNetworkTtsImpl& GetTestingInstance() {
    return *enhanced_network_tts_impl_;
  }

  EnhancedNetworkTtsImpl* enhanced_network_tts_impl_;
  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
  base::test::TaskEnvironment test_task_env_;
  TestServerURLLoaderFactory test_url_factory_;
  mojo::Remote<mojom::EnhancedNetworkTts> remote_;
};

TEST_F(EnhancedNetworkTtsImplTest, GetAudioDataSucceeds) {
  const std::string input_text = "Hi.";
  absl::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, absl::nullopt, absl::nullopt),
      base::BindOnce(&ReportResult, &error, &audio_data, &timing_data));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, absl::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  const std::string expected_body = CreateCorrectRequest(input_text);
  // |expected_output| here is arbitrary, which is encoded into a fake response
  // sent by the fake server, |TestServerURLLoaderFactory|. In general, we
  // expect the real server sends the audio data back as a base64 encoded JSON
  // string.
  const std::vector<uint8_t> expected_output = {1, 2, 5};
  std::string encoded_output(expected_output.begin(), expected_output.end());
  base::Base64Encode(encoded_output, &encoded_output);
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body,
      base::StringPrintf(kTemplateResponse, encoded_output.c_str()),
      net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // We only get the data after the server's response. We simulate the response
  // in the code above.
  EXPECT_EQ(audio_data, expected_output);
  // The timing data is hardcoded in |kTemplateResponse|.
  EXPECT_EQ(timing_data.size(), 2);
  EXPECT_EQ(timing_data[0].text, "test1");
  EXPECT_EQ(timing_data[0].time_offset, "0.01s");
  EXPECT_EQ(timing_data[0].duration, "0.14s");
  EXPECT_EQ(timing_data[0].text_offset, 0);
  EXPECT_EQ(timing_data[1].text, "test2");
  EXPECT_EQ(timing_data[1].time_offset, "0.16s");
  EXPECT_EQ(timing_data[1].duration, "0.17s");
  EXPECT_EQ(timing_data[1].text_offset, 6);
}

TEST_F(EnhancedNetworkTtsImplTest, EmptyUtteranceError) {
  const std::string input_text("");
  absl::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, absl::nullopt, absl::nullopt),
      base::BindOnce(&ReportResult, &error, &audio_data, &timing_data));
  test_task_env_.RunUntilIdle();

  // Over length request will be terminated before sending to server.
  EXPECT_EQ(error, mojom::TtsRequestError::kEmptyUtterance);
}

TEST_F(EnhancedNetworkTtsImplTest, OverLengthError) {
  const std::string input_text(mojom::kEnhancedNetworkTtsMaxCharacterSize + 1,
                               'x');
  absl::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, absl::nullopt, absl::nullopt),
      base::BindOnce(&ReportResult, &error, &audio_data, &timing_data));
  test_task_env_.RunUntilIdle();

  // Over length request will be terminated before sending to server.
  EXPECT_EQ(error, mojom::TtsRequestError::kOverLength);
}

TEST_F(EnhancedNetworkTtsImplTest, OverrideRequest) {
  const std::string input_text("request");
  absl::optional<mojom::TtsRequestError> error_first_request;
  std::vector<uint8_t> audio_data_first_request;
  std::vector<mojom::TimingInfo> timing_data_first_request;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, absl::nullopt, absl::nullopt),
      base::BindOnce(&ReportResult, &error_first_request,
                     &audio_data_first_request, &timing_data_first_request));
  test_task_env_.RunUntilIdle();
  // The second request comes in before the server replies to the first one.
  absl::optional<mojom::TtsRequestError> error_second_request;
  std::vector<uint8_t> audio_data_second_request;
  std::vector<mojom::TimingInfo> timing_data_second_request;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, absl::nullopt, absl::nullopt),
      base::BindOnce(&ReportResult, &error_second_request,
                     &audio_data_second_request, &timing_data_second_request));
  test_task_env_.RunUntilIdle();

  // Assume the server replies to the requests in sequence.
  const std::map<std::string, absl::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  std::string expected_body = CreateCorrectRequest(input_text);
  const std::vector<uint8_t> expected_output = {1, 2, 5};
  std::string encoded_output(expected_output.begin(), expected_output.end());
  base::Base64Encode(encoded_output, &encoded_output);
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body,
      base::StringPrintf(kTemplateResponse, encoded_output.c_str()),
      net::HTTP_OK);
  test_task_env_.RunUntilIdle();
  // Assume the server replies same message to both requests.
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body,
      base::StringPrintf(kTemplateResponse, encoded_output.c_str()),
      net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // The first request gets an error message while the second request gets the
  // data.
  EXPECT_EQ(error_first_request, mojom::TtsRequestError::kRequestOverride);
  EXPECT_EQ(timing_data_first_request.size(), 0);
  EXPECT_EQ(audio_data_first_request.size(), 0);
  EXPECT_EQ(audio_data_second_request, expected_output);
}

TEST_F(EnhancedNetworkTtsImplTest, ServerError) {
  const std::string input_text = "Hi.";
  absl::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, absl::nullopt, absl::nullopt),
      base::BindOnce(&ReportResult, &error, &audio_data, &timing_data));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, absl::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  const std::string expected_body = CreateCorrectRequest(input_text);
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body, "" /* response= */,
      net::HTTP_INTERNAL_SERVER_ERROR);
  test_task_env_.RunUntilIdle();

  // We only get the data after the server's response. We simulate the response
  // in the code above.
  EXPECT_EQ(error, mojom::TtsRequestError::kServerError);
}

TEST_F(EnhancedNetworkTtsImplTest, JsonDecodingError) {
  const std::string input_text = "Hi.";
  absl::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, absl::nullopt, absl::nullopt),
      base::BindOnce(&ReportResult, &error, &audio_data, &timing_data));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, absl::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  const std::string expected_body = CreateCorrectRequest(input_text);
  const char response[] = R"([{some wired response)";
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body, response,
      net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // We only get the data after the server's response. We simulate the response
  // in the code above.
  EXPECT_EQ(error, mojom::TtsRequestError::kReceivedUnexpectedData);
}

}  // namespace enhanced_network_tts
}  // namespace ash
