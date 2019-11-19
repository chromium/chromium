// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_SSL_VALIDITY_CHECKER_H_
#define CHROME_BROWSER_PAYMENTS_SSL_VALIDITY_CHECKER_H_

#include <string>

#include "base/macros.h"

namespace content {
class WebContents;
}

namespace payments {

class SslValidityChecker {
 public:
  // Returns a developer-facing error message for invalid SSL certificate state
  // or an empty string when the SSL certificate is valid. Only EV_SECURE,
  // SECURE, and SECURE_WITH_POLICY_INSTALLED_CERT are considered valid for web
  // payments, unless --ignore-certificate-errors is specified on the command
  // line.
  //
  // The |web_contents| parameter should not be null. A null
  // |web_contents| parameter will return an "Invalid certificate" error
  // message.
  static std::string GetInvalidSslCertificateErrorMessage(
      content::WebContents* web_contents);

  // Whether the given page should be allowed to be displayed in a payment
  // handler window.
  static bool IsValidPageInPaymentHandlerWindow(
      content::WebContents* web_contents);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SslValidityChecker);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_SSL_VALIDITY_CHECKER_H_
