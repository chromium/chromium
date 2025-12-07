// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/request_handler_base.h"

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/safe_browsing/core/common/features.h"

namespace enterprise_connectors {

RequestHandlerBase::RequestHandlerBase(
    ContentAnalysisInfo* content_analysis_info,
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    GURL url,
    DeepScanAccessPoint access_point)
    : content_analysis_info_(content_analysis_info),
      upload_service_(upload_service ? upload_service->AsWeakPtr() : nullptr),
      profile_(profile),
      url_(url),
      access_point_(access_point) {}

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

safe_browsing::BinaryUploadService*
RequestHandlerBase::GetBinaryUploadService() {
  return upload_service_.get();
}

}  // namespace enterprise_connectors
