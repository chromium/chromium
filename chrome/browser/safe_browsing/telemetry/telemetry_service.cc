// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/telemetry/telemetry_service.h"

namespace safe_browsing {

TelemetryService::TelemetryService() {}

TelemetryService::~TelemetryService() {
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace safe_browsing
