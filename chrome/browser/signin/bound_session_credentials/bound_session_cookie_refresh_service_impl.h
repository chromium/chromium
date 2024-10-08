// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_key.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace content {
class StoragePartition;
}

class GURL;
class BoundSessionParamsStorage;

BASE_DECLARE_FEATURE(kMultipleBoundSessionsEnabled);

class BoundSessionCookieRefreshServiceImpl
    : public BoundSessionCookieRefreshService,
      public BoundSessionCookieController::Delegate,
      public content::StoragePartition::DataRemovalObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class SessionTerminationTrigger {
    kCookieRotationPersistentError = 0,
    kCookiesCleared = 1,
    kSessionTerminationHeader = 2,
    kSessionOverride = 3,
    kMaxValue = kSessionOverride,
  };

  BoundSessionCookieRefreshServiceImpl(
      unexportable_keys::UnexportableKeyService& key_service,
      std::unique_ptr<BoundSessionParamsStorage> session_params_storage,
      content::StoragePartition* storage_partition,
      network::NetworkConnectionTracker* network_connection_tracker,
      bool is_off_the_record_profile);

  ~BoundSessionCookieRefreshServiceImpl() override;

  // BoundSessionCookieRefreshService:
  void Initialize() override;
  void RegisterNewBoundSession(
      const bound_session_credentials::BoundSessionParams& params) override;
  void MaybeTerminateSession(const GURL& response_url,
                             const net::HttpResponseHeaders* headers) override;
  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
  GetBoundSessionThrottlerParams() const override;
  void AddBoundSessionRequestThrottledHandlerReceiver(
      mojo::PendingReceiver<chrome::mojom::BoundSessionRequestThrottledHandler>
          receiver) override;
  void CreateRegistrationRequest(
      BoundSessionRegistrationFetcherParam registration_params) override;
  base::WeakPtr<BoundSessionCookieRefreshService> GetWeakPtr() override;
  void AddObserver(
      BoundSessionCookieRefreshService::Observer* observer) override;
  void RemoveObserver(
      BoundSessionCookieRefreshService::Observer* observer) override;
  std::vector<BoundSessionDebugInfo> GetBoundSessionDebugInfo() const override;

  // chrome::mojom::BoundSessionRequestThrottledHandler:
  void HandleRequestBlockedOnCookie(
      const GURL& untrusted_request_url,
      HandleRequestBlockedOnCookieCallback resume_blocked_request) override;

 private:
  friend class BoundSessionCookieRefreshServiceImplTestBase;

  // Used by tests to provide their own implementation of the
  // `BoundSessionCookieController` and `BoundSessionRegistrationFetcher`.
  using ControllerFactoryForTesting =
      base::RepeatingCallback<std::unique_ptr<BoundSessionCookieController>(
          const bound_session_credentials::BoundSessionParams&
              bound_session_params,
          Delegate* delegate)>;
  using RegistrationFetcherFactoryForTesting =
      base::RepeatingCallback<std::unique_ptr<BoundSessionRegistrationFetcher>(
          BoundSessionRegistrationFetcherParam fetcher_params)>;
  using DebugReportFetcherFactoryForTesting =
      base::RepeatingCallback<std::unique_ptr<BoundSessionRefreshCookieFetcher>(
          std::string_view session_id,
          const GURL& refresh_url,
          bool is_off_the_record_profile,
          bound_session_credentials::RotationDebugInfo debug_info)>;

  // BoundSessionCookieRefreshService:
  void SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
      RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater)
      override;
  void SetBoundSessionParamsUpdatedCallbackForTesting(
      base::RepeatingClosure updated_callback) override;

  void set_controller_factory_for_testing(
      const ControllerFactoryForTesting& controller_factory_for_testing) {
    controller_factory_for_testing_ = controller_factory_for_testing;
  }
  void set_registration_fetcher_factory_for_testing(
      const RegistrationFetcherFactoryForTesting&
          registration_fetcher_factory_for_testing) {
    registration_fetcher_factory_for_testing_ =
        registration_fetcher_factory_for_testing;
  }
  void set_debug_report_fetcher_factory_for_testing(
      const DebugReportFetcherFactoryForTesting&
          debug_report_fetcher_factory_for_testing) {
    debug_report_fetcher_factory_for_testing_ =
        debug_report_fetcher_factory_for_testing;
  }

  // Convenience getter while `BoundSessionCookieRefreshService` only supports a
  // single session.
  // Returns `nullptr` if no sessions are currently running.
  // TODO(http://b/325451275): remove the getter once multiple sessions are
  // supported.
  BoundSessionCookieController* cookie_controller() const;

  void StartRegistrationRequest();
  void OnRegistrationRequestComplete(
      std::optional<bound_session_credentials::BoundSessionParams>
          bound_session_params);

  // BoundSessionCookieController::Delegate
  void OnBoundSessionThrottlerParamsChanged() override;
  void OnPersistentErrorEncountered(
      BoundSessionCookieController* controller,
      BoundSessionRefreshCookieFetcher::Result refresh_error) override;

  // StoragePartition::DataRemovalObserver:
  void OnStorageKeyDataCleared(
      uint32_t remove_mask,
      content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
      const base::Time begin,
      const base::Time end) override;

  std::unique_ptr<BoundSessionCookieController>
  CreateBoundSessionCookieController(
      const bound_session_credentials::BoundSessionParams& bound_session_params,
      bool is_off_the_record_profile);
  void InitializeBoundSession(
      const bound_session_credentials::BoundSessionParams&
          bound_session_params);

  void UpdateAllRenderers();

  // Terminates an ongoing device bound session pointed by `controller`, clears
  // the session params from storage and updates all renderers.
  void TerminateSession(BoundSessionCookieController* controller,
                        SessionTerminationTrigger trigger,
                        std::optional<BoundSessionRefreshCookieFetcher::Result>
                            refresh_error = std::nullopt);
  void RecordSessionTerminationTrigger(SessionTerminationTrigger trigger);
  void NotifyBoundSessionTerminated(
      const GURL& site,
      const base::flat_set<std::string>& bound_cookie_names);

  void MaybeReportTerminationReason(
      BoundSessionCookieController* controller,
      SessionTerminationTrigger trigger,
      std::optional<BoundSessionRefreshCookieFetcher::Result> refresh_error);
  void OnTerminationReasonReportCompleted(
      BoundSessionRefreshCookieFetcher* reporter,
      BoundSessionRefreshCookieFetcher::Result result);

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  // Never null. Stored as `std::unique_ptr` for polymorphism.
  const std::unique_ptr<BoundSessionParamsStorage> session_params_storage_;
  const raw_ptr<content::StoragePartition> storage_partition_;
  const raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;
  // Required to attach X-Client-Data header to session registration and cookie
  // rotation requests for GWS-visible Finch experiment.
  const bool is_off_the_record_profile_;
  ControllerFactoryForTesting controller_factory_for_testing_;
  RegistrationFetcherFactoryForTesting
      registration_fetcher_factory_for_testing_;
  DebugReportFetcherFactoryForTesting debug_report_fetcher_factory_for_testing_;
  RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater_;
  base::RepeatingClosure session_updated_callback_for_testing_;

  base::ScopedObservation<content::StoragePartition,
                          content::StoragePartition::DataRemovalObserver>
      data_removal_observation_{this};

  base::flat_map<BoundSessionKey, std::unique_ptr<BoundSessionCookieController>>
      cookie_controllers_;
  std::set<std::unique_ptr<BoundSessionRefreshCookieFetcher>,
           base::UniquePtrComparator>
      termination_reason_reporters_;

  mojo::ReceiverSet<chrome::mojom::BoundSessionRequestThrottledHandler>
      renderer_request_throttled_handler_;

  // Only one session registration is active at a time.
  base::queue<std::unique_ptr<BoundSessionRegistrationFetcher>>
      registration_requests_;

  base::ObserverList<BoundSessionCookieRefreshService::Observer> observers_;

  base::WeakPtrFactory<BoundSessionCookieRefreshService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_
