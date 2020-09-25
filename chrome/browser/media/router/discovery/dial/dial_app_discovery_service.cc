// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_app_discovery_service.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "chrome/browser/media/router/data_decoder_util.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace media_router {

namespace {

GURL GetAppUrl(const media_router::MediaSinkInternal& sink,
               const std::string& app_name) {
  // The DIAL spec (Section 5.4) implies that the app URL must not have a
  // trailing slash.
  GURL partial_app_url = sink.dial_data().app_url;
  return GURL(partial_app_url.spec() + "/" + app_name);
}

void RecordDialFetchAppInfo(DialAppInfoResultCode result_code) {
  UMA_HISTOGRAM_ENUMERATION("MediaRouter.Dial.FetchAppInfo", result_code,
                            DialAppInfoResultCode::kCount);
}

}  // namespace

DialAppInfoResult::DialAppInfoResult(
    std::unique_ptr<ParsedDialAppInfo> app_info,
    DialAppInfoResultCode result_code)
    : app_info(std::move(app_info)), result_code(result_code) {}

DialAppInfoResult::DialAppInfoResult(DialAppInfoResult&& other) = default;

DialAppInfoResult::~DialAppInfoResult() = default;

DialAppDiscoveryService::DialAppDiscoveryService()
    : parser_(std::make_unique<SafeDialAppInfoParser>()) {}

DialAppDiscoveryService::~DialAppDiscoveryService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// Always query the device to get current app status.
void DialAppDiscoveryService::FetchDialAppInfo(
    const MediaSinkInternal& sink,
    const std::string& app_name,
    DialAppInfoCallback app_info_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_requests_.push_back(
      std::make_unique<DialAppDiscoveryService::PendingRequest>(
          sink, app_name, std::move(app_info_cb), this));
  pending_requests_.back()->Start();
}

void DialAppDiscoveryService::SetParserForTest(
    std::unique_ptr<SafeDialAppInfoParser> parser) {
  parser_ = std::move(parser);
}

void DialAppDiscoveryService::RemovePendingRequest(
    DialAppDiscoveryService::PendingRequest* request) {
  base::EraseIf(pending_requests_, [&request](const auto& entry) {
    return entry.get() == request;
  });
}

DialAppDiscoveryService::PendingRequest::PendingRequest(
    const MediaSinkInternal& sink,
    const std::string& app_name,
    DialAppInfoCallback app_info_cb,
    DialAppDiscoveryService* const service)
    : sink_id_(sink.sink().id()),
      app_name_(app_name),
      app_url_(GetAppUrl(sink, app_name)),
      // |base::Unretained(this)| since |fetcher_| is owned by |this|.
      fetcher_(
          base::BindOnce(&DialAppDiscoveryService::PendingRequest::
                             OnDialAppInfoFetchComplete,
                         base::Unretained(this)),
          base::BindOnce(
              &DialAppDiscoveryService::PendingRequest::OnDialAppInfoFetchError,
              base::Unretained(this))),
      app_info_cb_(std::move(app_info_cb)),
      service_(service) {}

DialAppDiscoveryService::PendingRequest::~PendingRequest() {
  DCHECK(app_info_cb_.is_null());
}

void DialAppDiscoveryService::PendingRequest::Start() {
  fetcher_.Get(app_url_);
}

void DialAppDiscoveryService::PendingRequest::OnDialAppInfoFetchComplete(
    const std::string& app_info_xml) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_->parser_->Parse(
      app_info_xml,
      base::BindOnce(
          &DialAppDiscoveryService::PendingRequest::OnDialAppInfoParsed,
          weak_ptr_factory_.GetWeakPtr()));
}

void DialAppDiscoveryService::PendingRequest::OnDialAppInfoFetchError(
    int response_code,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (response_code == net::HTTP_NOT_FOUND ||
      response_code >= net::HTTP_INTERNAL_SERVER_ERROR ||
      response_code == net::HTTP_OK) {
    RecordDialFetchAppInfo(DialAppInfoResultCode::kNotFound);
    std::move(app_info_cb_)
        .Run(sink_id_, app_name_,
             DialAppInfoResult(nullptr, DialAppInfoResultCode::kNotFound));
  } else {
    RecordDialFetchAppInfo(DialAppInfoResultCode::kNetworkError);
    std::move(app_info_cb_)
        .Run(sink_id_, app_name_,
             DialAppInfoResult(nullptr, DialAppInfoResultCode::kNetworkError));
  }
  service_->RemovePendingRequest(this);
}

void DialAppDiscoveryService::PendingRequest::OnDialAppInfoParsed(
    std::unique_ptr<ParsedDialAppInfo> parsed_app_info,
    SafeDialAppInfoParser::ParsingResult parsing_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!parsed_app_info) {
    RecordDialFetchAppInfo(DialAppInfoResultCode::kParsingError);
    std::move(app_info_cb_)
        .Run(sink_id_, app_name_,
             DialAppInfoResult(nullptr, DialAppInfoResultCode::kParsingError));
  } else {
    RecordDialFetchAppInfo(DialAppInfoResultCode::kOk);
    std::move(app_info_cb_)
        .Run(sink_id_, app_name_,
             DialAppInfoResult(std::move(parsed_app_info),
                               DialAppInfoResultCode::kOk));
  }
  service_->RemovePendingRequest(this);
}

}  // namespace media_router
