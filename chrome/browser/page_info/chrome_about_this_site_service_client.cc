// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_info/chrome_about_this_site_service_client.h"

#include <memory>

#include "components/optimization_guide/core/optimization_metadata.h"

ChromeAboutThisSiteServiceClient::ChromeAboutThisSiteServiceClient(
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : optimization_guide_decider_(optimization_guide_decider) {
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
  return optimization_guide_decider_->CanApplyOptimization(
      url, optimization_guide::proto::ABOUT_THIS_SITE, optimization_metadata);
}
