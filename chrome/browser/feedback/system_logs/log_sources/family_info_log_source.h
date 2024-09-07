// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_FAMILY_INFO_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_FAMILY_INFO_LOG_SOURCE_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher_status.h"

class PrefService;

namespace base {
class ElapsedTimer;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace system_logs {

inline constexpr char kFamilyInfoLogSourceFetchStatusUma[] =
    "FamilyLinkUser.FamilyInfoLogSource.FetchStatus";
inline constexpr char kFamilyInfoLogSourceFetchLatencyUma[] =
    "FamilyLinkUser.FamilyInfoLogSource.FetchLatency";
inline constexpr char kFamilyInfoLogSourceImmediatelyAvailableUma[] =
    "FamilyLinkUser.FamilyInfoLogSource.ImmediatelyAvailable";

// Fetches settings related to Family Link if the user is in a Family Group.
class FamilyInfoLogSource : public system_logs::SystemLogsSource {
 public:
  // Values for the kFamilyInfoLogSourceFetchStatusUma metric.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(FetchStatus)
  enum class FetchStatus {
    kOk = 0,
    kFailureResponse = 1,
    kTimeout = 2,
    kMaxValue = kTimeout,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:FamilyInfoLogSourceFetchStatus)

  FamilyInfoLogSource(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService& user_prefs);

  FamilyInfoLogSource(const FamilyInfoLogSource&) = delete;
  FamilyInfoLogSource& operator=(const FamilyInfoLogSource&) = delete;

  ~FamilyInfoLogSource() override;

  // SystemLogsSource:
  void Fetch(system_logs::SysLogsSourceCallback callback) override;

 private:
  // Runs `callback` with the FamilyLink Settings information if it was
  // retrieved successfully from the fetch before the timeout.
  void OnListMembersResponse(
      SysLogsSourceCallback callback,
      const supervised_user::ProtoFetcherStatus& status,
      std::unique_ptr<kidsmanagement::ListMembersResponse> response);

  // Runs `callback` following the timeout of FamilyLink Settings information
  // fetch.
  void OnListMembersResponseTimeout(
      system_logs::SysLogsSourceCallback callback);

  // Retrieves family member information from the `list_members_response` and
  // populates the `logs_response` if this is valid.
  void AppendFamilyMemberRoleForPrimaryAccount(
      const kidsmanagement::ListMembersResponse& list_members_response,
      SystemLogsResponse* logs_response);

  // Logs metrics for a fetch attempt.
  void RecordFetchUma(FamilyInfoLogSource::FetchStatus status,
                      base::TimeDelta duration,
                      bool immediately_available);

  std::unique_ptr<supervised_user::ListFamilyMembersFetcher> fetcher_;
  base::ElapsedTimer fetch_timer_;
  base::OneShotTimer list_members_response_timeout_;

  // Profile-keyed service that should outlive feedback fetching.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ref<PrefService> user_prefs_;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_FAMILY_INFO_LOG_SOURCE_H_
