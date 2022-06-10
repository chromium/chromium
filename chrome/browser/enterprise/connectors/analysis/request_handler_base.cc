// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/request_handler_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service_factory.h"

namespace enterprise_connectors {

RequestHandlerBase::RequestHandlerBase(
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    GURL url,
    safe_browsing::DeepScanAccessPoint access_point)
    : upload_service_(upload_service),
      profile_(profile),
      analysis_settings_(analysis_settings),
      url_(url),
      access_point_(access_point) {}

RequestHandlerBase::~RequestHandlerBase() = default;

bool RequestHandlerBase::UploadData() {
  upload_start_time_ = base::TimeTicks::Now();
  return UploadDataImpl();
}

void RequestHandlerBase::PrepareRequest(
    enterprise_connectors::AnalysisConnector connector,
    safe_browsing::BinaryUploadService::Request* request) {
  request->set_device_token(analysis_settings_.dm_token);
  request->set_analysis_connector(connector);
  request->set_email(safe_browsing::GetProfileEmail(profile_));
  request->set_url(url_.spec());
  request->set_tab_url(url_);
  request->set_per_profile_request(analysis_settings_.per_profile);
  for (const auto& tag : analysis_settings_.tags)
    request->add_tag(tag.first);
  if (analysis_settings_.client_metadata)
    request->set_client_metadata(*analysis_settings_.client_metadata);
}

bool RequestHandlerBase::ResultShouldAllowDataUse(
    safe_browsing::BinaryUploadService::Result result) {
  using safe_browsing::BinaryUploadService;
  // Keep this implemented as a switch instead of a simpler if statement so that
  // new values added to BinaryUploadService::Result cause a compiler error.
  switch (result) {
    case BinaryUploadService::Result::SUCCESS:
    case BinaryUploadService::Result::UPLOAD_FAILURE:
    case BinaryUploadService::Result::TIMEOUT:
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
    case BinaryUploadService::Result::TOO_MANY_REQUESTS:
    // UNAUTHORIZED allows data usage since it's a result only obtained if the
    // browser is not authorized to perform deep scanning. It does not make
    // sense to block data in this situation since no actual scanning of the
    // data was performed, so it's allowed.
    case BinaryUploadService::Result::UNAUTHORIZED:
    case BinaryUploadService::Result::UNKNOWN:
      return true;

    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return !analysis_settings_.block_large_files;

    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return !analysis_settings_.block_password_protected_files;

    case BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE:
      return !analysis_settings_.block_unsupported_file_types;
  }
}

safe_browsing::EventResult RequestHandlerBase::CalculateEventResult(
    bool allowed_by_scan_result,
    bool should_warn) {
  bool wait_for_verdict = analysis_settings_.block_until_verdict ==
                          enterprise_connectors::BlockUntilVerdict::BLOCK;
  return (allowed_by_scan_result || !wait_for_verdict)
             ? safe_browsing::EventResult::ALLOWED
             : (should_warn ? safe_browsing::EventResult::WARNED
                            : safe_browsing::EventResult::BLOCKED);
}

bool RequestHandlerBase::ContentAnalysisActionAllowsDataUse(
    enterprise_connectors::TriggeredRule::Action action) {
  switch (action) {
    case enterprise_connectors::TriggeredRule::ACTION_UNSPECIFIED:
    case enterprise_connectors::TriggeredRule::REPORT_ONLY:
      return true;
    case enterprise_connectors::TriggeredRule::WARN:
    case enterprise_connectors::TriggeredRule::BLOCK:
      return false;
  }
}

safe_browsing::BinaryUploadService*
RequestHandlerBase::GetBinaryUploadService() {
  return upload_service_;
}

}  // namespace enterprise_connectors
