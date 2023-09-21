// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANTA_MANTA_STATUS_H_
#define CHROME_BROWSER_MANTA_MANTA_STATUS_H_

#include <string>

namespace manta {

// Please do not renumber these.
enum class MantaStatusCode {
  kOk = 0,
  kGenericError = 1,
  // The input parameters to the manta service API don't meet the requirements,
  // e.g. missing some essential fields.
  kInvalidInput = 2,
  kResourceExhausted = 3,
  kBackendFailure = 4,
  kMalformedResponse = 5,
  kNoInternetConnection = 6,
  kUnsupportedLanguage = 7,
  kBlockedOutputs = 8,
  kRestrictedCountry = 9,
  kMax = kRestrictedCountry,
};

struct MantaStatus {
  MantaStatusCode status_code;
  // An optional field for more details. Usually a specific `status_code` makes
  // it unnecessary.
  std::string message;
};

}  // namespace manta

#endif  // CHROME_BROWSER_MANTA_MANTA_STATUS_H_
