// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_HANDLER_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_HANDLER_UTIL_H_

#include <stdint.h>

namespace google {
namespace protobuf {

class MessageLite;

}  // namespace protobuf
}  // namespace google

namespace safe_browsing {

// Computes a simple hash digest over the serialized form of |message|.
// |message| must be in a canonical form. For example, fields set to their
// default values should be cleared.
uint32_t HashMessage(const google::protobuf::MessageLite& message);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_HANDLER_UTIL_H_
