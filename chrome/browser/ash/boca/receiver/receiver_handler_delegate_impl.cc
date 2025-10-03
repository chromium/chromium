// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/receiver/receiver_handler_delegate_impl.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/check_deref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/boca/spotlight/spotlight_oauth_token_fetcher_impl.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_delegate.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/receiver/boca_device_auth_token_service.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash::boca_receiver {

ReceiverHandlerDelegateImpl::ReceiverHandlerDelegateImpl(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

ReceiverHandlerDelegateImpl::~ReceiverHandlerDelegateImpl() = default;

std::unique_ptr<boca::InvalidationService>
ReceiverHandlerDelegateImpl::CreateInvalidationService(
    boca::InvalidationServiceDelegate* invalidation_service_delegate) const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  gcm::GCMDriver* gcm_driver =
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver();
  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver();
  return std::make_unique<boca::InvalidationServiceImpl>(
      gcm_driver, instance_id_driver, invalidation_service_delegate);
}

std::unique_ptr<google_apis::RequestSender>
ReceiverHandlerDelegateImpl::CreateRequestSender(
    std::string_view requester_id,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) const {
  Profile* profile = Profile::FromWebUI(web_ui_);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetURLLoaderFactory();
  auto auth_service = std::make_unique<
      BocaDeviceAuthTokenService<DeviceOAuth2TokenServiceFactory>>(
      OAuth2AccessTokenManager::ScopeSet{boca::kSchoolToolsAuthScope},
      requester_id);
  return std::make_unique<google_apis::RequestSender>(
      std::move(auth_service), url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      /*custom_user_agent=*/"", traffic_annotation);
}

std::unique_ptr<boca::SpotlightRemotingClientManager>
ReceiverHandlerDelegateImpl::CreateRemotingClientManager() {
  // TODO(crbug.com/445415017): Replace `SpotlightOAuthTokenFetcher` by
  // `BocaDeviceAuthTokenService`
  return std::make_unique<boca::SpotlightRemotingClientManagerImpl>(
      std::make_unique<boca::SpotlightOAuthTokenFetcherImpl>(
          CHECK_DEREF(DeviceOAuth2TokenServiceFactory::Get())),
      Profile::FromWebUI(web_ui_)->GetURLLoaderFactory());
}

bool ReceiverHandlerDelegateImpl::IsAppEnabled(std::string_view url) {
  const session_manager::Session* session =
      session_manager::SessionManager::Get()->GetActiveSession();
  if (!session) {
    return false;
  }
  std::optional<KioskApp> app = KioskController::Get().GetAppById(
      KioskAppId::ForWebApp(session->account_id()));
  return app.has_value() && app->url().has_value() &&
         app->url().value() == GURL(url);
}

}  // namespace ash::boca_receiver
