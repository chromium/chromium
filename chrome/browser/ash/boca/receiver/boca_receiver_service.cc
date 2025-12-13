// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/receiver/boca_receiver_service.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/boca/spotlight/spotlight_oauth_token_fetcher_impl.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/boca/invalidations/fcm_handler.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"

namespace ash {
namespace {

std::unique_ptr<boca::FCMHandler> CreateFcmHandler(Profile* profile) {
  gcm::GCMDriver* gcm_driver =
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver();
  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver();
  return std::make_unique<boca::FCMHandlerImpl>(gcm_driver, instance_id_driver);
}

}  // namespace

BocaReceiverService::BocaReceiverService(Profile* profile)
    : fcm_handler_(CreateFcmHandler(profile)),
      remoting_client_(
          std::make_unique<boca::SpotlightRemotingClientManagerImpl>(
              // TODO(crbug.com/445415017): Replace `SpotlightOAuthTokenFetcher`
              // by `BocaDeviceAuthTokenService`
              std::make_unique<boca::SpotlightOAuthTokenFetcherImpl>(
                  CHECK_DEREF(DeviceOAuth2TokenServiceFactory::Get())),
              profile->GetURLLoaderFactory())) {}

BocaReceiverService::~BocaReceiverService() = default;

void BocaReceiverService::Shutdown() {
  fcm_handler_.reset();
  // Make sure any ongoing remoting client session is stopped before resetting
  // `remoting_client_`. If there is no ongoing remoting session the callback
  // will run immediately.
  boca::SpotlightRemotingClientManager* remoting_client_ptr =
      remoting_client_.get();
  remoting_client_ptr->StopCrdClient(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          [](std::unique_ptr<boca::SpotlightRemotingClientManager>) {},
          std::move(remoting_client_))));
}

boca::FCMHandler* BocaReceiverService::fcm_handler() const {
  return fcm_handler_.get();
}

boca::SpotlightRemotingClientManager* BocaReceiverService::remoting_client()
    const {
  return remoting_client_.get();
}

}  // namespace ash
