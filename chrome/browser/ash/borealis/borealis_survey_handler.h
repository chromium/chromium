// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SURVEY_HANDLER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SURVEY_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"

namespace borealis {

// Used to show a Happiness Tracking Survey when a game is closed.
class BorealisSurveyHandler
    : public BorealisWindowManager::AppWindowLifetimeObserver {
 public:
  BorealisSurveyHandler(Profile* profile,
                        BorealisWindowManager* window_manager);

  BorealisSurveyHandler(const BorealisSurveyHandler&) = delete;
  BorealisSurveyHandler& operator=(const BorealisSurveyHandler&) = delete;

  ~BorealisSurveyHandler() override;

  // BorealisWindowManager::AppWindowLifetimeObserver overrides
  void OnAppFinished(const std::string& app_id,
                     aura::Window* last_window) override;
  void OnWindowManagerDeleted(BorealisWindowManager* window_manager) override;

 private:
  friend class AshTestBase;
  friend class BorealisSurveyHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(BorealisSurveyHandlerTest,
                           GetGameIdReturnsCorrectId);
  FRIEND_TEST_ALL_PREFIXES(BorealisSurveyHandlerTest,
                           GetSurveyDataReturnsCorrectData);

  scoped_refptr<ash::HatsNotificationController> hats_notification_controller_;
  raw_ptr<Profile> profile_;
  base::ScopedObservation<BorealisWindowManager,
                          BorealisWindowManager::AppWindowLifetimeObserver>
      lifetime_observation_{this};
  base::WeakPtrFactory<BorealisSurveyHandler> weak_factory_{this};

  void CreateNotification(base::flat_map<std::string, std::string> survey_data);
  std::optional<int> GetGameId(const std::string& app_id);
  static base::flat_map<std::string, std::string> GetSurveyData(
      std::string owner_id,
      const std::string app_id,
      std::string window_title,
      std::optional<int> game_id);
};
}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SURVEY_HANDLER_H_
