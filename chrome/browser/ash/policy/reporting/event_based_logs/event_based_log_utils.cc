// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_utils.h"

#include <string>

#include "base/uuid.h"

namespace policy {

std::string GenerateEventBasedLogUploadId() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

}  // namespace policy
