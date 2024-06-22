// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_AUTH_ARC_AUTH_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_AUTH_ARC_AUTH_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/auth.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace arc {

class ArcAuthCodeFetcher;
class ArcBackgroundAuthCodeFetcher;
class ArcBridgeService;
class ArcFetcherBase;

constexpr char kArcAuthRequestAccountInfoResultPrimaryHistogramName[] =
    "Arc.Auth.RequestAccountInfoResult.Primary";
constexpr char kArcAuthRequestAccountInfoResultSecondaryHistogramName[] =
    "Arc.Auth.RequestAccountInfoResult.Secondary";

// Implementation of ARC authorization.
class ArcAuthService : public KeyedService,
                       public mojom::AuthHost,
                       public ConnectionObserver<mojom::AuthInstance>,
                       public signin::IdentityManager::Observer,
                       public ash::AccountAppsAvailability::Observer,
                       public ArcSessionManagerObserver {
 public:
  using GetGoogleAccountsInArcCallback =
      base::OnceCallback<void(std::vector<mojom::ArcAccountInfoPtr>)>;

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAuthService* GetForBrowserContext(content::BrowserContext* context);

  ArcAuthService(content::BrowserContext* profile,
                 ArcBridgeService* bridge_service);

  ArcAuthService(const ArcAuthService&) = delete;
  ArcAuthService& operator=(const ArcAuthService&) = delete;

  ~ArcAuthService() override;

  // Gets the list of Google accounts currently stored in ARC. This is used by
  // the one-time migration flow for migrating Google accounts in ARC to Chrome
  // OS Account Manager.
  void GetGoogleAccountsInArc(GetGoogleAccountsInArcCallback callback);

  void RequestPrimaryAccount(RequestPrimaryAccountCallback callback) override;

  // For supporting ArcServiceManager::GetService<T>().
  static const char kArcServiceName[];

  // ConnectionObserver<mojom::AuthInstance>:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // mojom::AuthHost:
  void OnAuthorizationResult(mojom::ArcSignInResultPtr result,
                             mojom::ArcSignInAccountPtr account) override;
  void ReportMetrics(mojom::MetricsType metrics_type, int32_t value) override;
  void ReportAccountCheckStatus(mojom::AccountCheckStatus status) override;
  void ReportAccountReauthReason(mojom::ReauthReason reason) override;
  void ReportManagementChangeStatus(
      mojom::ManagementChangeStatus status) override;
  void RequestPrimaryAccountInfo(
      RequestPrimaryAccountInfoCallback callback) override;
  void RequestAccountInfo(const std::string& account_name,
                          RequestAccountInfoCallback callback) override;
  void IsAccountManagerAvailable(
      IsAccountManagerAvailableCallback callback) override;
  void HandleAddAccountRequest() override;
  void HandleRemoveAccountRequest(const std::string& email) override;
  void HandleUpdateCredentialsRequest(const std::string& email) override;

  static void EnsureFactoryBuilt();

 private:
  friend class ArcAuthServiceTest;

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& account_info) override;

  // ash::AccountAppsAvailability::Observer:
  void OnAccountAvailableInArc(
      const account_manager::Account& account) override;
  void OnAccountUnavailableInArc(
      const account_manager::Account& account) override;

  // ArcSessionManagerObserver:
  void OnArcInitialStart() override;

  // KeyedService:
  void Shutdown() override;

  // Calls `mojom::OnAccountUpdated` with update type
  // `mojom::AccountUpdateType::UPSERT` if the account with provided
  // `account_info` doesn't have refresh token in persistent error state.
  void UpsertAccountToArc(const CoreAccountInfo& account_info);

  // Calls `mojom::OnAccountUpdated` with update type
  // `mojom::AccountUpdateType::REMOVAL` for the provided email.
  void RemoveAccountFromArc(const std::string& email);

  // Issues a request for fetching AccountInfo for the Device Account.
  // |initial_signin| denotes whether this is the initial ARC provisioning flow
  // or a subsequent sign-in.
  // |callback| is completed with |ArcAuthCodeStatus| and |AccountInfo|
  // depending on the success / failure of the operation.
  void FetchPrimaryAccountInfo(bool initial_signin,
                               RequestPrimaryAccountInfoCallback callback);

  // Callback for |FetchPrimaryAccountInfo|.
  // |fetcher| is a pointer to the object that issues this callback. Used for
  // deleting pending requests from |pending_token_requests_|.
  // |success| and |auth_code| are the callback parameters passed by
  // |ArcBackgroundAuthCodeFetcher::Fetch|.
  // |callback| is completed with |ArcAuthCodeStatus| and |AccountInfo|
  // depending on the success / failure of the operation.
  void OnPrimaryAccountAuthCodeFetched(
      ArcAuthCodeFetcher* fetcher,
      RequestPrimaryAccountInfoCallback callback,
      bool success,
      const std::string& auth_code);

  // Issues a request for fetching AccountInfo for a Secondary Account
  // represented by |account_name|. |account_name| is the account identifier
  // used by ARC/Android.
  void FetchSecondaryAccountInfo(const std::string& account_name,
                                 RequestAccountInfoCallback callback);

  // Callback for |FetchSecondaryAccountInfo|, issued by
  // |ArcBackgroundAuthCodeFetcher::Fetch|.
  // |account_name| is the account identifier used by ARC/Android.
  // |fetcher| is used to identify the |ArcBackgroundAuthCodeFetcher| instance
  // that completed the request. |callback| is completed with
  // |ArcAuthCodeStatus| and |AccountInfo| depending on the success / failure of
  // the operation. |success| and |auth_code| are arguments passed by
  // |ArcBackgroundAuthCodeFetcher::Fetch| callback.
  void OnSecondaryAccountAuthCodeFetched(const std::string& account_name,
                                         ArcBackgroundAuthCodeFetcher* fetcher,
                                         RequestAccountInfoCallback callback,
                                         bool success,
                                         const std::string& auth_code);

  // Callback for data removal confirmation.
  void OnDataRemovalAccepted(bool accepted);

  // Creates an |ArcBackgroundAuthCodeFetcher| for |account_id|. Can be used for
  // Device Account and Secondary Accounts. |initial_signin| denotes whether the
  // fetcher is being created for the initial ARC provisioning flow or for a
  // subsequent sign-in.
  std::unique_ptr<ArcBackgroundAuthCodeFetcher>
  CreateArcBackgroundAuthCodeFetcher(const CoreAccountId& account_id,
                                     bool initial_signin);

  // Deletes a completed enrollment token / auth code fetch request from
  // |pending_token_requests_|.
  void DeletePendingTokenRequest(ArcFetcherBase* fetcher);

  // Triggers an async push of the accounts in IdentityManager to ARC.
  // If |filter_primary_account| is set to |true|, the Primary Account in Chrome
  // OS Account Manager will not be pushed to ARC as part of this call.
  void TriggerAccountsPushToArc(bool filter_primary_account);

  // Pushes accounts in the `accounts` set to ARC. `accounts` set must contain
  // only Gaia accounts. If `filter_primary_account` is set to `true`, the
  // Primary Account in Chrome OS Account Manager will not be pushed to ARC as
  // part of this call.
  void CompleteAccountsPushToArc(
      bool filter_primary_account,
      const base::flat_set<account_manager::Account>& accounts);

  // Issues a request to ARC, which will complete callback with the list of
  // Google accounts in ARC.
  void DispatchAccountsInArc(GetGoogleAccountsInArcCallback callback);

  // Response for |mojom::GetMainAccountResolutionStatus|.
  void OnMainAccountResolutionStatus(mojom::MainAccountResolutionStatus status);

  // Whether we selectively push accounts to ARC based on policy or user
  // request.
  static bool AreAccountsRestricted();

  // Non-owning pointers.
  const raw_ptr<Profile> profile_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<ArcBridgeService> arc_bridge_service_;
  raw_ptr<ash::AccountAppsAvailability> account_apps_availability_ = nullptr;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  bool url_loader_factory_for_testing_set_ = false;

  // A list of pending enrollment token / auth code requests.
  std::vector<std::unique_ptr<ArcFetcherBase>> pending_token_requests_;

  // Pending callback for |GetGoogleAccountsInArc| if ARC bridge is not yet
  // ready.
  GetGoogleAccountsInArcCallback pending_get_arc_accounts_callback_;

  base::WeakPtrFactory<ArcAuthService> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_AUTH_ARC_AUTH_SERVICE_H_
