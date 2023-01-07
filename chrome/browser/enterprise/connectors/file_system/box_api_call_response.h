// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_RESPONSE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_RESPONSE_H_

#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"

namespace enterprise_connectors {

struct BoxApiCallResponse {
  bool success;          // Whether request returned success.
  int net_or_http_code;  // NET_ERROR code (< 0) or HTTP_STATUS code (> 0).
  std::string box_error_code;
  std::string box_request_id;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_API_CALL_RESPONSE_H_
