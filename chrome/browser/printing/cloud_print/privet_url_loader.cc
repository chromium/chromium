// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_url_loader.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/printing/cloud_print/privet_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace cloud_print {

namespace {

constexpr char kXPrivetTokenHeaderKey[] = "X-Privet-Token";
constexpr char kRangeHeaderKey[] = "Range";
constexpr char kRangeHeaderValueFormat[] = "bytes=%d-%d";
constexpr char kXPrivetEmptyToken[] = "\"\"";
constexpr int kPrivetMaxRetries = 20;
constexpr int kPrivetTimeoutOnError = 5;
constexpr int kHTTPErrorCodeInvalidXPrivetToken = 418;
constexpr size_t kPrivetMaxContentSize = 1 * 1024 * 1024;

base::LazyInstance<std::map<std::string, std::string>>::Leaky g_tokens =
    LAZY_INSTANCE_INITIALIZER;

std::string MakeRangeHeaderValue(int start, int end) {
  DCHECK_GE(start, 0);
  DCHECK_GT(end, 0);
  DCHECK_GT(end, start);
  return base::StringPrintf(kRangeHeaderValueFormat, start, end);
}

}  // namespace

PrivetURLLoader::RetryImmediatelyForTest::RetryImmediatelyForTest() {
  DCHECK(!skip_retry_timeouts_for_tests_);
  skip_retry_timeouts_for_tests_ = true;
}

PrivetURLLoader::RetryImmediatelyForTest::~RetryImmediatelyForTest() {
  DCHECK(skip_retry_timeouts_for_tests_);
  skip_retry_timeouts_for_tests_ = false;
}

// static
bool PrivetURLLoader::skip_retry_timeouts_for_tests_ = false;

void PrivetURLLoader::Delegate::OnNeedPrivetToken(TokenCallback callback) {
  OnError(0, TOKEN_ERROR);
}

bool PrivetURLLoader::Delegate::OnRawData(bool response_is_file,
                                          const std::string& data_string,
                                          const base::FilePath& data_file) {
  return false;
}

PrivetURLLoader::PrivetURLLoader(
    const GURL& url,
    const std::string& request_type,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    PrivetURLLoader::Delegate* delegate)
    : url_(url),
      request_type_(request_type),
      url_loader_factory_(url_loader_factory),
      traffic_annotation_(traffic_annotation),
      delegate_(delegate),
      max_retries_(kPrivetMaxRetries) {}

PrivetURLLoader::~PrivetURLLoader() {}

// static
void PrivetURLLoader::SetTokenForHost(const std::string& host,
                                      const std::string& token) {
  g_tokens.Get()[host] = token;
}

// static
void PrivetURLLoader::ResetTokenMapForTest() {
  g_tokens.Get().clear();
}

void PrivetURLLoader::SetMaxRetriesForTest(int max_retries) {
  DCHECK_EQ(tries_, 0);
  max_retries_ = max_retries;
}

void PrivetURLLoader::DoNotRetryOnTransientError() {
  DCHECK_EQ(tries_, 0);
  do_not_retry_on_transient_error_ = true;
}

void PrivetURLLoader::SendEmptyPrivetToken() {
  DCHECK_EQ(tries_, 0);
  send_empty_privet_token_ = true;
}

std::string PrivetURLLoader::GetPrivetAccessToken() {
  if (send_empty_privet_token_)
    return std::string();

  auto it = g_tokens.Get().find(GetHostString());
  return it != g_tokens.Get().end() ? it->second : std::string();
}

std::string PrivetURLLoader::GetHostString() {
  return url_.GetOrigin().spec();
}

void PrivetURLLoader::SaveResponseToFile() {
  DCHECK_EQ(tries_, 0);
  make_response_file_ = true;
}

void PrivetURLLoader::SetByteRange(int start, int end) {
  DCHECK_EQ(tries_, 0);
  byte_range_start_ = start;
  byte_range_end_ = end;
  has_byte_range_ = true;
}

void PrivetURLLoader::Try() {
  tries_++;
  if (tries_ > max_retries_) {
    delegate_->OnError(0, UNKNOWN_ERROR);
    return;
  }

  DVLOG(1) << "Attempt: " << tries_;

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url_;
  request->method = request_type_;
  // Privet requests are relevant to hosts on local network only.
  request->load_flags = net::LOAD_BYPASS_PROXY | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::string token = GetPrivetAccessToken();
  if (token.empty())
    token = kXPrivetEmptyToken;
  request->headers.SetHeader(kXPrivetTokenHeaderKey, token);

  if (has_byte_range_) {
    request->headers.SetHeader(
        kRangeHeaderKey,
        MakeRangeHeaderValue(byte_range_start_, byte_range_end_));
  }

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);

  url_loader_->SetAllowHttpErrorResults(true);

  url_loader_->SetOnResponseStartedCallback(base::BindOnce(
      &PrivetURLLoader::OnResponseStarted, weak_factory_.GetWeakPtr()));

  // URLFetcher requires us to set upload data for POST requests.
  if (request_type_ == "POST")
    url_loader_->AttachStringForUpload(upload_data_, upload_content_type_);

  if (make_response_file_) {
    url_loader_->DownloadToTempFile(
        url_loader_factory_.get(),
        base::BindOnce(&PrivetURLLoader::OnDownloadedToFile,
                       weak_factory_.GetWeakPtr()));
  } else {
    url_loader_->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&PrivetURLLoader::OnDownloadedToString,
                       weak_factory_.GetWeakPtr()),
        kPrivetMaxContentSize);
  }
}

