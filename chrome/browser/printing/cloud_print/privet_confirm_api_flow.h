// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_CONFIRM_API_FLOW_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_CONFIRM_API_FLOW_H_

#include <string>

#include "chrome/browser/printing/cloud_print/gcd_api_flow.h"

namespace cloud_print {

// API call flow for server-side communication with CloudPrint for registration.
class PrivetConfirmApiCallFlow : public CloudPrintApiFlowRequest {
 public:
  using ResponseCallback = base::OnceCallback<void(GCDApiFlow::Status)>;

  // Create an OAuth2-based confirmation
  PrivetConfirmApiCallFlow(const std::string& token, ResponseCallback callback);
  ~PrivetConfirmApiCallFlow() override;

  // CloudPrintApiFlowRequest implementation:
  void OnGCDApiFlowError(GCDApiFlow::Status status) override;
  void OnGCDApiFlowComplete(const base::DictionaryValue& value) override;
  GURL GetURL() override;
  NetworkTrafficAnnotation GetNetworkTrafficAnnotationType() override;

 private:
  ResponseCallback callback_;
  const std::string token_;
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_CONFIRM_API_FLOW_H_
