// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_TELEMETRY_SERVICE_H_

#include "base/memory/weak_ptr.h"

namespace safe_browsing {

// This class is used to send telemetry related to security incidents to Safe
// Browsing. It is currently only implemented for downoads of APK files on
// Android. See |AndroidTelemetryService|.
class TelemetryService {
 public:
  TelemetryService();
  virtual ~TelemetryService();

  base::WeakPtr<TelemetryService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TelemetryService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(TelemetryService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TELEMETRY_TELEMETRY_SERVICE_H_
