// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/payment_handler_navigation_throttle.h"

#include <cstddef>
#include <string>

#include "chrome/common/pdf_util.h"
#include "components/payments/content/payments_userdata_key.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace payments {
constexpr char kPdfMimeType[] = "application/pdf";

PaymentHandlerNavigationThrottle::PaymentHandlerNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

PaymentHandlerNavigationThrottle::~PaymentHandlerNavigationThrottle() = default;

const char* PaymentHandlerNavigationThrottle::GetNameForLogging() {
  return "PaymentHandlerNavigationThrottle";
}

// static
std::unique_ptr<PaymentHandlerNavigationThrottle>
PaymentHandlerNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (!handle->IsInMainFrame())
    return nullptr;

  if (!handle->GetWebContents()->GetUserData(
          kPaymentHandlerWebContentsUserDataKey)) {
    return nullptr;
  }
  return std::make_unique<PaymentHandlerNavigationThrottle>(handle);
}

content::NavigationThrottle::ThrottleCheckResult
PaymentHandlerNavigationThrottle::WillProcessResponse() {
  const net::HttpResponseHeaders* response_headers =
      navigation_handle()->GetResponseHeaders();
  if (!response_headers)
    return PROCEED;

  std::string mime_type;
  response_headers->GetMimeType(&mime_type);
  if (mime_type != kPdfMimeType)
    return PROCEED;

  return BLOCK_RESPONSE;
}
}  // namespace payments
