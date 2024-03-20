// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"

#include "content/public/browser/web_contents.h"

ChromeFacilitatedPaymentsClient::ChromeFacilitatedPaymentsClient(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : content::WebContentsUserData<ChromeFacilitatedPaymentsClient>(
          *web_contents),
      driver_factory_(web_contents,
                      /*client=*/this,
                      optimization_guide_decider) {}

ChromeFacilitatedPaymentsClient::~ChromeFacilitatedPaymentsClient() = default;

bool ChromeFacilitatedPaymentsClient::ShowPixPaymentPrompt(
    base::OnceCallback<void(bool, int64_t)> on_user_decision_callback) {
  return false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeFacilitatedPaymentsClient);
