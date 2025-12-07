// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_STORE_INTERFACE_H_
#define CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_STORE_INTERFACE_H_

#include "components/leveldb_proto/public/proto_database.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

class GURL;

namespace safe_browsing {

using CSBRR = ClientSafeBrowsingReportRequest;

class NotificationTelemetryStoreInterface {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using LoadEntriesCallback = base::OnceCallback<
      void(bool, std::unique_ptr<std::vector<CSBRR::ServiceWorkerBehavior>>)>;

  NotificationTelemetryStoreInterface() = default;
  virtual ~NotificationTelemetryStoreInterface() = default;

  NotificationTelemetryStoreInterface(
      const NotificationTelemetryStoreInterface&) = delete;
  NotificationTelemetryStoreInterface& operator=(
      const NotificationTelemetryStoreInterface&) = delete;

  virtual void AddServiceWorkerRegistrationBehavior(
      const GURL& scope_url,
      const std::vector<GURL>& import_script_urls,
      SuccessCallback success_callback) = 0;

  virtual void AddServiceWorkerPushBehavior(
      const GURL& script_url,
      const std::vector<GURL>& requested_urls,
      SuccessCallback success_callback) = 0;

  virtual void GetServiceWorkerBehaviors(
      LoadEntriesCallback load_entries_callback,
      SuccessCallback success_callback) = 0;
  virtual void DeleteAll(SuccessCallback success_callback) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_STORE_INTERFACE_H_
