// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"

namespace enterprise_connectors {

namespace errors {

const char kMissingCoreSignals[] = "missing_core_signals";
const char kMissingSigningKey[] = "missing_signing_key";
const char kBadChallengeFormat[] = "bad_challenge_format";
const char kBadChallengeSource[] = "bad_challenge_source";
const char kFailedToSerializeKeyInfo[] = "failed_to_serialize_keyinfo";
const char kFailedToGenerateResponse[] = "failed_to_generate_response";
const char kFailedToSignResponse[] = "failed_to_sign_response";
const char kFailedToSerializeResponse[] = "failed_to_serialize_response";
const char kEmptySerializedResponse[] = "empty_response";
const char kFailedToSerializeSignals[] = "failed_to_serialize_signals";

const char kUnknown[] = "unknown";
const char kTimeout[] = "timeout";
const char kFailedToParseChallenge[] = "failed_to_parse_challenge";
const char kFailedToCreateResponse[] = "failed_to_create_response";

}  // namespace errors

namespace timeouts {

// Overall timeout for the Device Trust handshake flow.
const base::TimeDelta kHandshakeTimeout = base::Minutes(1);

// Timeout for when the browser is waiting for another process to do some key
// management operation.
const base::TimeDelta kProcessWaitTimeout = base::Seconds(45);

// Timeout for when trying to upload the public key.
const base::TimeDelta kKeyUploadTimeout = base::Seconds(30);

}  // namespace timeouts

}  // namespace enterprise_connectors
