// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_SERVICE_H_
#define CHROME_BROWSER_INDIGO_INDIGO_SERVICE_H_

#include <optional>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefChangeRegistrar;
class PrefService;
class Profile;

namespace indigo {

class ApiClient;

struct RemoteEligibility {
  bool is_service_supported_for_account = false;
  bool has_user_image = false;
};

enum class LocalEligibility {
  kEligible,
  kNotSignedIn,
  kMissingCapabilities,
  kDisabledByPolicy,
  kMissingScript,
};

// Combined eligibility status including local constraints (features, profile
// state) and remote server checks.
struct CombinedEligibility {
  CombinedEligibility();
  CombinedEligibility(const CombinedEligibility&);
  CombinedEligibility& operator=(const CombinedEligibility&);
  CombinedEligibility(CombinedEligibility&&);
  CombinedEligibility& operator=(CombinedEligibility&&);
  ~CombinedEligibility();

  LocalEligibility local_eligibility = LocalEligibility::kNotSignedIn;
  base::expected<RemoteEligibility, std::string> remote_eligibility =
      base::unexpected("Status not available");
  bool has_onboarded_pref = false;

  bool CanGenerateImage() const;
  bool ReadyToOnboard() const;
};

class IndigoService : public KeyedService,
                      public signin::IdentityManager::Observer {
 public:
  using LocalEligibilityChangedCallback =
      base::RepeatingCallback<void(LocalEligibility)>;
  using CombinedEligibilityCallback =
      base::OnceCallback<void(const CombinedEligibility&)>;
  using RemoteEligibilityCallback =
      base::OnceCallback<void(base::expected<RemoteEligibility, std::string>)>;
  using RemoteEligibilityFetcher =
      base::RepeatingCallback<void(RemoteEligibilityCallback)>;

  IndigoService(Profile* profile,
                signin::IdentityManager* identity_manager,
                PrefService* pref_service);
  ~IndigoService() override;

  // Returns the path to the Indigo script if available (either via command line
  // override or component updater).
  static std::optional<base::FilePath> GetScriptPath();

  // Get and subscribe to information about whether the profile is eligible to
  // use the feature, as far as Chrome is concerned. This includes the user
  // being signed in with an account which isn't barred, for instance, but not
  // whether the user has actually completed all the steps required to use it.
  //
  // If the profile is not locally eligible, there is no point in offering the
  // feature to users.
  LocalEligibility GetLocalEligibility() const {
    return last_known_local_eligibility_;
  }
  bool IsLocallyEligible() const {
    return GetLocalEligibility() == LocalEligibility::kEligible;
  }
  base::CallbackListSubscription RegisterLocalEligibilityChangedCallback(
      LocalEligibilityChangedCallback callback);

  ApiClient& GetApiClient() const {
    CHECK(api_client_);
    return *api_client_;
  }

  // Anchored messages are rate-limited to reduce user fatigue. Clients should
  // use `CanShowAnchoredMessage` to check eligibility before displaying an
  // anchored message, and call `AnchoredMessageShown` when they do.
  bool CanShowAnchoredMessage() const;
  void AnchoredMessageShown();

  // Determine whether the feature is capable of generating images right now.
  // This may require contacting the service.
  void GetCombinedEligibility(CombinedEligibilityCallback callback);

  // Invalidate the cached or in-flight status fetch from the server, so it will
  // be refetched when GetCombinedEligibility is called next.
  void InvalidateRemoteEligibility();

  // KeyedService:
  void Shutdown() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  void SetRemoteEligibilityFetcherForTesting(RemoteEligibilityFetcher fetcher);

 private:
  LocalEligibility ComputeLocalEligibility() const;
  void UpdateLocalEligibilityAndNotify();
  void OnRemoteEligibilityReceived(
      base::expected<RemoteEligibility, std::string> eligibility_or_error);
  void TriggerRemoteEligibilityFetch();

  raw_ptr<Profile> profile_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<PrefService> pref_service_;

  LocalEligibility last_known_local_eligibility_;
  base::RepeatingCallbackList<void(LocalEligibility)>
      local_eligibility_callback_list_;

  void OnIndigoComponentReady();

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::CallbackListSubscription indigo_component_ready_subscription_;
  std::unique_ptr<ApiClient> api_client_;

  // The earliest time the anchored message can be shown again.
  base::TimeTicks anchored_message_not_before_;

  // True if a fetch for remote eligibility is currently in flight.
  bool remote_eligibility_fetch_in_progress_ = false;

  // Overrides the server fetch for testing purposes.
  RemoteEligibilityFetcher remote_eligibility_fetcher_;

  // The cached result of the last remote eligibility fetch (or error
  // description). It is empty if no fetch has succeeded yet or if it was
  // invalidated.
  std::optional<base::expected<RemoteEligibility, std::string>>
      remote_eligibility_;

  // Callbacks waiting for the current remote eligibility fetch to complete.
  std::vector<CombinedEligibilityCallback> pending_callbacks_;

  // Weak pointer factory used specifically for remote eligibility fetches to
  // allow invalidation of in-flight requests.
  base::WeakPtrFactory<IndigoService> remote_eligibility_weak_factory_{this};
};

}  // namespace indigo
#endif  // CHROME_BROWSER_INDIGO_INDIGO_SERVICE_H_
