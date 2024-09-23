// Copyright 2022 The Chromium Authors
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
    const std::string& source,
    const std::string& destination,
    const std::string& user_action_id,
    const std::string& tab_title,
    uint64_t user_action_requests_count,
    safe_browsing::DeepScanAccessPoint access_point,
    ContentAnalysisRequest::Reason reason)
    : upload_service_(upload_service ? upload_service->AsWeakPtr() : nullptr),
      profile_(profile),
      analysis_settings_(analysis_settings),
      url_(url),
      source_(source),
      destination_(destination),
      user_action_id_(user_action_id),
      tab_title_(tab_title),
      user_action_requests_count_(user_action_requests_count),
      access_point_(access_point),
      reason_(reason) {}

RequestHandlerBase::~RequestHandlerBase() = default;

bool RequestHandlerBase::UploadData() {
  upload_start_time_ = base::TimeTicks::Now();
  return UploadDataImpl();
}

void RequestHandlerBase::AppendFinalActionsTo(
    std::map<std::string, ContentAnalysisAcknowledgement::FinalAction>*
        final_actions) {
  DCHECK(final_actions);
  final_actions->insert(
      std::make_move_iterator(request_tokens_to_ack_final_actions_.begin()),
      std::make_move_iterator(request_tokens_to_ack_final_actions_.end()));

  request_tokens_to_ack_final_actions_.clear();
}

void RequestHandlerBase::PrepareRequest(
    enterprise_connectors::AnalysisConnector connector,
    safe_browsing::BinaryUploadService::Request* request) {
  if (analysis_settings_->cloud_or_local_settings.is_cloud_analysis()) {
    request->set_device_token(
        analysis_settings_->cloud_or_local_settings.dm_token());
  }
  if (analysis_settings_->cloud_or_local_settings.is_local_analysis()) {
    request->set_user_action_id(user_action_id_);
    request->set_user_action_requests_count(user_action_requests_count_);
    request->set_tab_title(tab_title_);
  }

  request->set_analysis_connector(connector);
  request->set_email(GetProfileEmail(profile_));
  request->set_url(url_.spec());
  request->set_source(source_);
  request->set_destination(destination_);
  request->set_tab_url(url_);
  request->set_per_profile_request(analysis_settings_->per_profile);
  for (const auto& tag : analysis_settings_->tags) {
    request->add_tag(tag.first);
  }

  if (analysis_settings_->client_metadata) {
    request->set_client_metadata(*analysis_settings_->client_metadata);
  }

  if (reason_ != ContentAnalysisRequest::UNKNOWN) {
    request->set_reason(reason_);
  }

  request->set_blocking(analysis_settings_->block_until_verdict !=
                        BlockUntilVerdict::kNoBlock);
}

safe_browsing::BinaryUploadService*
RequestHandlerBase::GetBinaryUploadService() {
  return upload_service_.get();
}

}  // namespace enterprise_connectors
