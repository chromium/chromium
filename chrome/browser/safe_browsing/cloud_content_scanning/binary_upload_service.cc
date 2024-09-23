// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "components/enterprise/common/strings.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service_factory.h"
#endif

namespace safe_browsing {
namespace {

constexpr char kCloudBinaryUploadServiceUrlFlag[] = "binary-upload-service-url";

std::optional<GURL> GetUrlOverride() {
  // Ignore this flag on Stable and Beta to avoid abuse.
  if (!g_browser_process || !g_browser_process->browser_policy_connector()
                                 ->IsCommandLineSwitchSupported()) {
    return std::nullopt;
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

  return std::nullopt;
}

}  // namespace

std::string BinaryUploadService::ResultToString(Result result) {
  switch (result) {
    case Result::UNKNOWN:
      return "UNKNOWN";
    case Result::SUCCESS:
      return "SUCCESS";
    case Result::UPLOAD_FAILURE:
      return "UPLOAD_FAILURE";
    case Result::TIMEOUT:
      return "TIMEOUT";
    case Result::FILE_TOO_LARGE:
      return "FILE_TOO_LARGE";
    case Result::FAILED_TO_GET_TOKEN:
      return "FAILED_TO_GET_TOKEN";
    case Result::UNAUTHORIZED:
      return "UNAUTHORIZED";
    case Result::FILE_ENCRYPTED:
      return "FILE_ENCRYPTED";
    case Result::TOO_MANY_REQUESTS:
      return "TOO_MANY_REQUESTS";
    case Result::INCOMPLETE_RESPONSE:
      return "INCOMPLETE_RESPONSE";
  }
}

BinaryUploadService::Request::Data::Data() = default;

BinaryUploadService::Request::Data::Data(const Data& other) {
  operator=(other);
}

BinaryUploadService::Request::Data::Data(Data&&) = default;

BinaryUploadService::Request::Data&
BinaryUploadService::Request::Data::operator=(
    const BinaryUploadService::Request::Data& other) {
  contents = other.contents;
  path = other.path;
  hash = other.hash;
  size = other.size;
  mime_type = other.mime_type;
  page = other.page.Duplicate();
  return *this;
}

BinaryUploadService::Request::Data&
BinaryUploadService::Request::Data::operator=(
    BinaryUploadService::Request::Data&& other) = default;
BinaryUploadService::Request::Data::~Data() = default;

BinaryUploadService::Request::Request(
    ContentAnalysisCallback callback,
    enterprise_connectors::CloudOrLocalAnalysisSettings settings)
    : content_analysis_callback_(std::move(callback)),
      cloud_or_local_settings_(std::move(settings)) {}

BinaryUploadService::Request::Request(
    ContentAnalysisCallback content_analysis_callback,
    enterprise_connectors::CloudOrLocalAnalysisSettings settings,
    Request::RequestStartCallback start_callback)
    : content_analysis_callback_(std::move(content_analysis_callback)),
      request_start_callback_(std::move(start_callback)),
      cloud_or_local_settings_(std::move(settings)) {}

BinaryUploadService::Request::~Request() = default;

void BinaryUploadService::Request::set_id(Id id) {
  id_ = id;
}

BinaryUploadService::Request::Id BinaryUploadService::Request::id() const {
  return id_;
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

void BinaryUploadService::Request::set_filename(const std::string& filename) {
  content_analysis_request_.mutable_request_data()->set_filename(filename);
}

void BinaryUploadService::Request::set_digest(const std::string& digest) {
  content_analysis_request_.mutable_request_data()->set_digest(digest);
}

void BinaryUploadService::Request::clear_dlp_scan_request() {
  auto* tags = content_analysis_request_.mutable_tags();
  auto it = base::ranges::find(*tags, "dlp");
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

void BinaryUploadService::Request::set_source(const std::string& source) {
  content_analysis_request_.mutable_request_data()->set_source(source);
}

void BinaryUploadService::Request::set_destination(
    const std::string& destination) {
  content_analysis_request_.mutable_request_data()->set_destination(
      destination);
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

void BinaryUploadService::Request::set_tab_title(const std::string& tab_title) {
  content_analysis_request_.mutable_request_data()->set_tab_title(tab_title);
}

void BinaryUploadService::Request::set_user_action_id(
    const std::string& user_action_id) {
  content_analysis_request_.set_user_action_id(user_action_id);
}

void BinaryUploadService::Request::set_user_action_requests_count(
    uint64_t user_action_requests_count) {
  content_analysis_request_.set_user_action_requests_count(
      user_action_requests_count);
}

void BinaryUploadService::Request::set_tab_url(const GURL& tab_url) {
  content_analysis_request_.mutable_request_data()->set_tab_url(tab_url.spec());
}

void BinaryUploadService::Request::set_printer_name(
    const std::string& printer_name) {
  content_analysis_request_.mutable_request_data()
      ->mutable_print_metadata()
      ->set_printer_name(printer_name);
}

void BinaryUploadService::Request::set_printer_type(
    enterprise_connectors::ContentMetaData::PrintMetadata::PrinterType
        printer_type) {
  content_analysis_request_.mutable_request_data()
      ->mutable_print_metadata()
      ->set_printer_type(printer_type);
}

void BinaryUploadService::Request::set_password(const std::string& password) {
  content_analysis_request_.mutable_request_data()->set_decryption_key(
      password);
}

void BinaryUploadService::Request::set_reason(
    enterprise_connectors::ContentAnalysisRequest::Reason reason) {
  content_analysis_request_.set_reason(reason);
}

void BinaryUploadService::Request::set_require_metadata_verdict(
    bool require_metadata_verdict) {
  content_analysis_request_.set_require_metadata_verdict(
      require_metadata_verdict);
}

void BinaryUploadService::Request::set_blocking(bool blocking) {
  content_analysis_request_.set_blocking(blocking);
}

std::string BinaryUploadService::Request::SetRandomRequestToken() {
  DCHECK(request_token().empty());
  content_analysis_request_.set_request_token(
      base::HexEncode(base::RandBytesAsVector(128)));
  return content_analysis_request_.request_token();
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

const std::string& BinaryUploadService::Request::user_action_id() const {
  return content_analysis_request_.user_action_id();
}

const std::string& BinaryUploadService::Request::tab_title() const {
  return content_analysis_request_.request_data().tab_title();
}

const std::string& BinaryUploadService::Request::printer_name() const {
  return content_analysis_request_.request_data()
      .print_metadata()
      .printer_name();
}

uint64_t BinaryUploadService::Request::user_action_requests_count() const {
  return content_analysis_request_.user_action_requests_count();
}

GURL BinaryUploadService::Request::tab_url() const {
  if (!content_analysis_request_.has_request_data()) {
    return GURL();
  }
  return GURL(content_analysis_request_.request_data().tab_url());
}

base::optional_ref<const std::string> BinaryUploadService::Request::password()
    const {
  return content_analysis_request_.request_data().has_decryption_key()
             ? base::optional_ref(
                   content_analysis_request_.request_data().decryption_key())
             : std::nullopt;
}

enterprise_connectors::ContentAnalysisRequest::Reason
BinaryUploadService::Request::reason() const {
  return content_analysis_request_.reason();
}

bool BinaryUploadService::Request::blocking() const {
  return content_analysis_request_.blocking();
}

void BinaryUploadService::Request::StartRequest() {
  if (!request_start_callback_.is_null()) {
    std::move(request_start_callback_).Run(*this);
  }
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
  DCHECK(absl::holds_alternative<enterprise_connectors::CloudAnalysisSettings>(
      cloud_or_local_settings_));

  GURL url = GetUrlOverride().value_or(cloud_or_local_settings_.analysis_url());
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
    case enterprise_connectors::FILE_TRANSFER:
      connector = "OnFileTransfer";
      break;
    case enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED:
      break;
  }
  if (!connector.empty()) {
    url = net::AppendQueryParameter(url, enterprise::kUrlParamConnector,
                                    connector);
  }

  for (const std::string& tag : content_analysis_request_.tags()) {
    url = net::AppendQueryParameter(url, enterprise::kUrlParamTag, tag);
  }

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

BinaryUploadService::Ack::Ack(
    enterprise_connectors::CloudOrLocalAnalysisSettings settings)
    : cloud_or_local_settings_(std::move(settings)) {}

BinaryUploadService::Ack::~Ack() = default;

void BinaryUploadService::Ack::set_request_token(const std::string& token) {
  ack_.set_request_token(token);
}

void BinaryUploadService::Ack::set_status(
    enterprise_connectors::ContentAnalysisAcknowledgement::Status status) {
  ack_.set_status(status);
}

void BinaryUploadService::Ack::set_final_action(
    enterprise_connectors::ContentAnalysisAcknowledgement::FinalAction
        final_action) {
  ack_.set_final_action(final_action);
}

BinaryUploadService::CancelRequests::CancelRequests(
    enterprise_connectors::CloudOrLocalAnalysisSettings settings)
    : cloud_or_local_settings_(std::move(settings)) {}

BinaryUploadService::CancelRequests::~CancelRequests() = default;

void BinaryUploadService::CancelRequests::set_user_action_id(
    const std::string& user_action_id) {
  user_action_id_ = user_action_id;
}

// static
BinaryUploadService* BinaryUploadService::GetForProfile(
    Profile* profile,
    const enterprise_connectors::AnalysisSettings& settings) {
  // Local content analysis is supported only on desktop platforms.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (settings.cloud_or_local_settings.is_cloud_analysis()) {
    return CloudBinaryUploadServiceFactory::GetForProfile(profile);
  } else {
    return enterprise_connectors::LocalBinaryUploadServiceFactory::
        GetForProfile(profile);
  }
#else
  DCHECK(settings.cloud_or_local_settings.is_cloud_analysis());
  return CloudBinaryUploadServiceFactory::GetForProfile(profile);
#endif
}

}  // namespace safe_browsing
