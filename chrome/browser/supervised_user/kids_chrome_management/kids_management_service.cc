// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_management_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"
#include "chrome/browser/supervised_user/kids_chrome_management/families_common.pb.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_external_fetcher.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_profile_manager.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "content/public/browser/browser_context.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
using ::base::BindOnce;
using ::base::Days;
using ::base::Hours;
using ::base::NoDestructor;
using ::base::OnceCallback;
using ::base::OnceClosure;
using ::base::Seconds;
using ::base::Singleton;
using ::base::StringPiece;
using ::base::Unretained;
using ::base::ranges::find;
using ::content::BrowserContext;
using ::kids_chrome_management::FamilyMember;
using ::kids_chrome_management::FamilyRole;
using ::kids_chrome_management::ListFamilyMembersResponse;
using ::net::BackoffEntry;
using ::network::SharedURLLoaderFactory;
using ::signin::ConsentLevel;
using ::signin::IdentityManager;
using ::signin::PrimaryAccountChangeEvent;
using ::signin::Tribool;

const BackoffEntry::Policy& GetFamilyFetchBackoffPolicy() {
  static const BackoffEntry::Policy nonce{
      // Number of initial errors (in sequence) to ignore before applying
      // exponential back-off rules.
      0,

      // Initial delay for exponential backoff.
      static_cast<int>(Seconds(2).InMilliseconds()),

      // Factor by which the waiting time will be multiplied.
      2,

      // Fuzzing percentage. ex: 10% will spread requests randomly
      // between 90%-100% of the calculated time.
      0.2,  // 20%

      // Maximum amount of time we are willing to delay our request.
      static_cast<int>(Hours(4).InMilliseconds()),

      // Time to keep an entry from being discarded even when it
      // has no significant state, -1 to never discard.
      -1,

      // Don't use initial delay unless the last request was an error.
      false,
  };
  return nonce;
}

constexpr bool HasSupervisionSupport() {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  return true;
#else
  return false;
#endif
}

AccountInfo GetPrimaryAccount(const IdentityManager& identity_manager) {
  return identity_manager.FindExtendedAccountInfo(
      identity_manager.GetPrimaryAccountInfo(ConsentLevel::kSignin));
}

std::vector<const FamilyMember>::iterator FindFamilyMemberWithRole(
    const std::vector<FamilyMember>& members,
    const FamilyRole role) {
  return find(members, role,
              [](const FamilyMember& member) { return member.role(); });
}
}  // namespace

KidsManagementService::KidsManagementService(
    Profile* profile,
    IdentityManager& identity_manager,
    SupervisedUserService& supervised_user_service,
    PrefService& pref_service,
    scoped_refptr<SharedURLLoaderFactory> url_loader_factory)
    : profile_(profile),
      identity_manager_(identity_manager),
      supervised_user_service_(supervised_user_service),
      profile_manager_(pref_service, *profile),
      url_loader_factory_(url_loader_factory),
      list_family_members_backoff_(&GetFamilyFetchBackoffPolicy()) {}
KidsManagementService::~KidsManagementService() = default;

void KidsManagementService::Init() {
  SupervisedUserServiceFactory::GetForProfile(profile_)->SetDelegate(this);
  identity_manager_->AddObserver(this);

  // If we're already signed in, check the account immediately just to be sure.
  // (We might have missed an update before registering as an observer.)
  // "Unconsented" because this class doesn't care about browser sync consent.
  AccountInfo primary_account_info = GetPrimaryAccount(*identity_manager_);
  if (!primary_account_info.IsEmpty()) {
    OnExtendedAccountInfoUpdated(primary_account_info);
  }
}

void KidsManagementService::Shutdown() {
  list_family_members_timer_.Stop();
  list_family_members_fetcher_.reset();

  identity_manager_->RemoveObserver(this);
  SupervisedUserServiceFactory::GetForProfile(profile_)->SetDelegate(nullptr);
}

bool KidsManagementService::IsChildAccountStatusKnown() const {
  return profile_manager_.IsChildAccountStatusKnown();
}

bool KidsManagementService::IsFetchFamilyMembersStarted() const {
  return !!(list_family_members_fetcher_);
}

CoreAccountId KidsManagementService::GetAuthAccountId() const {
  return identity_manager_->GetPrimaryAccountId(ConsentLevel::kSignin);
}

void KidsManagementService::AddChildStatusReceivedCallback(
    OnceClosure callback) {
  if (IsChildAccountStatusKnown()) {
    std::move(callback).Run();
    return;
  }
  status_received_listeners_.push_back(std::move(callback));
}

