// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accuracy_tips/accuracy_service_delegate.h"

#include "chrome/browser/ui/page_info/chrome_accuracy_tip_ui.h"
#include "components/site_engagement/content/site_engagement_service.h"

AccuracyServiceDelegate::~AccuracyServiceDelegate() = default;

AccuracyServiceDelegate::AccuracyServiceDelegate(
    site_engagement::SiteEngagementService* site_engagement_service)
    : site_engagement_service_(site_engagement_service) {}

bool AccuracyServiceDelegate::IsEngagementHigh(const GURL& url) {
  // TODO(crbug.com/1210891): Decide on the proper minimum engagement level.
  return site_engagement_service_->IsEngagementAtLeast(
      url, blink::mojom::EngagementLevel::MEDIUM);
}

void AccuracyServiceDelegate::ShowAccuracyTip(
    content::WebContents* web_contents,
    accuracy_tips::AccuracyTipStatus type,
    base::OnceCallback<void(accuracy_tips::AccuracyTipInteraction)>
        close_callback) {
  ShowAccuracyTipDialog(web_contents, type, std::move(close_callback));
}
