// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_confirm_api_flow.h"

#include "base/values.h"
#include "chrome/browser/printing/cloud_print/gcd_api_flow.h"
#include "chrome/browser/printing/cloud_print/gcd_constants.h"
#include "chrome/browser/printing/cloud_print/privet_constants.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "net/base/url_util.h"

namespace cloud_print {

PrivetConfirmApiCallFlow::PrivetConfirmApiCallFlow(const std::string& token,
                                                   ResponseCallback callback)
    : callback_(std::move(callback)), token_(token) {}

PrivetConfirmApiCallFlow::~PrivetConfirmApiCallFlow() {
}

void PrivetConfirmApiCallFlow::OnGCDApiFlowError(GCDApiFlow::Status status) {
  if (callback_)
    std::move(callback_).Run(status);
}

void PrivetConfirmApiCallFlow::OnGCDApiFlowComplete(
    const base::DictionaryValue& value) {
  if (!callback_)
    return;

  bool success = false;
  if (!value.GetBoolean(cloud_print::kSuccessValue, &success)) {
    std::move(callback_).Run(GCDApiFlow::ERROR_MALFORMED_RESPONSE);
    return;
  }

  std::move(callback_).Run(success ? GCDApiFlow::SUCCESS
                                   : GCDApiFlow::ERROR_FROM_SERVER);
}

GURL PrivetConfirmApiCallFlow::GetURL() {
  return net::AppendQueryParameter(
      cloud_devices::GetCloudPrintRelativeURL("confirm"), "token", token_);
}

GCDApiFlow::Request::NetworkTrafficAnnotation
PrivetConfirmApiCallFlow::GetNetworkTrafficAnnotationType() {
  return TYPE_PRIVET_REGISTER;
}

}  // namespace cloud_print
