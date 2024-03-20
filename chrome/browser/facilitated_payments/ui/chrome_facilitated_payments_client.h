// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_UI_CHROME_FACILITATED_PAYMENTS_CLIENT_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_UI_CHROME_FACILITATED_PAYMENTS_CLIENT_H_

#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver_factory.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

// Chrome implementation of `FacilitatedPaymentsClient`. `WebContents` owns 1
// instance of this class. Creates and owns
// `ContentFacilitatedPaymentsDriverFactory`.
class ChromeFacilitatedPaymentsClient
    : public payments::facilitated::FacilitatedPaymentsClient,
      public content::WebContentsUserData<ChromeFacilitatedPaymentsClient> {
 public:
  ChromeFacilitatedPaymentsClient(
      content::WebContents* web_contents,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);
  ChromeFacilitatedPaymentsClient(const ChromeFacilitatedPaymentsClient&) =
      delete;
  ChromeFacilitatedPaymentsClient& operator=(
      const ChromeFacilitatedPaymentsClient&) = delete;
  ~ChromeFacilitatedPaymentsClient() override;

 private:
  friend class content::WebContentsUserData<ChromeFacilitatedPaymentsClient>;

  // FacilitatedPaymentsClient:
  bool ShowPixPaymentPrompt(base::OnceCallback<void(bool, int64_t)>
                                on_user_decision_callback) override;

  payments::facilitated::ContentFacilitatedPaymentsDriverFactory
      driver_factory_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_UI_CHROME_FACILITATED_PAYMENTS_CLIENT_H_
