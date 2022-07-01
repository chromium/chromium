// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/request_handler_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

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
  if (analysis_settings_.cloud_or_local_settings.is_cloud_analysis()) {
    request->set_device_token(
        analysis_settings_.cloud_or_local_settings.dm_token());
  }
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

safe_browsing::BinaryUploadService*
RequestHandlerBase::GetBinaryUploadService() {
  return upload_service_;
}

}  // namespace enterprise_connectors
