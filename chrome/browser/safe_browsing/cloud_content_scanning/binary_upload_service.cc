// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/common/strings.h"
#include "net/base/url_util.h"

namespace safe_browsing {
namespace {

constexpr char kCloudBinaryUploadServiceUrlFlag[] = "binary-upload-service-url";

absl::optional<GURL> GetUrlOverride() {
  // Ignore this flag on Stable and Beta to avoid abuse.
  if (!g_browser_process || !g_browser_process->browser_policy_connector()
                                 ->IsCommandLineSwitchSupported()) {
    return absl::nullopt;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kCloudBinaryUploadServiceUrlFlag)) {
    GURL url = GURL(
        command_line->GetSwitchValueASCII(kCloudBinaryUploadServiceUrlFlag));
    if (url.is_valid())
      return url;
    else
      LOG(ERROR) << "--binary-upload-service-url is set to an invalid URL";
  }

  return absl::nullopt;
}

}  // namespace

BinaryUploadService::Request::Data::Data() = default;
BinaryUploadService::Request::Data::Data(Data&&) = default;
BinaryUploadService::Request::Data&
BinaryUploadService::Request::Data::operator=(
    BinaryUploadService::Request::Data&& other) = default;
BinaryUploadService::Request::Data::~Data() = default;

BinaryUploadService::Request::Request(ContentAnalysisCallback callback,
                                      GURL url)
    : content_analysis_callback_(std::move(callback)), url_(url) {}

BinaryUploadService::Request::~Request() = default;

void BinaryUploadService::Request::set_tab_url(const GURL& tab_url) {
  tab_url_ = tab_url;
}

const GURL& BinaryUploadService::Request::tab_url() const {
  return tab_url_;
}

void BinaryUploadService::Request::set_per_profile_request(
    bool per_profile_request) {
  per_profile_request_ = per_profile_request;
}

bool BinaryUploadService::Request::per_profile_request() const {
  return per_profile_request_;
}

void BinaryUploadService::Request::set_fcm_token(const std::string& token) {
  content_analysis_request_.set_fcm_notification_token(token);
}

void BinaryUploadService::Request::set_device_token(const std::string& token) {
  content_analysis_request_.set_device_token(token);
}

void BinaryUploadService::Request::set_request_token(const std::string& token) {
  content_analysis_request_.set_request_token(token);
}

void BinaryUploadService::Request::set_filename(const std::string& filename) {
  content_analysis_request_.mutable_request_data()->set_filename(filename);
}

void BinaryUploadService::Request::set_digest(const std::string& digest) {
  content_analysis_request_.mutable_request_data()->set_digest(digest);
}

void BinaryUploadService::Request::clear_dlp_scan_request() {
  auto* tags = content_analysis_request_.mutable_tags();
  auto it = std::find(tags->begin(), tags->end(), "dlp");
  if (it != tags->end())
    tags->erase(it);
}

void BinaryUploadService::Request::set_analysis_connector(
    enterprise_connectors::AnalysisConnector connector) {
  content_analysis_request_.set_analysis_connector(connector);
}

void BinaryUploadService::Request::set_url(const std::string& url) {
  content_analysis_request_.mutable_request_data()->set_url(url);
}

void BinaryUploadService::Request::set_csd(ClientDownloadRequest csd) {
  *content_analysis_request_.mutable_request_data()->mutable_csd() =
      std::move(csd);
}

void BinaryUploadService::Request::add_tag(const std::string& tag) {
  content_analysis_request_.add_tags(tag);
}

void BinaryUploadService::Request::set_email(const std::string& email) {
  content_analysis_request_.mutable_request_data()->set_email(email);
}

void BinaryUploadService::Request::set_client_metadata(
    enterprise_connectors::ClientMetadata metadata) {
  *content_analysis_request_.mutable_client_metadata() = std::move(metadata);
}

void BinaryUploadService::Request::set_content_type(const std::string& type) {
  content_analysis_request_.mutable_request_data()->set_content_type(type);
}

enterprise_connectors::AnalysisConnector
BinaryUploadService::Request::analysis_connector() {
  return content_analysis_request_.analysis_connector();
}

const std::string& BinaryUploadService::Request::device_token() const {
  return content_analysis_request_.device_token();
}

const std::string& BinaryUploadService::Request::request_token() const {
  return content_analysis_request_.request_token();
}

const std::string& BinaryUploadService::Request::fcm_notification_token()
    const {
  return content_analysis_request_.fcm_notification_token();
}

const std::string& BinaryUploadService::Request::filename() const {
  return content_analysis_request_.request_data().filename();
}

const std::string& BinaryUploadService::Request::digest() const {
  return content_analysis_request_.request_data().digest();
}

const std::string& BinaryUploadService::Request::content_type() const {
  return content_analysis_request_.request_data().content_type();
}

void BinaryUploadService::Request::FinishRequest(
    Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  std::move(content_analysis_callback_).Run(result, response);
}

void BinaryUploadService::Request::SerializeToString(
    std::string* destination) const {
  content_analysis_request_.SerializeToString(destination);
}

GURL BinaryUploadService::Request::GetUrlWithParams() const {
  GURL url = GetUrlOverride().value_or(url_);

  url = net::AppendQueryParameter(url, enterprise::kUrlParamDeviceToken,
                                  device_token());

  std::string connector;
  switch (content_analysis_request_.analysis_connector()) {
    case enterprise_connectors::FILE_ATTACHED:
      connector = "OnFileAttached";
      break;
    case enterprise_connectors::FILE_DOWNLOADED:
      connector = "OnFileDownloaded";
      break;
    case enterprise_connectors::BULK_DATA_ENTRY:
      connector = "OnBulkDataEntry";
      break;
    case enterprise_connectors::PRINT:
      connector = "OnPrint";
      break;
    case enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED:
      break;
  }
  if (!connector.empty()) {
    url = net::AppendQueryParameter(url, enterprise::kUrlParamConnector,
                                    connector);
  }

  for (const std::string& tag : content_analysis_request_.tags())
    url = net::AppendQueryParameter(url, enterprise::kUrlParamTag, tag);

  return url;
}

bool BinaryUploadService::Request::IsAuthRequest() const {
  return false;
}

const std::string& BinaryUploadService::Request::access_token() const {
  return access_token_;
}

void BinaryUploadService::Request::set_access_token(
    const std::string& access_token) {
  access_token_ = access_token;
}

}  // namespace safe_browsing
