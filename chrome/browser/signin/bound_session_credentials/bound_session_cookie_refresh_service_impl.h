// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace content {
class StoragePartition;
}

class BoundSessionParamsStorage;

class BoundSessionCookieRefreshServiceImpl
    : public BoundSessionCookieRefreshService,
      public BoundSessionCookieController::Delegate,
      public content::StoragePartition::DataRemovalObserver {
 public:
  explicit BoundSessionCookieRefreshServiceImpl(
      unexportable_keys::UnexportableKeyService& key_service,
      std::unique_ptr<BoundSessionParamsStorage> session_params_storage,
      content::StoragePartition* storage_partition,
      network::NetworkConnectionTracker* network_connection_tracker);

  ~BoundSessionCookieRefreshServiceImpl() override;

  // BoundSessionCookieRefreshService:
  void Initialize() override;
  // Can be called iff the kBoundSessionExplicitRegistration feature is enabled.
  void RegisterNewBoundSession(
      const bound_session_credentials::BoundSessionParams& params) override;
  void MaybeTerminateSession(const net::HttpResponseHeaders* headers) override;
  chrome::mojom::BoundSessionThrottlerParamsPtr GetBoundSessionThrottlerParams()
      const override;
  void AddBoundSessionRequestThrottledListenerReceiver(
      mojo::PendingReceiver<chrome::mojom::BoundSessionRequestThrottledListener>
          receiver) override;

  // chrome::mojom::BoundSessionRequestThrottledListener:
  void OnRequestBlockedOnCookie(
      OnRequestBlockedOnCookieCallback resume_blocked_request) override;

  void CreateRegistrationRequest(
      BoundSessionRegistrationFetcherParam registration_params) override;

  base::WeakPtr<BoundSessionCookieRefreshService> GetWeakPtr() override;

 private:
  class BoundSessionStateTracker;
  friend class BoundSessionCookieRefreshServiceImplTest;

  // Used by tests to provide their own implementation of the
  // `BoundSessionCookieController`.
  using BoundSessionCookieControllerFactoryForTesting =
      base::RepeatingCallback<std::unique_ptr<BoundSessionCookieController>(
          const bound_session_credentials::BoundSessionParams&
              bound_session_params,
          const base::flat_set<std::string>& cookie_names,
          Delegate* delegate)>;

  // BoundSessionCookieRefreshService:
  void SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
      RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater)
      override;

  void set_controller_factory_for_testing(
      const BoundSessionCookieControllerFactoryForTesting&
          controller_factory_for_testing) {
    controller_factory_for_testing_ = controller_factory_for_testing;
  }

  void OnRegistrationRequestComplete(
      absl::optional<bound_session_credentials::BoundSessionParams>
          bound_session_params);

  // BoundSessionCookieController::Delegate
  void OnBoundSessionThrottlerParamsChanged() override;
  void TerminateSession() override;

  // StoragePartition::DataRemovalObserver:
  void OnStorageKeyDataCleared(
      uint32_t remove_mask,
      content::StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
      const base::Time begin,
      const base::Time end) override;

  std::unique_ptr<BoundSessionCookieController>
  CreateBoundSessionCookieController(
      const bound_session_credentials::BoundSessionParams& bound_session_params,
      const base::flat_set<std::string>& cookie_names);
  void InitializeBoundSession(
      const bound_session_credentials::BoundSessionParams&
          bound_session_params);

  void UpdateAllRenderers();

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  // Never null. Stored as `std::unique_ptr` for polymorphism.
  const std::unique_ptr<BoundSessionParamsStorage> session_params_storage_;
  const raw_ptr<content::StoragePartition> storage_partition_;
  const raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;
  BoundSessionCookieControllerFactoryForTesting controller_factory_for_testing_;
  RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater_;

  base::ScopedObservation<content::StoragePartition,
                          content::StoragePartition::DataRemovalObserver>
      data_removal_observation_{this};

  std::unique_ptr<BoundSessionCookieController> cookie_controller_;

  mojo::ReceiverSet<chrome::mojom::BoundSessionRequestThrottledListener>
      renderer_request_throttled_listener_;

  // There is only one active session registration at a time.
  std::unique_ptr<BoundSessionRegistrationFetcher> active_registration_request_;

  base::WeakPtrFactory<BoundSessionCookieRefreshService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_
