// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/cloud_binary_upload_service_factory.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/analysis_settings.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service_factory.h"
#endif

namespace safe_browsing {

namespace {

using ::enterprise_connectors::BinaryUploadRequest;
using ::enterprise_connectors::GetBrowserPolicyConnector;

}  // namespace

BinaryUploadService::Request::Request(
    ContentAnalysisCallback callback,
    enterprise_connectors::CloudOrLocalAnalysisSettings settings)
    : BinaryUploadRequest(std::move(callback),
                          std::move(settings),
                          base::BindRepeating(&GetBrowserPolicyConnector)) {}

BinaryUploadService::Request::Request(
    ContentAnalysisCallback content_analysis_callback,
    enterprise_connectors::CloudOrLocalAnalysisSettings settings,
    Request::RequestStartCallback start_callback)
    : BinaryUploadRequest(std::move(content_analysis_callback),
                          std::move(settings),
                          std::move(start_callback),
                          base::BindRepeating(&GetBrowserPolicyConnector)) {}

BinaryUploadService::Request::~Request() = default;

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
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
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
