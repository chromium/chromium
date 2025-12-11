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