#if !BUILDFLAG(IS_CHROMEOS)
void KidsManagementService::UpdateUserSignOutSetting() {
  signin_util::UserSignoutSetting::GetForProfile(profile_)
      ->SetClearPrimaryAccountAllowed(false);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

void KidsManagementService::SetActive(bool newValue) {
  if (!profile_manager_.IsChildAccount()) {
    return;
  }
  if (newValue == IsFetchFamilyMembersStarted()) {
    return;
  }

  if (newValue) {
#if !BUILDFLAG(IS_CHROMEOS)
    UpdateUserSignOutSetting();
#endif  // !BUILDFLAG(IS_CHROMEOS)

    StartFetchFamilyMembers();
    DCHECK(IsFetchFamilyMembersStarted())
        << "StartFetchFamilyMembers should make the status started";

    // Registers a request for permission for the user to access a blocked site.
    supervised_user_service_->web_approvals_manager()
        .AddRemoteApprovalRequestCreator(
            PermissionRequestCreatorApiary::CreateWithProfile(profile_));
  } else {
#if !BUILDFLAG(IS_CHROMEOS)
    signin_util::UserSignoutSetting::GetForProfile(profile_)
        ->ResetSignoutSetting();
#endif  // !BUILDFLAG(IS_CHROMEOS)
    StopFetchFamilyMembers();
    DCHECK(!IsFetchFamilyMembersStarted())
        << "StopFetchFamilyMembers should make the status stopped";
  }
}

void KidsManagementService::OnPrimaryAccountChanged(
    const PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
        event_details.GetCurrentState().primary_account);
    if (!account_info.IsEmpty()) {
      OnExtendedAccountInfoUpdated(account_info);
    }
    // Otherwise OnExtendedAccountInfoUpdated will be notified once
    // the account info is available.
  }
}

void KidsManagementService::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (GetAuthAccountId() == account_info.account_id) {
    return;
  }

  if (!HasSupervisionSupport()) {
    SetIsChildAccount(false);
    return;
  }

  SetIsChildAccount(account_info.is_child_account == Tribool::kTrue);
}

void KidsManagementService::OnExtendedAccountInfoRemoved(
    const AccountInfo& account_info) {
  if (GetAuthAccountId() == account_info.account_id) {
    return;
  }
  SetIsChildAccount(false);
}

void KidsManagementService::SetIsChildAccount(bool is_child_account) {
  profile_manager_.UpdateChildAccountStatus(is_child_account);

  for (OnceClosure& callback : status_received_listeners_) {
    std::move(callback).Run();
  }
  status_received_listeners_.clear();
}

void KidsManagementService::StartFetchFamilyMembers() {
  list_family_members_fetcher_ = FetchListFamilyMembers(
      *identity_manager_, url_loader_factory_, GetEndpointUrl(),
      BindOnce(&KidsManagementService::ConsumeListFamilyMembers,
               Unretained(this)));
}

void KidsManagementService::StopFetchFamilyMembers() {
  list_family_members_fetcher_.reset();
  list_family_members_timer_.Stop();
}

void KidsManagementService::ConsumeListFamilyMembers(
    KidsExternalFetcherStatus status,
    std::unique_ptr<ListFamilyMembersResponse> response) {
  if (status.IsTransientError()) {
    list_family_members_backoff_.InformOfRequest(false);
    list_family_members_timer_.Start(
        FROM_HERE, list_family_members_backoff_.GetTimeUntilRelease(), this,
        &KidsManagementService::StartFetchFamilyMembers);
    return;
  }
  if (status.IsPersistentError()) {
    return;
  }

  DCHECK(status.IsOk());

  family_members_ = std::vector<kids_chrome_management::FamilyMember>(
      response->members().begin(), response->members().end());

  auto head_of_household = FindFamilyMemberWithRole(
      family_members_, kids_chrome_management::HEAD_OF_HOUSEHOLD);
  auto parent =
      FindFamilyMemberWithRole(family_members_, kids_chrome_management::PARENT);

  if (head_of_household != family_members_.end()) {
    profile_manager_.SetFirstCustodian(*head_of_household);
  }
  if (parent != family_members_.end()) {
    profile_manager_.SetSecondCustodian(*parent);
  }
  list_family_members_backoff_.InformOfRequest(true);
  list_family_members_timer_.Start(
      FROM_HERE, Days(1), this,
      &KidsManagementService::StartFetchFamilyMembers);
}

bool KidsManagementService::IsPendingNextFetchFamilyMembers() const {
  return list_family_members_timer_.IsRunning();
}

const std::string& KidsManagementService::GetEndpointUrl() {
  static const NoDestructor<std::string> nonce(
      "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/");
  return *nonce;
}

KidsManagementServiceFactory::KidsManagementServiceFactory()
    : ProfileKeyedServiceFactory("KidsManagementService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SupervisedUserServiceFactory::GetInstance());
}
KidsManagementServiceFactory::~KidsManagementServiceFactory() = default;

KidsManagementService* KidsManagementServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<KidsManagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
KidsManagementServiceFactory* KidsManagementServiceFactory::GetInstance() {
  return Singleton<KidsManagementServiceFactory>::get();
}

// Builds the service instance and its local dependencies.
// The profile dependency is needed to verify the dynamic child account status.
KeyedService* KidsManagementServiceFactory::BuildServiceInstanceFor(
    BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return new KidsManagementService(
      profile, *IdentityManagerFactory::GetForProfile(profile),
      *SupervisedUserServiceFactory::GetForProfile(profile),
      *profile->GetPrefs(), profile->GetURLLoaderFactory());
}
