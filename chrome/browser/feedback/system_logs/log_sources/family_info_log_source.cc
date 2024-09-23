// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/family_info_log_source.h"

#include "base/metrics/histogram_functions.h"
#include "base/timer/elapsed_timer.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

FamilyInfoLogSource::FamilyInfoLogSource(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService& user_prefs)
    : SystemLogsSource("FamilyInfo"),
      identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      user_prefs_(user_prefs) {}

FamilyInfoLogSource::~FamilyInfoLogSource() = default;

void FamilyInfoLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(callback);

  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(callback).Run(std::make_unique<SystemLogsResponse>());
    return;
  }

  if (base::FeatureList::IsEnabled(
          supervised_user::kUseFamilyMemberRolePrefsForFeedback) &&
      !user_prefs_->GetString(prefs::kFamilyLinkUserMemberRole).empty()) {
    auto logs_response = std::make_unique<SystemLogsResponse>();
    const std::string& family_link_role =
        user_prefs_->GetString(prefs::kFamilyLinkUserMemberRole);
    if (family_link_role != supervised_user::kDefaultEmptyFamilyMemberRole) {
      logs_response->emplace(supervised_user::kFamilyMemberRoleFeedbackTag,
                             family_link_role);
    }
    RecordFetchUma(FamilyInfoLogSource::FetchStatus::kOk, base::Seconds(0),
                   /*immediately_available=*/true);
    std::move(callback).Run(std::move(logs_response));
    return;
  }

  auto callback_split = base::SplitOnceCallback(std::move(callback));

  fetch_timer_ = base::ElapsedTimer();
  fetcher_ = FetchListFamilyMembers(
      *identity_manager_, url_loader_factory_,
      base::BindOnce(&FamilyInfoLogSource::OnListMembersResponse,
                     base::Unretained(this), std::move(callback_split.first)));

  // Set the delay timeout to capture about 75% of users (approx. 3 seconds),
  // see Signin.ListFamilyMembersRequest.OverallLatency for Windows/Mac/Linux.
  list_members_response_timeout_.Start(
      FROM_HERE, base::Seconds(3),
      base::BindOnce(&FamilyInfoLogSource::OnListMembersResponseTimeout,
                     base::Unretained(this), std::move(callback_split.second)));
}

void FamilyInfoLogSource::OnListMembersResponse(
    SysLogsSourceCallback callback,
    const supervised_user::ProtoFetcherStatus& status,
    std::unique_ptr<kidsmanagement::ListMembersResponse> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(callback);
  list_members_response_timeout_.Stop();

  auto logs_response = std::make_unique<SystemLogsResponse>();
  if (status.IsOk()) {
    RecordFetchUma(FamilyInfoLogSource::FetchStatus::kOk,
                   fetch_timer_.Elapsed(), /*immediately_available=*/false);
    AppendFamilyMemberRoleForPrimaryAccount(*response, logs_response.get());
  } else {
    RecordFetchUma(FamilyInfoLogSource::FetchStatus::kFailureResponse,
                   fetch_timer_.Elapsed(), /*immediately_available=*/false);
  }

  std::move(callback).Run(std::move(logs_response));
}

void FamilyInfoLogSource::OnListMembersResponseTimeout(
    SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RecordFetchUma(FamilyInfoLogSource::FetchStatus::kTimeout,
                 fetch_timer_.Elapsed(), /*immediately_available=*/false);
  fetcher_.reset();

  std::move(callback).Run(std::make_unique<SystemLogsResponse>());
}

void FamilyInfoLogSource::AppendFamilyMemberRoleForPrimaryAccount(
    const kidsmanagement::ListMembersResponse& list_members_response,
    SystemLogsResponse* logs_response) {
  CoreAccountInfo account_info =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  // If the user has signed out since the fetch started do not record the family
  // member role.
  if (account_info.IsEmpty()) {
    return;
  }

  for (const kidsmanagement::FamilyMember& member :
       list_members_response.members()) {
    if (member.user_id() == account_info.gaia) {
      logs_response->emplace(
          supervised_user::kFamilyMemberRoleFeedbackTag,
          supervised_user::FamilyRoleToString(member.role()));
      return;
    }
  }
}

void FamilyInfoLogSource::RecordFetchUma(
    FamilyInfoLogSource::FetchStatus status,
    base::TimeDelta duration,
    bool immediately_available) {
  base::UmaHistogramEnumeration(kFamilyInfoLogSourceFetchStatusUma, status);
  base::UmaHistogramTimes(kFamilyInfoLogSourceFetchLatencyUma, duration);
  base::UmaHistogramBoolean(kFamilyInfoLogSourceImmediatelyAvailableUma,
                            immediately_available);
}

}  // namespace system_logs
