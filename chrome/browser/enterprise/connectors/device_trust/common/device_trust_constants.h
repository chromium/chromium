// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_DEVICE_TRUST_CONSTANTS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_DEVICE_TRUST_CONSTANTS_H_

#include "base/time/time.h"

namespace enterprise_connectors {

namespace errors {

extern const char kMissingCoreSignals[];
extern const char kMissingSigningKey[];
extern const char kBadChallengeFormat[];
extern const char kBadChallengeSource[];
extern const char kFailedToSerializeKeyInfo[];
extern const char kFailedToGenerateResponse[];
extern const char kFailedToSignResponse[];
extern const char kFailedToSerializeResponse[];
extern const char kEmptySerializedResponse[];
extern const char kFailedToSerializeSignals[];

extern const char kUnknown[];
extern const char kTimeout[];
extern const char kFailedToParseChallenge[];
extern const char kFailedToCreateResponse[];

}  // namespace errors

namespace timeouts {

extern const base::TimeDelta kHandshakeTimeout;
extern const base::TimeDelta kProcessWaitTimeout;
extern const base::TimeDelta kKeyUploadTimeout;

}  // namespace timeouts

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_DEVICE_TRUST_CONSTANTS_H_
