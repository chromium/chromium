// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/metrics_utils.h"

#include "base/metrics/histogram_functions.h"

namespace enterprise_connectors {

void RecordError(ManagementServiceError error) {
  base::UmaHistogramEnumeration(
      "Enterprise.DeviceTrust.ManagementService.Error", error);
}

}  // namespace enterprise_connectors
