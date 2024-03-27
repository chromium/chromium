// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_
#define ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace ash {

// Communicates with `chromeos::MahiManager` and notifies observers of updates.
class ASH_EXPORT MahiUiController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when an answer is loaded with a success.
    virtual void OnAnswerLoaded(const std::u16string& answer) {}

    // Called when receiving any error status. `status` cannot be `kSuccess`.
    virtual void OnError(chromeos::MahiResponseStatus status) {}

    // Called when navigating to the summary & outlines section.
    virtual void OnNavigatedToSummaryOutlinesSection() {}

    // Called when outlines are loaded with a success.
    virtual void OnOutlinesLoaded(
        const std::vector<chromeos::MahiOutline>& outlines) {}

    // Called when a question is posted to backend.
    virtual void OnQuestionPosted(const std::u16string& question) {}

    // Called when a summary is loaded with a success.
    virtual void OnSummaryLoaded(const std::u16string& summary) {}
  };

  MahiUiController();
  MahiUiController(const MahiUiController&) = delete;
  MahiUiController& operator=(const MahiUiController&) = delete;
  ~MahiUiController();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Navigates to the summary & outlines section and notifies observers.
  void NavigateToSummaryOutlinesSection();

  // Sends `question` to the backend. `current_panel_content` determines if the
  // `question` is regarding the current content displayed on the panel.
  void SendQuestion(const std::u16string& question, bool current_panel_content);

  // Sends requests to the backend to update summary and outlines.
  // `observers_` will be notified of the updated summary and outlines when
  // requests are fulfilled.
  void UpdateSummaryAndOutlines();

 private:
  // Handles the error indicated by `status`. `status` cannot be
  // `chromeos::MahiResponseStatus::kSuccess`.
  void HandleErrorStatus(chromeos::MahiResponseStatus status);

  // Callbacks of `chromeos::MahiManager` APIs ---------------------------------

  void OnAnswerLoaded(std::optional<std::u16string> answer,
                      chromeos::MahiResponseStatus status);

  void OnOutlinesLoaded(std::vector<chromeos::MahiOutline> outlines,
                        chromeos::MahiResponseStatus status);

  void OnSummaryLoaded(std::u16string summary_text,
                       chromeos::MahiResponseStatus status);

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<MahiUiController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_
