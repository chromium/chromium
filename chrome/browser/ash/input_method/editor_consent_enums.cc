// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_consent_enums.h"

#include "base/logging.h"
#include "base/types/cxx23_to_underlying.h"

namespace ash::input_method {

ConsentStatus GetConsentStatusFromInteger(int status_value) {
  switch (status_value) {
    case base::to_underlying(ConsentStatus::kUnset):
      return ConsentStatus::kUnset;
    case base::to_underlying(ConsentStatus::kApproved):
      return ConsentStatus::kApproved;
    case base::to_underlying(ConsentStatus::kDeclined):
      return ConsentStatus::kDeclined;
    case base::to_underlying(ConsentStatus::kPending):
      return ConsentStatus::kPending;
    default:
      LOG(ERROR) << "Invalid consent status: " << status_value;
      // For any of the invalid states, treat the consent status as unset.
      return ConsentStatus::kUnset;
  }
}

}  // namespace ash::input_method
