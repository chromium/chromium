// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_REPORTING_UTIL_H_
#define CHROME_BROWSER_PROFILES_REPORTING_UTIL_H_

#include "base/values.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace reporting {

// Fetches additional information that is common to every event. Fetches and
// returns corresponding info to a Device, Browser and Profile protos defined in
// google3/google/internal/chrome/reporting/v1/chromereporting.proto.
base::Value::Dict GetContext(Profile* profile);

// Fetches the same information as GetContext, but in a protobuf instead of a
// Value.
enterprise_connectors::ClientMetadata GetContextAsClientMetadata(
    Profile* profile);

// Returns User DMToken or client id for a given `profile` if:
// * `profile` is NOT incognito profile.
// * `profile` is NOT sign-in screen profile
// * user corresponding to a `profile` is managed.
// Otherwise returns empty optional. More about DMToken:
// go/dmserver-domain-model#dmtoken.
absl::optional<std::string> GetUserDmToken(Profile* profile);
absl::optional<std::string> GetUserClientId(Profile* profile);

}  // namespace reporting

#endif  // CHROME_BROWSER_PROFILES_REPORTING_UTIL_H_
