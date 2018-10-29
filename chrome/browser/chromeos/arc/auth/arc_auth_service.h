// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/account_mapper_util.h"
#include "chrome/browser/chromeos/arc/auth/arc_active_directory_enrollment_token_fetcher.h"
#include "chromeos/account_manager/account_manager.h"
#include "components/arc/common/auth.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class AccountTrackerService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace arc {

class ArcAuthCodeFetcher;
class ArcBackgroundAuthCodeFetcher;
class ArcBridgeService;
class ArcFetcherBase;

// Implementation of ARC authorization.
class ArcAuthService : public KeyedService,
                       public mojom::AuthHost,
                       public ConnectionObserver<mojom::AuthInstance>,
                       public chromeos::AccountManager::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAuthService* GetForBrowserContext(content::BrowserContext* context);

  ArcAuthService(content::BrowserContext* profile,
                 ArcBridgeService* bridge_service);
  ~ArcAuthService() override;

  // For supporting ArcServiceManager::GetService<T>().
  static const char kArcServiceName[];

  // ConnectionObserver<mojom::AuthInstance>:
  void OnConnectionClosed() override;

  // mojom::AuthHost:
  void OnAuthorizationComplete(mojom::ArcSignInStatus status,
                               bool initial_signin) override;
  void OnSignInCompleteDeprecated() override;
  void OnSignInFailedDeprecated(mojom::ArcSignInStatus reason) override;
  void RequestAccountInfoDeprecated(bool initial_signin) override;
  void ReportMetrics(mojom::MetricsType metrics_type, int32_t value) override;
  void ReportAccountCheckStatus(mojom::AccountCheckStatus status) override;
  void ReportSupervisionChangeStatus(
      mojom::SupervisionChangeStatus status) override;
  void RequestPrimaryAccountInfo(
      RequestPrimaryAccountInfoCallback callback) override;
  void RequestAccountInfo(const std::string& account_name,
                          RequestAccountInfoCallback callback) override;

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // chromeos::AccountManager::Observer:
  void OnTokenUpserted(
      const chromeos::AccountManager::AccountKey& account_key) override;
  void OnAccountRemoved(
      const chromeos::AccountManager::AccountKey& account_key) override;

  void SkipMergeSessionForTesting();

 private:
  // Callback when Active Directory Enrollment Token is fetched.
  // |callback| is completed with |ArcSignInStatus| and |AccountInfo| depending
  // on the success / failure of the operation.
  void OnActiveDirectoryEnrollmentTokenFetched(
      ArcActiveDirectoryEnrollmentTokenFetcher* fetcher,
      RequestPrimaryAccountInfoCallback callback,
      ArcActiveDirectoryEnrollmentTokenFetcher::Status status,
      const std::string& enrollment_token,
      const std::string& user_id);

  // Issues a request for fetching AccountInfo for the Device Account.
  // |initial_signin| denotes whether this is the initial ARC++ provisioning
  // flow or a subsequent sign-in.
  // |callback| is completed with |ArcSignInStatus| and |AccountInfo| depending
  // on the success / failure of the operation.
  void FetchPrimaryAccountInfo(bool initial_signin,
                               RequestPrimaryAccountInfoCallback callback);

  // Callback for |FetchPrimaryAccountInfo|.
  // |fetcher| is a pointer to the object that issues this callback. Used for
  // deleting pending requests from |pending_token_requests_|.
  // |success| and |auth_code| are the callback parameters passed by
  // |ArcBackgroundAuthCodeFetcher::Fetch|.
  // |callback| is completed with |ArcSignInStatus| and |AccountInfo| depending
  // on the success / failure of the operation.
  void OnPrimaryAccountAuthCodeFetched(
      ArcAuthCodeFetcher* fetcher,
      RequestPrimaryAccountInfoCallback callback,
      bool success,
      const std::string& auth_code);

  // Called to let ARC container know the account info.
  void OnAccountInfoReadyDeprecated(mojom::ArcSignInStatus status,
                                    mojom::AccountInfoPtr account_info);

  // Callback for data removal confirmation.
  void OnDataRemovalAccepted(bool accepted);

  // |AccountManager::GetAccounts| callback.
  void OnGetAccounts(
      std::vector<chromeos::AccountManager::AccountKey> accounts);

  // Creates an |ArcBackgroundAuthCodeFetcher| for |account_id|. Can be used for
  // Device Account and Secondary Accounts. |initial_signin| denotes whether the
  // fetcher is being created for the initial ARC++ provisioning flow or for a
  // subsequent sign-in.
  std::unique_ptr<ArcBackgroundAuthCodeFetcher>
  CreateArcBackgroundAuthCodeFetcher(const std::string& account_id,
                                     bool initial_signin);

  // Deletes a completed enrollment token / auth code fetch request from
  // |pending_token_requests_|.
  void DeletePendingTokenRequest(ArcFetcherBase* fetcher);

  // Non-owning pointers.
  Profile* const profile_;
  chromeos::AccountManager* account_manager_ = nullptr;
  AccountTrackerService* const account_tracker_service_;
  ArcBridgeService* const arc_bridge_service_;

  chromeos::AccountMapperUtil account_mapper_util_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  bool url_loader_factory_for_testing_set_ = false;

  // A list of pending enrollment token / auth code requests.
  std::vector<std::unique_ptr<ArcFetcherBase>> pending_token_requests_;

  bool skip_merge_session_for_testing_ = false;

  base::WeakPtrFactory<ArcAuthService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_SERVICE_H_
