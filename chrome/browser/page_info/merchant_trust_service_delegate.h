// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_MERCHANT_TRUST_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_PAGE_INFO_MERCHANT_TRUST_SERVICE_DELEGATE_H_

#include "components/page_info/core/merchant_trust_service.h"

class Profile;

class MerchantTrustServiceDelegate
    : public page_info::MerchantTrustService::Delegate {
 public:
  explicit MerchantTrustServiceDelegate(Profile* profile);
  ~MerchantTrustServiceDelegate() override;

  MerchantTrustServiceDelegate(const MerchantTrustServiceDelegate&) = delete;
  MerchantTrustServiceDelegate& operator=(const MerchantTrustServiceDelegate&) =
      delete;

  // Attempt to show an evaluation survey. It will show either a control or an
  // experiment survey depending on the feature state.
  void ShowEvaluationSurvey() override;

  double GetSiteEngagementScore(const GURL url) override;

 private:
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_PAGE_INFO_MERCHANT_TRUST_SERVICE_DELEGATE_H_
