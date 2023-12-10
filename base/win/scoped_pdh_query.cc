// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_pdh_query.h"

namespace base::win {

// static
ScopedPdhQuery ScopedPdhQuery::Create() {
  PDH_HQUERY pdh_query;
  PDH_STATUS pdh_status = ::PdhOpenQuery(NULL, NULL, &pdh_query);
  if (pdh_status == ERROR_SUCCESS) {
    return ScopedPdhQuery(pdh_query);
  }
  return ScopedPdhQuery();
}

}  // namespace base::win
