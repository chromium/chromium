// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SERVICE_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SERVICE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/lobster/lobster_announcer.h"
#include "chrome/browser/ash/lobster/lobster_bubble_coordinator.h"
#include "chrome/browser/ash/lobster/lobster_candidate_id_generator.h"
#include "chrome/browser/ash/lobster/lobster_candidate_resizer.h"
#include "chrome/browser/ash/lobster/lobster_event_sink.h"
#include "chrome/browser/ash/lobster/lobster_image_fetcher.h"
#include "chrome/browser/ash/lobster/lobster_insertion.h"
#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace manta {
class SnapperProvider;
}  // namespace manta

class Profile;

class LobsterService : public KeyedService, public LobsterEventSink {
 public:
  explicit LobsterService(
      std::unique_ptr<manta::SnapperProvider> image_provider,
      Profile* profile);
  ~LobsterService() override;

  void SetActiveSession(ash::LobsterSession* session);

  ash::LobsterSession* active_session();
  LobsterSystemStateProvider* system_state_provider();

  void RequestCandidates(const std::string& query,
                         int num_candidates,
                         ash::RequestCandidatesCallback);

  void InflateCandidate(uint32_t seed,
                        const std::string& query,
                        ash::InflateCandidateCallback);

  void QueueInsertion(const std::string& image_bytes,
                      StatusCallback insert_status_callback);

  void ShowDisclaimerUI();

  void LoadUI(std::optional<std::string> query,
              ash::LobsterMode mode,
              const gfx::Rect& caret_bounds);

  void ShowUI();

  void CloseUI();

  const AccountId& GetAccountId() const { return account_id_; }

  void AnnounceLater(const std::u16string& message);

  // Relevant input events
  void OnFocus(int context_id) override;

  bool CanShowFeatureSettingsToggle();

  bool OverrideLobsterImageProviderForTesting();

  void set_lobster_system_state_provider_for_testing(
      std::unique_ptr<LobsterSystemStateProvider> provider);

 private:
  // Not owned by this class
  raw_ptr<Profile> profile_;
  AccountId account_id_;
  raw_ptr<ash::LobsterSession> active_session_;

  LobsterCandidateIdGenerator candidate_id_generator_;

  std::unique_ptr<manta::SnapperProvider> image_provider_;

  std::unique_ptr<LobsterImageFetcher> image_fetcher_;
  std::unique_ptr<LobsterCandidateResizer> resizer_;

  std::unique_ptr<LobsterSystemStateProvider> system_state_provider_;

  ash::LobsterBubbleCoordinator bubble_coordinator_;

  std::unique_ptr<LobsterInsertion> queued_insertion_;

  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<LobsterLiveRegionAnnouncer> announcer_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SERVICE_H_
