// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REALTIME_EVENT_UPLOAD_HELPER_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REALTIME_EVENT_UPLOAD_HELPER_DESKTOP_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"

class Profile;

namespace enterprise_reporting {

// Helper class for preparing real-time enterprise event uploads on Desktop.
// Manages finding the appropriate client and retrieving the DM token.
class RealtimeEventUploadHelper {
 public:
  RealtimeEventUploadHelper(std::string_view reporting_scope,
                            std::string_view event_name,
                            Profile* profile = nullptr);
  ~RealtimeEventUploadHelper();

  RealtimeEventUploadHelper(const RealtimeEventUploadHelper&) = delete;
  RealtimeEventUploadHelper& operator=(const RealtimeEventUploadHelper&) =
      delete;

  struct ReportingContext {
    raw_ref<enterprise_connectors::RealtimeReportingClientBase> client;
    std::string dm_token;
    bool per_profile;
  };

  // Prepares for an upload by finding the appropriate client and token.
  // Returns nullopt and logs errors if the pipeline is not ready.
  std::optional<ReportingContext> PrepareUpload(bool per_profile);

  bool IsRealTimeReportingClientAvailable() const;

  // Returns true if reporting is enabled via feature flag and a valid DM
  // token is available for the given scope.
  bool IsEligibleForUpload(bool per_profile) const;

  std::string_view reporting_scope() const { return reporting_scope_; }
  std::string_view event_name() const { return event_name_; }

 private:
  std::optional<std::string> GetDMToken(bool per_profile) const;
  enterprise_connectors::RealtimeReportingClientBase*
  GetRealTimeReportingClient() const;

  // The scope of the report (e.g., "browser" or "profile"). Used for logging.
  const std::string reporting_scope_;
  // The name of the event being reported (e.g., "SaaS usage" or "browser
  // launch"). Used for logging.
  const std::string event_name_;
  const raw_ptr<Profile> profile_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REALTIME_EVENT_UPLOAD_HELPER_DESKTOP_H_
