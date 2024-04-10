// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_
#define ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// Communicates with `chromeos::MahiManager` and notifies delegates of updates.
class ASH_EXPORT MahiUiController {
 public:
  // Establishes the connection between `MahiUiController` and dependent views.
  class Delegate : public base::CheckedObserver {
   public:
    explicit Delegate(MahiUiController* ui_controller);
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    ~Delegate() override;

    // Returns the view that associates with the delegate.
    virtual views::View* GetView() = 0;

    // Returns the visibility of the delegate's associated view for `state`.
    virtual bool GetViewVisibility(VisibilityState state) const = 0;

    // Notifies of a Mahi UI update.
    virtual void OnUpdated(const MahiUiUpdate& update) {}

   private:
    base::ScopedObservation<MahiUiController, Delegate> observation_{this};
  };

  MahiUiController();
  MahiUiController(const MahiUiController&) = delete;
  MahiUiController& operator=(const MahiUiController&) = delete;
  ~MahiUiController();

  void AddDelegate(Delegate* delegate);
  void RemoveDelegate(Delegate* delegate);

  // Navigates to the summary & outlines section and notifies delegates.
  void NavigateToSummaryOutlinesSection();

  // Notifies delegates that there is a content refresh availability change.
  void NotifyRefreshAvailabilityChanged(bool available);

  // Updates the content icon and title, calls `UpdateSummaryAndOutlines` and
  // navigates to the summary view.
  void RefreshContents();

  // Sends `question` to the backend. `current_panel_content` determines if the
  // `question` is regarding the current content displayed on the panel.
  void SendQuestion(const std::u16string& question, bool current_panel_content);

  // Sends requests to the backend to update summary and outlines.
  // `delegates_` will be notified of the updated summary and outlines when
  // requests are fulfilled.
  void UpdateSummaryAndOutlines();

 private:
  // Handles the error indicated by `status`. `status` cannot be
  // `chromeos::MahiResponseStatus::kSuccess`.
  void HandleErrorStatus(chromeos::MahiResponseStatus status);

  // Notifies `delegates_` of `update`.
  void NotifyUiUpdate(const MahiUiUpdate& update);

  // Sets the visibility state and notifies `delegates_` of `update`.
  void SetVisibilityStateAndNotifyUiUpdate(VisibilityState state,
                                           const MahiUiUpdate& update);

  // Callbacks of `chromeos::MahiManager` APIs ---------------------------------

  void OnAnswerLoaded(std::optional<std::u16string> answer,
                      chromeos::MahiResponseStatus status);

  void OnOutlinesLoaded(std::vector<chromeos::MahiOutline> outlines,
                        chromeos::MahiResponseStatus status);

  void OnSummaryLoaded(std::u16string summary_text,
                       chromeos::MahiResponseStatus status);

  // The current state. Use `VisibilityState::kSummaryAndOutlines` by default.
  VisibilityState visibility_state_ = VisibilityState::kSummaryAndOutlines;

  base::ObserverList<Delegate> delegates_;

  base::WeakPtrFactory<MahiUiController> weak_ptr_factory_{this};
};

}  // namespace ash

namespace base {

template <>
struct ScopedObservationTraits<ash::MahiUiController,
                               ash::MahiUiController::Delegate> {
  static void AddObserver(ash::MahiUiController* controller,
                          ash::MahiUiController::Delegate* delegate) {
    controller->AddDelegate(delegate);
  }
  static void RemoveObserver(ash::MahiUiController* controller,
                             ash::MahiUiController::Delegate* delegate) {
    controller->RemoveDelegate(delegate);
  }
};

}  // namespace base

#endif  // ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_
