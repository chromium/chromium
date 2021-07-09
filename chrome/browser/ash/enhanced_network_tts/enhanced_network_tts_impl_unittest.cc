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
constexpr char kTemplateRequest[] = R"({
                                        "text": {
                                          "text_parts": ["%s"]
                                        }
                                      })";

// Template for a server response.
constexpr char kTemplateResponse[] = R"([{
                                      "metadata": {}
                                    }
                                    ,
                                    {
                                      "text": {}
                                    }
                                    ,
                                    {
                                      "audio": {
                                        "bytes": "%s"
                                      }
                                    }
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

}  // namespace

class EnhancedNetworkTtsImplTest : public testing::Test {
 protected:
  void SetUp() override {
    EnhancedNetworkTtsImpl::GetInstance().BindReceiverAndURLFactory(
        remote_.BindNewPipeAndPassReceiver(),
        test_url_factory_.AsSharedURLLoaderFactory());
  }

  base::test::TaskEnvironment test_task_env_;
  TestServerURLLoaderFactory test_url_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  mojo::Remote<mojom::EnhancedNetworkTts> remote_;
};

TEST_F(EnhancedNetworkTtsImplTest, GetAudioDataSucceeds) {
  const std::string input_text = "Hi.";
  std::vector<uint8_t> result;
  EnhancedNetworkTtsImpl::GetInstance().GetAudioData(
      input_text,
      base::BindOnce([](std::vector<uint8_t>* result,
                        const std::vector<uint8_t>& bytes) { *result = bytes; },
                     &result));
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
  EXPECT_EQ(result, expected_output);
}

TEST_F(EnhancedNetworkTtsImplTest, ServerError) {
  const std::string input_text = "Hi.";
  std::vector<uint8_t> result;
  EnhancedNetworkTtsImpl::GetInstance().GetAudioData(
      input_text,
      base::BindOnce([](std::vector<uint8_t>* result,
                        const std::vector<uint8_t>& bytes) { *result = bytes; },
                     &result));
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
  EXPECT_EQ(result, std::vector<uint8_t>());
}

TEST_F(EnhancedNetworkTtsImplTest, JsonDecodingError) {
  const std::string input_text = "Hi.";
  std::vector<uint8_t> result;
  EnhancedNetworkTtsImpl::GetInstance().GetAudioData(
      input_text,
      base::BindOnce([](std::vector<uint8_t>* result,
                        const std::vector<uint8_t>& bytes) { *result = bytes; },
                     &result));
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
  EXPECT_EQ(result, std::vector<uint8_t>());
}

}  // namespace enhanced_network_tts
}  // namespace ash
