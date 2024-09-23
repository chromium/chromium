// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/test_authorization_server.h"

#include <list>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace oauth2 {

namespace {

// Checks 'value' == 'expected'. Returns empty string when true, otherwise
// returns error message.
std::string ExpectEqual(const std::string& name,
                        const std::string& value,
                        const std::string& expected) {
  if (value == expected) {
    return "";
  }
  return base::StringPrintf("Invalid %s: got \"%s\", expected \"%s\";  ",
                            name.c_str(), value.c_str(), expected.c_str());
}

// Returns a list with a single element `value`.
base::Value OneElementArray(const std::string& value) {
  base::Value::List arr;
  arr.Append(value);
  return base::Value(std::move(arr));
}

}  // namespace

bool ParseURLParameters(const std::string& params_str,
                        base::flat_map<std::string, std::string>& results) {
  results.clear();
  auto params_vect = base::SplitString(params_str, "&", base::KEEP_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
  for (auto& key_val : params_vect) {
    auto equal_sign = key_val.find('=');
    std::string key;
    std::string val;
    if (equal_sign < key_val.size()) {
      key = key_val.substr(0, equal_sign);
      val = key_val.substr(equal_sign + 1);
    } else {
      key = key_val;
    }
    key = base::UnescapeBinaryURLComponent(key);
    if (key.empty() || results.contains(key)) {
      return false;
    }
    results[key] = base::UnescapeBinaryURLComponent(val);
  }
  return true;
}

base::OnceCallback<void(StatusCode, std::string)> BindResult(
    CallbackResult& target) {
  target.status = StatusCode::kUnexpectedError;
  target.data.clear();
  auto save_results = [](CallbackResult* target, StatusCode status,
                         std::string data) {
    target->status = status;
    target->data = std::move(data);
  };
  return base::BindOnce(save_results, base::Unretained(&target));
}

base::Value::Dict BuildMetadata(const std::string& authorization_server_uri,
                                const std::string& authorization_uri,
                                const std::string& token_uri,
                                const std::string& registration_uri,
                                const std::string& revocation_uri) {
  base::Value::Dict dict;
  dict.Set("issuer", authorization_server_uri);
  dict.Set("authorization_endpoint", authorization_uri);
  dict.Set("token_endpoint", token_uri);
  if (!registration_uri.empty()) {
    dict.Set("registration_endpoint", registration_uri);
  }
  if (!revocation_uri.empty()) {
    dict.Set("revocation_endpoint", revocation_uri);
  }
  dict.Set("response_types_supported", OneElementArray("code"));
  dict.Set("response_modes_supported", OneElementArray("query"));
  dict.Set("grant_types_supported", OneElementArray("authorization_code"));
  dict.Set("token_endpoint_auth_methods_supported", OneElementArray("none"));
  dict.Set("code_challenge_methods_supported", OneElementArray("S256"));
  return dict;
}

FakeAuthorizationServer::FakeAuthorizationServer() {
  fake_server_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        upload_data_.push(network::GetUploadData(request));
      }));
}

FakeAuthorizationServer::~FakeAuthorizationServer() = default;

std::string FakeAuthorizationServer::ReceiveGET(const std::string& url) {
  std::string payload;
  auto msg = GetNextRequest("GET", url, "", payload);
  if (!payload.empty()) {
    msg +=
        base::StrCat({"Unexpected payload: \"", payload.substr(0, 256), "\""});
  }
  return msg;
}

std::string FakeAuthorizationServer::ReceivePOSTWithJSON(
    const std::string& url,
    base::Value::Dict& out_params) {
  std::string payload;
  auto msg = GetNextRequest("POST", url, "application/json", payload);
  auto content = base::JSONReader::Read(payload);
  out_params.clear();
  if (content && content->is_dict()) {
    out_params = std::move(content).value().TakeDict();
  } else {
    msg += base::StrCat(
        {"Cannot parse the payload: \"", payload.substr(0, 256), "\""});
  }
  return msg;
}

std::string FakeAuthorizationServer::ReceivePOSTWithURLParams(
    const std::string& url,
    base::flat_map<std::string, std::string>& out_params) {
  std::string payload;
  auto msg =
      GetNextRequest("POST", url, "application/x-www-form-urlencoded", payload);
  if (!ParseURLParameters(payload, out_params)) {
    msg += base::StrCat(
        {"Cannot parse the payload: \"", payload.substr(0, 256), "\""});
  }
  return msg;
}

void FakeAuthorizationServer::ResponseWithJSON(
    net::HttpStatusCode status,
    const base::Value::Dict& params) {
  CHECK(current_request_);
  std::string response_content;
  CHECK(base::JSONWriter::Write(params, &response_content));
  network::mojom::URLResponseHeadPtr response_head(
      network::CreateURLResponseHead(status));
  response_head->headers->SetHeader("Content-Type", "application/json");
  response_head->headers->SetHeader("Cache-Control", "no-store");
  response_head->headers->SetHeader("Pragma", "no-cache");
  network::URLLoaderCompletionStatus compl_status;
  CHECK(fake_server_.SimulateResponseForPendingRequest(
      current_request_->request.url, compl_status, std::move(response_head),
      response_content));
  current_request_ = nullptr;
  task_environment_.RunUntilIdle();
}

std::string FakeAuthorizationServer::GetNextRequest(
    const std::string& method,
    const std::string& url,
    const std::string& content_type,
    std::string& content) {
  CHECK(!current_request_);
  task_environment_.RunUntilIdle();
  current_request_ = fake_server_.GetPendingRequest(0);
  if (!current_request_) {
    return "There is no pending requests";
  }
  CHECK(!upload_data_.empty());
  content = std::move(upload_data_.front());
  upload_data_.pop();

  std::string msg;
  msg += ExpectEqual("HTTP method", current_request_->request.method, method);
  msg += ExpectEqual("URL", current_request_->request.url.spec(), url);
  msg += ExpectEqual("header Content-Type",
                     current_request_->request.headers.GetHeader("Content-Type")
                         .value_or(std::string()),
                     content_type);
  std::optional<std::string> value =
      current_request_->request.headers.GetHeader("Accept");
  if (value) {
    msg += ExpectEqual("header Accept", *value, "application/json");
  }
  return msg;
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
