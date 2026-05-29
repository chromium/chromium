// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/fake_api.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "net/base/data_url.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/http_request.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/gurl.h"

namespace indigo {

FakeApi::FakeApi() = default;
FakeApi::~FakeApi() = default;

bool FakeApi::InitializeAndListen() {
  return test_server_.InitializeAndListen();
}

void FakeApi::StartAcceptingConnections(int num_generate_requests,
                                        int num_delete_requests) {
  for (int i = 0; i < num_generate_requests; ++i) {
    controllable_responses_.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &test_server_, "/generate"));
  }
  for (int i = 0; i < num_delete_requests; ++i) {
    delete_responses_.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &test_server_, "/delete"));
  }

  test_server_.StartAcceptingConnections();
}

void FakeApi::StartAcceptingConnectionsAutomatic() {
  test_server_.RegisterRequestHandler(base::BindRepeating(
      &FakeApi::HandleDefaultRequest, base::Unretained(this)));
  test_server_.StartAcceptingConnections();
}

GURL FakeApi::GetGenerateUrl() const {
  return test_server_.GetURL("/generate");
}

void FakeApi::WaitForGenerateRequest(size_t index) {
  CHECK_LT(index, controllable_responses_.size());
  controllable_responses_[index]->WaitForRequest();
}

void FakeApi::SendSuccessResponse(const GURL& image_url, size_t index) {
  CHECK_LT(index, controllable_responses_.size());
  auto& controllable_response = controllable_responses_[index];
  CHECK(image_url.SchemeIs(url::kDataScheme));

  std::string mime_type, charset, data;
  CHECK(net::DataURL::Parse(image_url, &mime_type, &charset, &data));
  CHECK(blink::IsSupportedImageMimeType(mime_type));

  std::string response_body = absl::StrFormat(
      R"({"result": {"generatedImageUrl": "%s"}})", image_url.spec());
  controllable_response->Send(net::HTTP_OK, "application/json", response_body);
  controllable_response->Done();
}

void FakeApi::SendErrorResponse(size_t index) {
  CHECK_LT(index, controllable_responses_.size());
  auto& controllable_response = controllable_responses_[index];
  std::string response_body =
      R"({"error": {"code": "INTERNAL", "message": "Generation failed"}})";
  controllable_response->Send(net::HTTP_OK, "application/json", response_body);
  controllable_response->Done();
}

GURL FakeApi::GetDeleteUrl() const {
  return test_server_.GetURL("/delete");
}

void FakeApi::WaitForDeleteRequest(size_t index) {
  CHECK_LT(index, delete_responses_.size());
  delete_responses_[index]->WaitForRequest();
}

void FakeApi::SendDeleteSuccessResponse(size_t index) {
  CHECK_LT(index, delete_responses_.size());
  auto& controllable_response = delete_responses_[index];
  controllable_response->Send(net::HTTP_OK, "application/json", "{}");
  controllable_response->Done();
}

testing::AssertionResult FakeApi::RequestHasValidProductImage(
    base::span<const uint8_t> expected_image_bytes,
    size_t index) {
  CHECK_LT(index, controllable_responses_.size());
  const net::test_server::HttpRequest* request =
      controllable_responses_[index]->http_request();
  if (!request) {
    return testing::AssertionFailure() << "No request received";
  }

  if (request->method != net::test_server::METHOD_POST) {
    return testing::AssertionFailure()
           << "Expected POST request, got " << request->method_string;
  }

  std::optional<base::Value> value =
      base::JSONReader::Read(request->content, 0);
  if (!value) {
    return testing::AssertionFailure()
           << "Failed to parse JSON content: " << request->content;
  }

  if (!value->is_dict()) {
    return testing::AssertionFailure() << "Expected JSON dictionary";
  }

  const base::DictValue& dict = value->GetDict();
  const std::string* product_image_bytes_base64 =
      dict.FindString("productImageBytes");
  if (!product_image_bytes_base64) {
    return testing::AssertionFailure()
           << "Missing productImageBytes in request";
  }

  std::string expected_base64 = base::Base64Encode(expected_image_bytes);
  if (*product_image_bytes_base64 != expected_base64) {
    return testing::AssertionFailure()
           << "Expected productImageBytes: " << expected_base64
           << ", got: " << *product_image_bytes_base64;
  }

  const std::string* output_format = dict.FindString("outputFormat");
  if (!output_format) {
    return testing::AssertionFailure() << "Missing outputFormat in request";
  }

  if (*output_format != "OUTPUT_FORMAT_IMAGE_BYTES") {
    return testing::AssertionFailure()
           << "Expected outputFormat: OUTPUT_FORMAT_IMAGE_BYTES, got: "
           << *output_format;
  }

  return testing::AssertionSuccess();
}

std::unique_ptr<net::test_server::HttpResponse> FakeApi::HandleDefaultRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/generate") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(
        "{\n"
        "  \"result\": {\n"
        "    \"generatedImageUrl\": \"data:image/png;base64,"
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhg"
        "GAWjR9awAAAABJRU5ErkJggg==\"\n"
        "  }\n"
        "}");
    response->set_content_type("application/json");
    return response;
  }
  if (request.relative_url == "/delete") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content("{}");
    response->set_content_type("application/json");
    return response;
  }
  return nullptr;
}

}  // namespace indigo
