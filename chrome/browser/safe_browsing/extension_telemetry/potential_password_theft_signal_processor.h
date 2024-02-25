// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_POTENTIAL_PASSWORD_THEFT_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_POTENTIAL_PASSWORD_THEFT_SIGNAL_PROCESSOR_H_

#include <algorithm>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/timer/timer.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

using LoginReputationClientReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;
using password_manager::metrics_util::PasswordType;
using ExtensionTelemetryReusedPasswordAccountType =
    ExtensionTelemetryReportRequest::SignalInfo::PotentialPasswordTheftInfo::
        PasswordReuseInfo::ReusedPasswordAccountType;

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;
struct PasswordReuseInfo;

/**
 * This signal processor maintains two data stores, password reuse event info
 * and remote hosts contacted (within 1 sec of a password reuse).
 * - Password reuse information contains information about a list of password
 * reuse events.
 * - Remote hosts contacted store lists hosts that were contacted by the
 * extension within 1 second of any password reuse event (present in the
 * password reuse store).
 */
class PotentialPasswordTheftSignalProcessor : public ExtensionSignalProcessor {
 public:
  PotentialPasswordTheftSignalProcessor();
  ~PotentialPasswordTheftSignalProcessor() override;

  PotentialPasswordTheftSignalProcessor(
      PotentialPasswordTheftSignalProcessor&) = delete;
  PotentialPasswordTheftSignalProcessor& operator=(
      const PotentialPasswordTheftSignalProcessor&) = delete;

  // ExtensionSignalProcessor:
  void ProcessSignal(const ExtensionSignal& signal) override;
  std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) override;
  bool IsPasswordQueueEmptyForTest();
  bool IsRemoteHostURLQueueEmptyForTest();
  bool HasDataToReportForTest() const override;

  // Convert the ReusedPasswordAccountType of LoginReputationClient type passed
  // in from password detection manager to extension telemetry service proto
  // type.
  ExtensionTelemetryReusedPasswordAccountType
  ConvertToExtensionTelemetryReusedPasswordAccountType(
      LoginReputationClientReusedPasswordAccountType
          login_rep_reused_pw_account_type);

  // Helper function to update password and remote hosts contacted data stores
  // after a new signal is received. Updating the data stores involves
  // maintaining a list of hosts contacted by the extension within 1 second of
  // received password reuse events. See the implementation for more details.
  void UpdateDataStores(extensions::ExtensionId extension_id,
                        base::Time current_time);

  // Helper function to update the information associated with a reused
  // password.
  void UpdatePasswordReuseInfo(PasswordReuseInfo& reuse_info,
                               PasswordReuseInfo& incoming_reuse_info);

 protected:
  // Stores remote host urls (only host portion) with timestamps (FIFO).
  using RemoteHostURLsWithTimestamps =
      std::deque<std::pair<std::string, base::Time>>;
  // Maps extension id to RemoteHostURLsWithTimestamps.
  using RemoteHostURLsWithTimestampsPerExtension =
      base::flat_map<extensions::ExtensionId, RemoteHostURLsWithTimestamps>;
  RemoteHostURLsWithTimestampsPerExtension remote_host_url_queue_;

  // Stores password with timestamps (FIFO).
  using PasswordsWithTimestamps =
      base::queue<std::pair<PasswordReuseInfo, base::Time>>;
  // Maps extension id to PasswordsWithTimestamps.
  using PasswordsWithTimestampsPerExtension =
      base::flat_map<extensions::ExtensionId, PasswordsWithTimestamps>;
  PasswordsWithTimestampsPerExtension password_queue_;

  // Maps remote host urls to the number of times the remote host was contacted
  // for reporting.
  using RemoteHostURLsWithCounts = base::flat_map<std::string, uint32_t>;
  using RemoteHostURLsWithCountsPerExtension =
      base::flat_map<extensions::ExtensionId, RemoteHostURLsWithCounts>;
  RemoteHostURLsWithCountsPerExtension remote_host_url_store_;

  // Maps passwords to the number of times the password was reused for
  // reporting.
  using PasswordsWithCounts = base::flat_map<uint64_t, PasswordReuseInfo>;
  using PasswordsWithCountsPerExtension =
      base::flat_map<extensions::ExtensionId, PasswordsWithCounts>;
  PasswordsWithCountsPerExtension password_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_POTENTIAL_PASSWORD_THEFT_SIGNAL_PROCESSOR_H_
