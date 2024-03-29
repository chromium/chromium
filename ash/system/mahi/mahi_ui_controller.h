// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_
#define ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace ash {

// Communicates with `chromeos::MahiManager` and notifies observers of updates.
class ASH_EXPORT MahiUiController {
 public:
  enum class State {
    // The state that presents the error triggered by user actions.
    kError,

    // The state that presents questions and answers.
    kQuestionAndAnswer,

    // The state that presents the summary and outlines.
    kSummaryAndOutlines,
  };

  class Observer : public base::CheckedObserver {
   public:
    explicit Observer(MahiUiController* ui_controller);
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override;

    // Called when an answer is loaded with a success.
    virtual void OnAnswerLoaded(const std::u16string& answer) {}

    // Called when a request to refresh the panel contents was initiated.
    virtual void OnContentsRefreshInitiated() {}

    // Called when outlines are loaded with a success.
    virtual void OnOutlinesLoaded(
        const std::vector<chromeos::MahiOutline>& outlines) {}

    // Called when content refresh availability changes.
    virtual void OnRefreshAvailabilityChanged(bool available) {}

    // Called when the current state of `MahiUiController` updates. `payload`
    // indicates the additional data when state changes. If `new_state` is
    // `State::kError`, `payload` is the error that leads to this change;
    // if `new_state` is `kQuestionAndAnswer`, `payload` is a question string.
    using PayloadType = std::variant</*posted_question=*/std::u16string,
                                     /*error=*/chromeos::MahiResponseStatus>;
    virtual void OnStateChanged(State new_state,
                                const std::optional<PayloadType>& payload) {}

    // Called when a summary is loaded with a success.
    virtual void OnSummaryLoaded(const std::u16string& summary) {}

   private:
    base::ScopedObservation<MahiUiController, MahiUiController::Observer>
        observation_{this};
  };

  MahiUiController();
  MahiUiController(const MahiUiController&) = delete;
  MahiUiController& operator=(const MahiUiController&) = delete;
  ~MahiUiController();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Navigates to the summary & outlines section and notifies observers.
  void NavigateToSummaryOutlinesSection();

  // Notifies UI observers that there is a content refresh availability change.
  void NotifyRefreshAvailabilityChanged(bool available);

  // Updates the content icon and title, calls `UpdateSummaryAndOutlines` and
  // navigates to the summary view.
  void RefreshContents();

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
