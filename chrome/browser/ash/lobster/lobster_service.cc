// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_service.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/lobster/lobster_candidate_id_generator.h"
#include "chrome/browser/ash/lobster/lobster_image_fetcher.h"
#include "chrome/browser/ash/lobster/lobster_image_provider_from_memory.h"
#include "chrome/browser/ash/lobster/lobster_image_provider_from_snapper.h"
#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"
#include "chrome/browser/ash/lobster/lobster_system_state_provider_impl.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "components/manta/snapper_provider.h"
#include "ui/display/screen.h"

namespace {

constexpr std::u16string_view kAnnouncementViewName = u"Lobster";

constexpr base::TimeDelta kAnnouncementDelay = base::Milliseconds(200);

}  // namespace

LobsterService::LobsterService(
    std::unique_ptr<manta::SnapperProvider> snapper_provider,
    Profile* profile)
    : profile_(profile),
      // `LobsterService` is only created for regular profiles as specified in
      // the `LobsterServiceProvider` constructor, so the below call should
      // always return a non-null pointer.
      account_id_(CHECK_DEREF(ash::AnnotatedAccountId::Get(profile))),
      image_provider_(std::move(snapper_provider)),
      image_fetcher_(std::make_unique<LobsterImageFetcher>(
          std::make_unique<LobsterImageProviderFromSnapper>(
              image_provider_.get(),
              &candidate_id_generator_))),
      resizer_(std::make_unique<LobsterCandidateResizer>(image_fetcher_.get())),
      system_state_provider_(std::make_unique<LobsterSystemStateProviderImpl>(
          profile->GetPrefs(),
          IdentityManagerFactory::GetForProfile(profile),
          /*is_in_demo_mode=*/ash::demo_mode::IsDeviceInDemoMode())),
      announcer_(
          std::make_unique<LobsterLiveRegionAnnouncer>(kAnnouncementViewName)) {
  if (profile != nullptr) {
    PrefService* pref_service = profile->GetPrefs();
    pref_change_registrar_.Init(pref_service);
    pref_change_registrar_.Add(
        ash::prefs::kLobsterEnabled,
        base::BindRepeating(
            [](PrefService* pref_service) {
              if (pref_service->GetBoolean(ash::prefs::kLobsterEnabled) &&
                  chromeos::editor_menu::GetConsentStatusFromInteger(
                      pref_service->GetInteger(
                          ash::prefs::kOrcaConsentStatus)) ==
                      chromeos::editor_menu::EditorConsentStatus::kDeclined) {
                pref_service->SetInteger(
                    ash::prefs::kOrcaConsentStatus,
                    base::to_underlying(
                        chromeos::editor_menu::EditorConsentStatus::kUnset));
              }
            },
            pref_service));
  }
}

LobsterService::~LobsterService() = default;

void LobsterService::SetActiveSession(ash::LobsterSession* session) {
  active_session_ = session;
}

ash::LobsterSession* LobsterService::active_session() {
  return active_session_;
}

LobsterSystemStateProvider* LobsterService::system_state_provider() {
  return system_state_provider_.get();
}

void LobsterService::RequestCandidates(
    const std::string& query,
    int num_candidates,
    ash::RequestCandidatesCallback callback) {
  image_fetcher_->RequestCandidates(query, num_candidates, std::move(callback));
}

void LobsterService::InflateCandidate(uint32_t seed,
                                      const std::string& query,
                                      ash::InflateCandidateCallback callback) {
  resizer_->InflateImage(seed, query, std::move(callback));
}

void LobsterService::QueueInsertion(const std::string& image_bytes,
                                    StatusCallback insert_status_callback) {
  queued_insertion_ = std::make_unique<LobsterInsertion>(
      image_bytes, std::move(insert_status_callback));
}

void LobsterService::ShowDisclaimerUI() {
  if (chromeos::MagicBoostState::Get()->IsUserEligibleForGenAIFeatures()) {
    ash::MagicBoostControllerAsh::Get()->ShowDisclaimerUi(
        /*display_id=*/display::Screen::Get()->GetPrimaryDisplay().id(),
        /*action=*/
        crosapi::mojom::MagicBoostController::TransitionAction::
            kShowLobsterPanel,
        /*opt_in_features=*/
        crosapi::mojom::MagicBoostController::OptInFeatures::kOrcaAndHmr);
  }
}

void LobsterService::LoadUI(std::optional<std::string> query,
                            ash::LobsterMode mode,
                            const gfx::Rect& caret_bounds) {
  bubble_coordinator_.LoadUI(
      profile_, query, mode, caret_bounds,
      /*should_show_feedback=*/
      profile_->GetPrefs()->GetInteger(
          ash::prefs::kLobsterEnterprisePolicySettings) ==
              base::to_underlying(ash::LobsterEnterprisePolicyValue::
                                      kAllowedWithModelImprovement) &&
          base::FeatureList::IsEnabled(ash::features::kLobsterFeedback));
}

void LobsterService::ShowUI() {
  bubble_coordinator_.ShowUI();
}

void LobsterService::CloseUI() {
  bubble_coordinator_.CloseUI();
}

void LobsterService::OnFocus(int context_id) {
  if (queued_insertion_ == nullptr) {
    return;
  }

  if (queued_insertion_->HasTimedOut()) {
    queued_insertion_ = nullptr;
    return;
  }

  queued_insertion_->Commit();
  queued_insertion_ = nullptr;
}

void LobsterService::AnnounceLater(const std::u16string& message) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](LobsterAnnouncer* announcer, const std::u16string& message) {
            announcer->Announce(message);
          },
          announcer_.get(), message),
      kAnnouncementDelay);
}

bool LobsterService::CanShowFeatureSettingsToggle() {
  // An empty text input context should not affect the visibility of the feature
  // settings toggle.
  ash::LobsterSystemState::SystemChecks system_checks =
      system_state_provider()
          ->GetSystemState(ash::LobsterTextInputContext())
          .failed_checks;

  return (!system_checks.Has(
              ash::LobsterSystemCheck::kInvalidAccountCapabilities) &&
          !system_checks.Has(ash::LobsterSystemCheck::kInvalidRegion) &&
          !system_checks.Has(ash::LobsterSystemCheck::kInvalidAccountType) &&
          !system_checks.Has(ash::LobsterSystemCheck::kInvalidFeatureFlags) &&
          !system_checks.Has(ash::LobsterSystemCheck::kUnsupportedHardware) &&
          !system_checks.Has(ash::LobsterSystemCheck::kUnsupportedInKioskMode));
}

bool LobsterService::OverrideLobsterImageProviderForTesting() {
  image_fetcher_->SetProvider(std::make_unique<LobsterImageProviderFromMemory>(
      &candidate_id_generator_));
  return true;
}

void LobsterService::set_lobster_system_state_provider_for_testing(
    std::unique_ptr<LobsterSystemStateProvider> provider) {
  system_state_provider_ = std::move(provider);
}
