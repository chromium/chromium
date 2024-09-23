// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/http_exchange.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {
namespace printing {
namespace oauth2 {

namespace {

// Converts ContentFormat to MIME string.
std::string ToString(ContentFormat format) {
  switch (format) {
    case ContentFormat::kJson:
      return "application/json";
    case ContentFormat::kXWwwFormUrlencoded:
      return "application/x-www-form-urlencoded";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return "";
}

}  // namespace

HttpExchange::HttpExchange(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

HttpExchange::~HttpExchange() {}

void HttpExchange::Clear() {
  content_.clear();
  error_msg_.clear();
  url_loader_.reset();
}

void HttpExchange::AddParamString(const std::string& name,
                                  const std::string& value) {
  DCHECK(!name.empty());
  content_.Set(name, value);
}

void HttpExchange::AddParamArrayString(const std::string& name,
                                       const std::vector<std::string>& value) {
  DCHECK(!name.empty());
  base::Value::List list_node;
  for (const auto& value_element : value) {
    list_node.Append(value_element);
  }
  content_.Set(name, std::move(list_node));
}

void HttpExchange::Exchange(
    const std::string& http_method,
    const GURL& url,
    ContentFormat request_format,
    int success_http_status,
    int error_http_status,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation,
    OnExchangeCompletedCallback callback) {
  std::string data;
  // Converts `content_` to `data`.
  if (request_format == ContentFormat::kJson) {
    if (!base::JSONWriter::Write(content_, &data)) {
      error_msg_ = "Cannot create JSON payload.";
      std::move(callback).Run(StatusCode::kUnexpectedError);
      return;
    }
  } else if (request_format == ContentFormat::kXWwwFormUrlencoded) {
    for (const auto kv : content_) {
      if (!data.empty()) {
        data += "&";
      }
      data += base::EscapeUrlEncodedData(kv.first, true);
      data += "=";
      switch (kv.second.type()) {
        case base::Value::Type::BOOLEAN:
          data += (kv.second.GetBool()) ? ("true") : ("false");
          break;
        case base::Value::Type::INTEGER:
          data += base::NumberToString(kv.second.GetInt());
          break;
        case base::Value::Type::STRING:
          data += base::EscapeUrlEncodedData(kv.second.GetString(), true);
          break;
        default:
          error_msg_ = "Cannot save a vector value as x-www-form-urlencoded.";
          std::move(callback).Run(StatusCode::kUnexpectedError);
          return;
      }
    }
  }

  // Prepares `url_loader_`.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = http_method;
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader("Accept", "application/json");
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::CompleteNetworkTrafficAnnotation("printing_oauth2_http_exchange",
                                            partial_traffic_annotation, R"(
        semantics {
          sender: "Printers Manager"
          trigger: "User tries to use a printer that requires OAuth2 "
            "authorization."
          destination: OTHER
          destination_other: "Trusted Authorization Server"
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this experimental feature via 'Enable "
            "OAuth when printing via the IPP protocol' on the chrome://flags "
            "tab."
          policy_exception_justification: "This is an experimental feature. "
            "The policy controlling this feature does not yet exist."
        })");
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  // Sets the payload.
  if (request_format != ContentFormat::kEmpty) {
    url_loader_->AttachStringForUpload(data, ToString(request_format));
  }
  // We want to receive the body even on error, as it may contain the reason
  // for failure.
  url_loader_->SetAllowHttpErrorResults(true);
  // It's safe to use Unretained below as the |url_loader_| is owned by
  // |this|.
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&HttpExchange::OnURLLoaderCompleted,
                     base::Unretained(this), success_http_status,
                     error_http_status, std::move(callback)),
      1024 * 1024);
}

void HttpExchange::OnURLLoaderCompleted(
    int success_http_status,
    int error_http_status,
    OnExchangeCompletedCallback callback,
    std::unique_ptr<std::string> response_body) {
  // Checks for connection errors.
  const int net_error = url_loader_->NetError();
  if (!(net_error == net::OK && url_loader_->ResponseInfo() &&
        url_loader_->ResponseInfo()->headers)) {
    error_msg_ = base::StringPrintf("NetError=%d", net_error);
    std::move(callback).Run(StatusCode::kConnectionError);
    return;
  }

  // Checks for special HTTP status codes.
  const auto http_status = GetHttpStatus();
  if (http_status == 500) {
    error_msg_ = "Internal Server Error (HTTP status code=500)";
    std::move(callback).Run(StatusCode::kServerError);
    return;
  }
  if (http_status == 503) {
    error_msg_ = "Service Unavailable (HTTP status code=503)";
    std::move(callback).Run(StatusCode::kServerTemporarilyUnavailable);
    return;
  }
  if (http_status != success_http_status && http_status != error_http_status) {
    error_msg_ = "Unexpected HTTP status: " + base::NumberToString(http_status);
    std::move(callback).Run(StatusCode::kInvalidResponse);
    return;
  }

  // Checks if response body (payload) exists. It cannot be empty since we
  // expect a JSON object.
  if (!response_body || response_body->empty()) {
    error_msg_ = "Missing payload.";
    std::move(callback).Run(StatusCode::kInvalidResponse);
    return;
  }

  // Checks if the payload of the response contains a JSON object.
  std::string mime_type;
  std::string charset;
  url_loader_->ResponseInfo()->headers->GetMimeTypeAndCharset(&mime_type,
                                                              &charset);
  if (mime_type != "application/json") {
    error_msg_ = "Unexpected type of payload: " + mime_type;
    std::move(callback).Run(StatusCode::kInvalidResponse);
    return;
  }
  if (!charset.empty() && charset != "utf-8" && charset != "ascii") {
    error_msg_ = "Unsupported charset: " + charset;
    std::move(callback).Run(StatusCode::kInvalidResponse);
    return;
  }
  auto parsed = base::JSONReader::Read(*response_body);
  if (!parsed) {
    error_msg_ = "Cannot parse JSON payload.";
    std::move(callback).Run(StatusCode::kInvalidResponse);
    return;
  }
  if (!parsed->is_dict()) {
    error_msg_ = "JSON payload is not a dictionary.";
    std::move(callback).Run(StatusCode::kInvalidResponse);
    return;
  }
  content_ = std::move(parsed).value().TakeDict();

  // Exits if success.
  if (http_status == success_http_status) {
    std::move(callback).Run(StatusCode::kOK);
    return;
  }

  // Parses error response.
  std::string error;
  std::string error_description;
  std::string error_uri;
  if (!ParamStringGet("error", true, &error) ||
      !ParamStringGet("error_description", false, &error_description) ||
      !ParamStringGet("error_uri", false, &error_uri)) {
    std::move(callback).Run(StatusCode::kInvalidResponse);
    return;
  }

  // Handles the error response.
  error_msg_ = "error=" + error;
  if (!error_description.empty()) {
    error_msg_ += base::StrCat({"; description=", error_description});
  }
  if (!error_uri.empty()) {
    error_msg_ += base::StrCat({"; uri=", error_uri});
  }
  if (error == "invalid_grant") {
    std::move(callback).Run(StatusCode::kInvalidAccessToken);
  } else {
    std::move(callback).Run(StatusCode::kAccessDenied);
  }
}

int HttpExchange::GetHttpStatus() const {
  if (url_loader_ && url_loader_->ResponseInfo() &&
      url_loader_->ResponseInfo()->headers) {
    return url_loader_->ResponseInfo()->headers->response_code();
  }
  return 0;
}

bool HttpExchange::ParamArrayStringContains(const std::string& name,
                                            bool required,
                                            const std::string& value) {
  base::Value* node = FindNode(name, required);
  if (!node) {
    return !required;
  }
  if (!node->is_list()) {
    error_msg_ = base::StrCat({"Field ", name, " must be an array."});
    return false;
  }
  const auto& node_as_list = node->GetList();
  for (auto& element : node_as_list) {
    if (element.is_string() && element.GetString() == value) {
      // Success!
      return true;
    }
  }
  error_msg_ =
      base::StrCat({"Field ", name, " must contain the value '", value, "'"});
  return false;
}

bool HttpExchange::ParamArrayStringEquals(
    const std::string& name,
    bool required,
    const std::vector<std::string>& value) {
  base::Value* node = FindNode(name, required);
  if (!node) {
    return !required;
  }
  if (!node->is_list()) {
    error_msg_ = base::StrCat({"Field ", name, " must be an array"});
    return false;
  }
  const base::Value::List& node_as_list = node->GetList();
  if (node_as_list.size() == value.size()) {
    // Compares the vectors, element by element.
    bool are_equal = true;
    for (size_t i = 0; i < value.size() && are_equal; ++i) {
      const auto& element = node_as_list[i];
      if (!element.is_string() || element.GetString() != value[i]) {
        are_equal = false;
      }
    }
    if (are_equal) {
      // Success!
      return true;
    }
  }
  // Vectors are different, builds an error message.
  error_msg_ = base::StrCat({"Field ", name, " must contain the value ["});
  for (auto& element : value) {
    error_msg_ += base::StrCat({"'", element, "',"});
  }
  // Removes the last comma and add a closing bracket.
  if (!value.empty()) {
    error_msg_.pop_back();
  }
  error_msg_ += "]";
  return false;
}

bool HttpExchange::ParamStringGet(const std::string& name,
                                  bool required,
                                  std::string* value) {
  base::Value* node = FindNode(name, required);
  if (!node) {
    return !required;
  }
  if (!node->is_string()) {
    error_msg_ = base::StrCat({"Field ", name, " must be a string"});
    return false;
  }
  if (required && node->GetString().empty()) {
    error_msg_ = base::StrCat({"Field ", name, " cannot be empty"});
    return false;
  }
  if (value) {
    *value = node->GetString();
  }
  return true;
}

bool HttpExchange::ParamStringEquals(const std::string& name,
                                     bool required,
                                     const std::string& value) {
  base::Value* node = FindNode(name, required);
  if (!node) {
    return !required;
  }
  if (!node->is_string()) {
    error_msg_ = base::StrCat({"Field ", name, " must be a string"});
    return false;
  }
  if (value != node->GetString()) {
    error_msg_ = base::StrCat({"Field ", name, " must be equal '", value, "'"});
    return false;
  }
  return true;
}

bool HttpExchange::ParamURLGet(const std::string& name,
                               bool required,
                               GURL* value) {
  base::Value* node = FindNode(name, required);
  if (!node) {
    return !required;
  }
  if (!node->is_string()) {
    error_msg_ = base::StrCat({"Field ", name, " must be an URL"});
    return false;
  }
  GURL gurl(node->GetString());
  if (gurl.is_valid() && gurl.IsStandard() && gurl.scheme() == "https") {
    // Success!
    if (value) {
      *value = gurl;
    }
    return true;
  }
  error_msg_ =
      base::StrCat({"Field ", name, " must be a valid URL of type 'https://'"});
  return false;
}

bool HttpExchange::ParamURLEquals(const std::string& name,
                                  bool required,
                                  const GURL& value) {
  base::Value* node = FindNode(name, required);
  if (!node) {
    return !required;
  }
  if (!node->is_string()) {
    error_msg_ = base::StrCat({"Field ", name, " must be an URL"});
    return false;
  }
  if (value != GURL(node->GetString())) {
    error_msg_ =
        base::StrCat({"Field ", name, " must be equal '", value.spec(), "'"});
    return false;
  }
  return true;
}

const std::string& HttpExchange::GetErrorMessage() const {
  return error_msg_;
}

base::Value* HttpExchange::FindNode(const std::string& name, bool required) {
  base::Value* value = content_.Find(name);
  if (required && !value) {
    error_msg_ = base::StrCat({"Field ", name, " is missing"});
  }
  return value;
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
