// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/web_payments_observer.h"

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "components/payments/core/features.h"
#include "components/payments/core/web_payments_telemetry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace payments {

WebPaymentsObserver::WebPaymentsObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebPaymentsObserver::~WebPaymentsObserver() = default;

void WebPaymentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kThreeDSecureTelemetry)) {
    RecordThreeDSecureTelemetry(navigation_handle);
  }
}

void WebPaymentsObserver::RecordThreeDSecureTelemetry(
    content::NavigationHandle* navigation_handle) {
  // Filter for only HTTP POST requests from form submissions.
  if (!navigation_handle->IsPost() || !navigation_handle->IsFormSubmission()) {
    return;
  }

  scoped_refptr<const network::ResourceRequestBody> post_data =
      navigation_handle->GetPostData();
  if (!post_data || !post_data->elements()) {
    return;
  }

  for (const network::DataElement& element : *post_data->elements()) {
    if (const auto* bytes_element =
            element.TryAs<network::DataElementBytes>()) {
      RecordThreeDSecureTelemetryFromFormData(
          bytes_element->AsStringView(),
          navigation_handle->GetNextPageUkmSourceId());
    }
  }
}

}  // namespace payments
