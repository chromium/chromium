// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_utils.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/notreached.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace ash::mahi_utils {

bool CalculateRetryLinkVisible(chromeos::MahiResponseStatus error) {
  switch (error) {
    case chromeos::MahiResponseStatus::kCantFindOutputData:
    case chromeos::MahiResponseStatus::kContentExtractionError:
    case chromeos::MahiResponseStatus::kInappropriate:
    case chromeos::MahiResponseStatus::kUnknownError:
      return true;
    case chromeos::MahiResponseStatus::kQuotaLimitHit:
    case chromeos::MahiResponseStatus::kResourceExhausted:
      return false;
    case chromeos::MahiResponseStatus::kLowQuota:
    case chromeos::MahiResponseStatus::kSuccess:
      NOTREACHED_NORETURN();
  }
}

int GetErrorStatusViewTextId(chromeos::MahiResponseStatus error) {
  switch (error) {
    case chromeos::MahiResponseStatus::kCantFindOutputData:
    case chromeos::MahiResponseStatus::kContentExtractionError:
    case chromeos::MahiResponseStatus::kInappropriate:
    case chromeos::MahiResponseStatus::kUnknownError:
      return IDS_ASH_MAHI_ERROR_STATUS_LABEL_GENERAL;
    case chromeos::MahiResponseStatus::kQuotaLimitHit:
      return IDS_ASH_MAHI_ERROR_STATUS_LABEL_QUOTA_LIMIT_HIT;
    case chromeos::MahiResponseStatus::kResourceExhausted:
      return IDS_ASH_MAHI_ERROR_STATUS_LABEL_RESOURCE_EXHAUSTED;
    case chromeos::MahiResponseStatus::kLowQuota:
    case chromeos::MahiResponseStatus::kSuccess:
      NOTREACHED_NORETURN();
  }
}

}  // namespace ash::mahi_utils
