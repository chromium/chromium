// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/boca_manager.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/ash/boca/babelorca/babel_orca_speech_recognizer_impl.h"
#include "chrome/browser/ash/boca/babelorca/babel_orca_translation_dispatcher_impl.h"
#include "chrome/browser/ash/boca/babelorca/caption_bubble_context_boca.h"
#include "chrome/browser/ash/boca/on_task/on_task_extensions_manager_impl.h"
#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_translation_dispatcher.h"
#include "chromeos/ash/components/boca/babelorca/caption_controller.h"
#include "chromeos/ash/components/boca/boca_metrics_manager.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"
#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/user_manager/user.h"
#include "google_apis/google_api_keys.h"

namespace ash {
namespace {

std::unique_ptr<boca::BabelOrcaManager> CreateBabelOrcaManager(
    Profile* profile,
    const std::string& application_locale,
    bool is_consumer) {
  // Passing `DoNothing` since we do not currently show settings for BabelOrca.
  auto caption_bubble_context =
      std::make_unique<babelorca::CaptionBubbleContextBoca>(
          base::DoNothing(), /*translation_enabled=*/is_consumer);
  if (is_consumer) {
    auto babel_orca_translator =
        std::make_unique<babelorca::BabelOrcaCaptionTranslator>(
            std::make_unique<BabelOrcaTranslationDispatcherImpl>(
                std::make_unique<::captions::TranslationDispatcher>(
                    google_apis::GetBocaAPIKey(), profile)));
    const AccountId& account_id = ash::BrowserContextHelper::Get()
                                      ->GetUserByBrowserContext(profile)
                                      ->GetAccountId();
    return boca::BabelOrcaManager::CreateAsConsumer(
        IdentityManagerFactory::GetForProfile(profile),
        profile->GetURLLoaderFactory(),
        std::make_unique<babelorca::CaptionController>(
            std::move(caption_bubble_context), profile->GetPrefs(),
            application_locale),
        account_id.GetGaiaId(), std::move(babel_orca_translator),
        profile->GetPrefs());
  }
  // Producer
  if (!base::FeatureList::IsEnabled(
          ash::features::kOnDeviceSpeechRecognition)) {
    return nullptr;
  }
  auto speech_recognizer =
      std::make_unique<babelorca::BabelOrcaSpeechRecognizerImpl>(profile);
  return boca::BabelOrcaManager::CreateAsProducer(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(),
      ::captions::LiveCaptionControllerFactory::GetForProfile(profile),
      std::move(caption_bubble_context), std::move(speech_recognizer));
}

}  // namespace

BocaManager::BocaManager(
    std::unique_ptr<boca::OnTaskSessionManager> on_task_session_manager,
    std::unique_ptr<boca::SessionClientImpl> session_client_impl,
    std::unique_ptr<boca::BocaSessionManager> boca_session_manager,
    std::unique_ptr<boca::InvalidationServiceImpl> invalidation_service_impl,
    std::unique_ptr<boca::BabelOrcaManager> babel_orca_manager,
    std::unique_ptr<boca::BocaMetricsManager> boca_metrics_manager)
    : on_task_session_manager_(std::move(on_task_session_manager)),
      session_client_impl_(std::move(session_client_impl)),
      boca_session_manager_(std::move(boca_session_manager)),
      invalidation_service_impl_(std::move(invalidation_service_impl)),
      babel_orca_manager_(std::move(babel_orca_manager)),
      boca_metrics_manager_(std::move(boca_metrics_manager)) {
  AddObservers(nullptr);
}

BocaManager::BocaManager(Profile* profile,
                         const std::string& application_locale)
    : session_client_impl_(std::make_unique<boca::SessionClientImpl>()) {
  auto* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  bool is_consumer = ash::boca_util::IsConsumer(user);
  boca_session_manager_ = std::make_unique<boca::BocaSessionManager>(
      session_client_impl_.get(), user->GetAccountId(),
      /*is_producer=*/!is_consumer);
  if (ash::features::IsBabelOrcaAvailable()) {
    babel_orca_manager_ =
        CreateBabelOrcaManager(profile, application_locale, is_consumer);
  }
  if (is_consumer) {
    on_task_session_manager_ = std::make_unique<boca::OnTaskSessionManager>(
        std::make_unique<boca::OnTaskSystemWebAppManagerImpl>(profile),
        std::make_unique<boca::OnTaskExtensionsManagerImpl>(profile));
  }
  boca_metrics_manager_ =
      std::make_unique<boca::BocaMetricsManager>(/*is_producer*/ !is_consumer);

  gcm::GCMDriver* gcm_driver =
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver();
  instance_id::InstanceIDDriver* instance_id_driver =
      instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver();
  invalidation_service_impl_ = std::make_unique<boca::InvalidationServiceImpl>(
      gcm_driver, instance_id_driver, user->GetAccountId(),
      boca_session_manager_.get(), session_client_impl_.get());
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
  babel_orca_manager_.reset();
}

void BocaManager::AddObservers(const user_manager::User* user) {
  if (babel_orca_manager_) {
    boca_session_manager_->AddObserver(babel_orca_manager_.get());
  }
  if (ash::boca_util::IsConsumer(user)) {
    boca_session_manager_->AddObserver(on_task_session_manager_.get());
  }
  boca_session_manager_->AddObserver(boca_metrics_manager_.get());
}

}  // namespace ash
