// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_CHROME_ABOUT_THIS_SITE_SERVICE_CLIENT_H_
#define CHROME_BROWSER_PAGE_INFO_CHROME_ABOUT_THIS_SITE_SERVICE_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/page_info/core/about_this_site_service.h"

class PrefService;

class ChromeAboutThisSiteServiceClient
    : public page_info::AboutThisSiteService::Client {
 public:
  // `optimization_guide_decider` may be nullptr.
  explicit ChromeAboutThisSiteServiceClient(
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      bool is_off_the_record,
      PrefService* prefs);
  ~ChromeAboutThisSiteServiceClient() override;

  ChromeAboutThisSiteServiceClient(const ChromeAboutThisSiteServiceClient&) =
      delete;
  ChromeAboutThisSiteServiceClient& operator=(
      const ChromeAboutThisSiteServiceClient&) = delete;

  // page_info::AboutThisSiteService::Client:
  bool IsOptimizationGuideAllowed() override;
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::OptimizationMetadata* optimization_metadata) override;

 private:
  const raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;
  const bool is_off_the_record_;
  const raw_ptr<PrefService> prefs_;
};

#endif  // CHROME_BROWSER_PAGE_INFO_CHROME_ABOUT_THIS_SITE_SERVICE_CLIENT_H_
