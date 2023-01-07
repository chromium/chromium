// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_DEVICE_TRUST_CONSTANTS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_DEVICE_TRUST_CONSTANTS_H_

namespace enterprise_connectors::errors {

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

}  // namespace enterprise_connectors::errors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_COMMON_DEVICE_TRUST_CONSTANTS_H_