void PrivetURLLoader::Start() {
  DCHECK_EQ(tries_, 0);  // We haven't called |Start()| yet.

  if (!url_.is_valid())
    return delegate_->OnError(0, UNKNOWN_ERROR);

  if (!send_empty_privet_token_) {
    std::string privet_access_token;
    privet_access_token = GetPrivetAccessToken();
    if (privet_access_token.empty()) {
      RequestTokenRefresh();
      return;
    }
  }

  Try();
}

void PrivetURLLoader::SetUploadData(const std::string& upload_content_type,
                                    const std::string& upload_data) {
  upload_content_type_ = upload_content_type;
  upload_data_ = upload_data;
}

void PrivetURLLoader::OnResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  if (!response_head.headers ||
      response_head.headers->response_code() == net::HTTP_SERVICE_UNAVAILABLE) {
    url_loader_.reset();
    ScheduleRetry(kPrivetTimeoutOnError);
  }
}

void PrivetURLLoader::OnDownloadedToString(
    std::unique_ptr<std::string> response_body) {
  DCHECK(!make_response_file_);

  if (CheckURLLoaderForError())
    return;

  if (delegate_->OnRawData(false, *response_body, base::FilePath()))
    return;

  // Byte ranges should only be used when we're not parsing the data as JSON.
  DCHECK(!has_byte_range_);

  // Response contains error description.
  int response_code = url_loader_->ResponseInfo()->headers->response_code();
  bool is_error_response = false;
  if (response_code != net::HTTP_OK) {
    delegate_->OnError(response_code, RESPONSE_CODE_ERROR);
    return;
  }

  base::JSONReader json_reader(base::JSON_ALLOW_TRAILING_COMMAS);
  base::Optional<base::Value> value = json_reader.ReadToValue(*response_body);
  if (!value || !value->is_dict()) {
    delegate_->OnError(0, JSON_PARSE_ERROR);
    return;
  }

  const base::Value* error_value =
      value->FindKeyOfType(kPrivetKeyError, base::Value::Type::STRING);
  if (error_value) {
    const std::string& error = error_value->GetString();
    if (error == kPrivetErrorInvalidXPrivetToken) {
      RequestTokenRefresh();
      return;
    }
    if (PrivetErrorTransient(error)) {
      if (!do_not_retry_on_transient_error_) {
        const base::Value* timeout_value =
            value->FindKeyOfType(kPrivetKeyTimeout, base::Value::Type::INTEGER);
        ScheduleRetry(timeout_value ? timeout_value->GetInt()
                                    : kPrivetDefaultTimeout);
        return;
      }
    }
    is_error_response = true;
  }

  std::unique_ptr<base::DictionaryValue> dict_value =
      base::DictionaryValue::From(
          base::Value::ToUniquePtrValue(std::move(*value)));
  delegate_->OnParsedJson(response_code, *dict_value, is_error_response);
}

void PrivetURLLoader::OnDownloadedToFile(base::FilePath path) {
  DCHECK(make_response_file_);

  if (CheckURLLoaderForError())
    return;

  bool result = delegate_->OnRawData(true, std::string(), path);
  DCHECK(result);
}

bool PrivetURLLoader::CheckURLLoaderForError() {
  switch (url_loader_->NetError()) {
    case net::OK:
      break;
    case net::ERR_ABORTED:
      delegate_->OnError(0, REQUEST_CANCELED);
      return true;
    default:
      delegate_->OnError(0, UNKNOWN_ERROR);
      return true;
  }
  int response_code = net::ERR_FAILED;
  if (url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  if (response_code == kHTTPErrorCodeInvalidXPrivetToken) {
    RequestTokenRefresh();
    return true;
  }
  if (response_code != net::HTTP_OK &&
      response_code != net::HTTP_PARTIAL_CONTENT &&
      response_code != net::HTTP_BAD_REQUEST) {
    delegate_->OnError(response_code, RESPONSE_CODE_ERROR);
    return true;
  }
  return false;
}

void PrivetURLLoader::ScheduleRetry(int timeout_seconds) {
  double random_scaling_factor =
      1 + base::RandDouble() * kPrivetMaximumTimeRandomAddition;

  int timeout_seconds_randomized =
      static_cast<int>(timeout_seconds * random_scaling_factor);

  timeout_seconds_randomized =
      std::max(timeout_seconds_randomized, kPrivetMinimumTimeout);

  // Don't wait because only error callback is going to be called.
  if (tries_ >= max_retries_)
    timeout_seconds_randomized = 0;

  if (skip_retry_timeouts_for_tests_)
    timeout_seconds_randomized = 0;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrivetURLLoader::Try, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(timeout_seconds_randomized));
}

void PrivetURLLoader::RequestTokenRefresh() {
  delegate_->OnNeedPrivetToken(base::BindOnce(&PrivetURLLoader::RefreshToken,
                                              weak_factory_.GetWeakPtr()));
}

void PrivetURLLoader::RefreshToken(const std::string& token) {
  if (token.empty()) {
    delegate_->OnError(0, TOKEN_ERROR);
  } else {
    SetTokenForHost(GetHostString(), token);
    Try();
  }
}

bool PrivetURLLoader::PrivetErrorTransient(const std::string& error) {
  return error == kPrivetErrorDeviceBusy ||
         error == kPrivetErrorPendingUserAction ||
         error == kPrivetErrorPrinterBusy;
}

}  // namespace cloud_print
