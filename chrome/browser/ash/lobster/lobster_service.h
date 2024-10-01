// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SERVICE_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SERVICE_H_

#include <string>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/lobster/image_fetcher.h"
#include "chrome/browser/ash/lobster/lobster_bubble_coordinator.h"
#include "chrome/browser/ash/lobster/lobster_candidate_id_generator.h"
#include "chrome/browser/ash/lobster/lobster_candidate_resizer.h"
#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"
#include "components/keyed_service/core/keyed_service.h"

namespace manta {
class SnapperProvider;
}  // namespace manta

class Profile;

class LobsterService : public KeyedService {
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

  bool SubmitFeedback(const std::string& query,
                      const std::string& model_version,
                      const std::string& description,
                      const std::string& image_bytes);

  void LoadUI(std::optional<std::string> query);

  void ShowUI();

  void CloseUI();

 private:
  // Not owned by this class
  raw_ptr<Profile> profile_;
  raw_ptr<ash::LobsterSession> active_session_;

  LobsterCandidateIdGenerator candidate_id_generator_;

  std::unique_ptr<manta::SnapperProvider> image_provider_;

  ImageFetcher image_fetcher_;
  LobsterCandidateResizer resizer_;

  LobsterSystemStateProvider system_state_provider_;

  ash::LobsterBubbleCoordinator bubble_coordinator_;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SERVICE_H_
