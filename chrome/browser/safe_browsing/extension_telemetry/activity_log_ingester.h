// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_ACTIVITY_LOG_INGESTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_ACTIVITY_LOG_INGESTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"

class Profile;

namespace safe_browsing {

class ExtensionTelemetryService;

// An ingester for ActivityLog that filters and transforms extension activities
// into telemetry signals for the ExtensionTelemetryService.
class ActivityLogIngester {
 public:
  ActivityLogIngester(Profile* profile,
                      ExtensionTelemetryService* telemetry_service);
  ~ActivityLogIngester();

  ActivityLogIngester(const ActivityLogIngester&) = delete;
  ActivityLogIngester& operator=(const ActivityLogIngester&) = delete;

  // Receives activities via the ActivityLog::TelemetryCallback.
  void OnExtensionActivity(scoped_refptr<extensions::Action> action);

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<ExtensionTelemetryService> telemetry_service_;

  base::WeakPtrFactory<ActivityLogIngester> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_ACTIVITY_LOG_INGESTER_H_
