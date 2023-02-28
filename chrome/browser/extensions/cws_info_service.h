// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CWS_INFO_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_CWS_INFO_SERVICE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
class Extension;

// This is an interface class to allow for easy mocking.
class CWSInfoServiceInterface {
 public:
  virtual ~CWSInfoServiceInterface() = default;

  // Synchronously checks if the extension is currently live in CWS.
  // If the information is not available immediately (i.e., not stored in local
  // cache), does not return a value.
  virtual absl::optional<bool> IsLiveInCWS(const Extension& extension) = 0;
};

// TODO(anunoy) : The keyed service implementation.
// This service retrieves information about installed extensions from CWS
// periodically (default: every 24 hours). It also supports out-of-cycle
// queries to CWS for use cases that require the latest CWS information sooner.
// An example use case is when the ExtensionUnpublishedAvailability policy
// setting changes.
// Note: Extensions that do not update from CWS are never queried.

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CWS_INFO_SERVICE_H_
