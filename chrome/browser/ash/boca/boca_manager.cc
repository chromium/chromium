// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/boca/babelorca/babel_orca_speech_recognizer_impl.h"
#include "chrome/browser/ash/boca/babelorca/caption_bubble_context_boca.h"
#include "chrome/browser/ash/boca/on_task/on_task_extensions_manager_impl.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/ash/boca/spotlight/spotlight_crd_manager_impl.h"
#include "chrome/browser/ash/boca/spotlight/spotlight_oauth_token_fetcher_impl.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_translation_dispatcher_impl.h"
#include "chromeos/ash/components/boca/babelorca/soda_installer.h"
#include "chromeos/ash/components/boca/boca_metrics_manager.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_crd_manager.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_remoting_client_manager.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_session_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"

namespace ash {
namespace {

std::unique_ptr<boca::BabelOrcaManager> CreateBabelOrcaManager(
    boca::BocaSessionManager* session_manager,
    Profile* profile,
    PrefService* global_prefs,
    babelorca::SodaInstaller* soda_installer,
    const std::string& application_locale,
    const std::string& caption_language,
    bool is_consumer) {
  // Passing `DoNothing` since we do not currently show settings for BabelOrca.
  auto caption_bubble_context =
      std::make_unique<babelorca::CaptionBubbleContextBoca>(base::DoNothing());
  auto babel_orca_translator =
      std::make_unique<babelorca::BabelOrcaCaptionTranslator>(
          std::make_unique<BabelOrcaTranslationDispatcherImpl>(
              std::make_unique<::captions::TranslationDispatcher>(
                  google_apis::GetBocaAPIKey(), profile)));
  // Unretained is safe since `babel_orca_manager_` instance is destroyed
  // explicitly before `boca_session_manager_`.
  auto on_caption_disabled_cb =
      base::BindRepeating(&boca::BocaSessionManager::NotifyLocalCaptionClosed,
                          base::Unretained(session_manager));
  if (is_consumer) {
    const AccountId& account_id = ash::BrowserContextHelper::Get()
                                      ->GetUserByBrowserContext(profile)
                                      ->GetAccountId();
    return boca::BabelOrcaManager::CreateAsConsumer(
        IdentityManagerFactory::GetForProfile(profile),
        profile->GetURLLoaderFactory(), std::move(caption_bubble_context),
        account_id.GetGaiaId(),
        boca::BocaAppClient::Get()->GetSchoolToolsServerBaseUrl(),
        std::move(babel_orca_translator), on_caption_disabled_cb,
        profile->GetPrefs(), application_locale, caption_language);
  }
  // Producer
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition)) {
    return nullptr;
  }

  auto speech_recognizer =
      std::make_unique<babelorca::BabelOrcaSpeechRecognizerImpl>(
          profile, soda_installer, application_locale, caption_language);
  auto babel_orca_manager = boca::BabelOrcaManager::CreateAsProducer(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(), std::move(caption_bubble_context),
      std::move(speech_recognizer), std::move(babel_orca_translator),
      on_caption_disabled_cb, profile->GetPrefs(), application_locale,
      caption_language);
  // Safe to use base::Unretained since the callback is removed in
  // `BocaManager::Shutdown()` before `babel_orca_manager_` destruction.
  auto session_caption_initializer =
      base::BindRepeating(&boca::BabelOrcaManager::SigninToTachyonAndRespond,
                          base::Unretained(babel_orca_manager.get()));
  session_manager->SetSessionCaptionInitializer(
      std::move(session_caption_initializer));
  return babel_orca_manager;
}

DeviceOAuth2TokenService& GetOAuthServiceForSpotlight() {
  return CHECK_DEREF(DeviceOAuth2TokenServiceFactory::Get());
}

}  // namespace

BocaManager::BocaManager(
    std::unique_ptr<boca::OnTaskSessionManager> on_task_session_manager,
    std::unique_ptr<boca::SessionClientImpl> session_client_impl,
    std::unique_ptr<boca::BocaSessionManager> boca_session_manager,
    std::unique_ptr<boca::InvalidationServiceImpl> invalidation_service_impl,
    std::unique_ptr<boca::BabelOrcaManager> babel_orca_manager,
    std::unique_ptr<boca::BocaMetricsManager> boca_metrics_manager,
    std::unique_ptr<boca::SpotlightSessionManager> spotlight_session_manager)
    : on_task_session_manager_(std::move(on_task_session_manager)),
      session_client_impl_(std::move(session_client_impl)),
      boca_session_manager_(std::move(boca_session_manager)),
      invalidation_service_impl_(std::move(invalidation_service_impl)),
      babel_orca_manager_(std::move(babel_orca_manager)),
      boca_metrics_manager_(std::move(boca_metrics_manager)),
      spotlight_session_manager_(std::move(spotlight_session_manager)) {
  AddObservers(nullptr);
}

