// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/test/test_constants.h"

namespace enterprise_connectors::test {

const char kAllowedHost[] = "allowed.google.com";
const char kOtherHost[] = "notallowed.google.com";

const char kFailedToParseChallengeJsonResponse[] =
    "{\"error\":\"failed_to_parse_challenge\"}";

}  // namespace enterprise_connectors::test
