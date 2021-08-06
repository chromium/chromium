// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCURACY_TIPS_ACCURACY_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_ACCURACY_TIPS_ACCURACY_SERVICE_DELEGATE_H_

#include "components/accuracy_tips/accuracy_service.h"

namespace site_engagement {
class SiteEngagementService;
}

class AccuracyServiceDelegate
    : public accuracy_tips::AccuracyService::Delegate {
 public:
  explicit AccuracyServiceDelegate(
      site_engagement::SiteEngagementService* site_engagement_service);
  ~AccuracyServiceDelegate() override;

  AccuracyServiceDelegate(const AccuracyServiceDelegate&) = delete;
  AccuracyServiceDelegate& operator=(const AccuracyServiceDelegate&) = delete;

  bool IsEngagementHigh(const GURL& url) override;

 private:
  site_engagement::SiteEngagementService* site_engagement_service_;
};

#endif  // CHROME_BROWSER_ACCURACY_TIPS_ACCURACY_SERVICE_DELEGATE_H_
