// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/metrics/credential_storage_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace ash::nearby::presence::metrics {

void RecordCredentialStorageInitializationResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage.Initialization.Result", success);
}

}  // namespace ash::nearby::presence::metrics
