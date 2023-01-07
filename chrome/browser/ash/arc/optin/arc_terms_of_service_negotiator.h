// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_NEGOTIATOR_H_
#define CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_NEGOTIATOR_H_

#include "base/functional/callback.h"

namespace arc {

// Interface to handle the Terms-of-service agreement user action.
class ArcTermsOfServiceNegotiator {
 public:
  ArcTermsOfServiceNegotiator();

  ArcTermsOfServiceNegotiator(const ArcTermsOfServiceNegotiator&) = delete;
  ArcTermsOfServiceNegotiator& operator=(const ArcTermsOfServiceNegotiator&) =
      delete;

  virtual ~ArcTermsOfServiceNegotiator();

  // Invokes the|callback| asynchronously with "|accepted| = true" if user
  // accepts ToS. If user explicitly rejects ToS, invokes |callback| with
  // |accepted| = false. Deleting this instance cancels the operation, so
  // |callback| will never be invoked then.
  using NegotiationCallback = base::OnceCallback<void(bool accepted)>;
  void StartNegotiation(NegotiationCallback callback);

 protected:
  // Reports result of negotiation via callback and then resets it. If
  // |accepted| is true then this means terms of service were accepted.
  void ReportResult(bool accepted);

 private:
  // Performs implementation specific action to start negotiation.
  virtual void StartNegotiationImpl() = 0;

  NegotiationCallback pending_callback_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_OPTIN_ARC_TERMS_OF_SERVICE_NEGOTIATOR_H_
