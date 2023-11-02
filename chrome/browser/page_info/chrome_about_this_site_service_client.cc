// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/chrome_about_this_site_service_client.h"

#include <memory>

#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_metadata.h"

ChromeAboutThisSiteServiceClient::ChromeAboutThisSiteServiceClient(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    bool is_off_the_record,
    PrefService* prefs)
    : optimization_guide_decider_(optimization_guide_decider),
      is_off_the_record_(is_off_the_record),
      prefs_(prefs) {
  if (optimization_guide_decider_) {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::ABOUT_THIS_SITE});
  }
}

ChromeAboutThisSiteServiceClient::~ChromeAboutThisSiteServiceClient() = default;

optimization_guide::OptimizationGuideDecision
ChromeAboutThisSiteServiceClient::CanApplyOptimization(
    const GURL& url,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  if (!optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          is_off_the_record_, prefs_)) {
    return optimization_guide::OptimizationGuideDecision::kUnknown;
  }
  return optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::ABOUT_THIS_SITE, optimization_metadata);
}