BocaManager::BocaManager(Profile* profile,
                         PrefService* global_prefs,
                         const std::string& application_locale)
    : session_client_impl_(std::make_unique<boca::SessionClientImpl>()) {
  auto* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  bool is_consumer = ash::boca_util::IsConsumer(user);
  std::unique_ptr<boca::SpotlightRemotingClientManager> remoting_client_manager;
  if (ash::features::IsBocaSpotlightRobotRequesterEnabled() && !is_consumer) {
    remoting_client_manager =
        std::make_unique<boca::SpotlightRemotingClientManager>(
            std::make_unique<boca::SpotlightOAuthTokenFetcherImpl>(
                GetOAuthServiceForSpotlight()),
            profile->GetURLLoaderFactory());
  }
  boca_session_manager_ = std::make_unique<boca::BocaSessionManager>(
      session_client_impl_.get(), user->GetProfilePrefs(), user->GetAccountId(),
      /*is_producer=*/!is_consumer, std::move(remoting_client_manager));
  if (ash::features::IsBabelOrcaAvailable()) {
    const std::string caption_language = speech::GetDefaultLiveCaptionLanguage(
        application_locale, profile->GetPrefs());
    if (!is_consumer && base::FeatureList::IsEnabled(
                            ash::features::kOnDeviceSpeechRecognition)) {
      soda_installer_ = std::make_unique<babelorca::SodaInstaller>(
          global_prefs, profile->GetPrefs(), caption_language);
    }
    babel_orca_manager_ = CreateBabelOrcaManager(
        boca_session_manager_.get(), profile, global_prefs,
        soda_installer_.get(), application_locale, caption_language,
        is_consumer);
  }
  if (is_consumer) {
    on_task_session_manager_ = std::make_unique<boca::OnTaskSessionManager>(
        std::make_unique<boca::OnTaskSystemWebAppManagerImpl>(profile),
        std::make_unique<boca::OnTaskExtensionsManagerImpl>(profile));
  }

  boca_metrics_manager_ =
      std::make_unique<boca::BocaMetricsManager>(/*is_producer=*/!is_consumer);

  spotlight_session_manager_ = std::make_unique<boca::SpotlightSessionManager>(
      std::make_unique<boca::SpotlightCrdManagerImpl>(profile->GetPrefs()));

  gcm::GCMDriver* gcm_driver =
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver();
  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver();
  invalidation_service_impl_ = std::make_unique<boca::InvalidationServiceImpl>(
      gcm_driver, instance_id_driver, user->GetAccountId(),
      boca_session_manager_.get(), session_client_impl_.get(),
      boca::BocaAppClient::Get()->GetSchoolToolsServerBaseUrl());
  AddObservers(user);
}

BocaManager::~BocaManager() = default;

void BocaManager::Shutdown() {
  invalidation_service_impl_->ShutDown();
  // Dependencies like GCM driver is teardown in Shutdown phase. Reset now to
  // avoid dangling pointer.
  invalidation_service_impl_.reset();
  for (auto& obs : boca_session_manager_->observers()) {
    boca_session_manager_->RemoveObserver(&obs);
  }
  boca_session_manager_->RemoveSessionCaptionInitializer();
  babel_orca_manager_.reset();
}

void BocaManager::AddObservers(const user_manager::User* user) {
  if (babel_orca_manager_) {
    boca_session_manager_->AddObserver(babel_orca_manager_.get());
    boca_session_manager_->SetSodaInstaller(soda_installer_.get());
  }
  if (ash::boca_util::IsConsumer(user)) {
    boca_session_manager_->AddObserver(on_task_session_manager_.get());
  }
  boca_session_manager_->AddObserver(boca_metrics_manager_.get());
  boca_session_manager_->AddObserver(spotlight_session_manager_.get());
}

}  // namespace ash
