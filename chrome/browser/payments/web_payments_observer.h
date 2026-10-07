// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_WEB_PAYMENTS_OBSERVER_H_
#define CHROME_BROWSER_PAYMENTS_WEB_PAYMENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace payments {

// WebPaymentsObserver observes changes in the web contents, to measure web
// payments flows.
//
// In order to help the payments industry (via the W3C Web Payments working
// Group) understand and improve the user experience of payment flows in
// Chromium, this class records basic anonymized metrics (for clients which are
// opted into metrics collection). Only technical information for such payment
// flows is ever recorded, never any personal information or transaction
// details.
//
// Currently we record such metrics for:
//
// * 3D-Secure payment challenges (see RecordThreeDSecureTelemetry).
class WebPaymentsObserver : public content::WebContentsObserver {
 public:
  explicit WebPaymentsObserver(content::WebContents* web_contents);
  WebPaymentsObserver(const WebPaymentsObserver&) = delete;
  WebPaymentsObserver& operator=(const WebPaymentsObserver&) = delete;
  ~WebPaymentsObserver() override;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Records metrics for 3DS (3D-Secure) Challenge Requests and Challenge
  // Responses. The metrics are used to help the payments industry understand
  // and improve the user experience of 3D-Secure payments challenges in
  // Chromium.
  //
  // It does this by filtering for HTTP POST requests from form
  // submissions that contain `creq` (for Challenge Requests) or `cres` (for
  // Challenge Responses) in the form data, as per the 3D-Secure specification.
  // The total count of these requests and responses are recorded.
  //
  // In addition, it will parse the `cres` value and record metrics on the
  // outcome of the response from the `transStatus` field.
  void RecordThreeDSecureTelemetry(
      content::NavigationHandle* navigation_handle);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_WEB_PAYMENTS_OBSERVER_H_
