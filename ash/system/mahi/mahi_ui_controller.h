// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_
#define ASH_SYSTEM_MAHI_MAHI_UI_CONTROLLER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class View;
}  // namespace views

class AccountId;

namespace ash {

// Communicates with `chromeos::MahiManager` and notifies delegates of updates.
class ASH_EXPORT MahiUiController : public SessionObserver {
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

  // Lists question sources.
  // Note: this should be kept in sync with `MahiQuestionSource` enum in
  // tools/metrics/histograms/metadata/ash/enums.xml
  enum class QuestionSource {
    // From the Mahi menu view.
    kMenuView,

    // From the Mahi panel view.
    kPanel,

    // From the retry button.
    kRetry,

    kMaxValue = kRetry,
  };

  MahiUiController();
  MahiUiController(const MahiUiController&) = delete;
  MahiUiController& operator=(const MahiUiController&) = delete;
  ~MahiUiController() override;

  void AddDelegate(Delegate* delegate);
  void RemoveDelegate(Delegate* delegate);

  // Opens/closes the mahi panel on the display associated with `display_id`.
  // The panel is positioned on top of the provided `mahi_menu_bounds`.
  void OpenMahiPanel(int64_t display_id, gfx::Rect mahi_menu_bounds);
  void CloseMahiPanel();

  bool IsMahiPanelOpen();

  // Navigates to the Q&A view and notifies delegates.
  void NavigateToQuestionAnswerView();

  // Navigates to the summary & outlines section and notifies delegates.
  void NavigateToSummaryOutlinesSection();

  // Notifies delegates that there is a content refresh availability change.
  void NotifyRefreshAvailabilityChanged(bool available);

  // Updates the content icon and title, calls `UpdateSummaryAndOutlines` and
  // navigates to the summary view.
  void RefreshContents();

  // Retries the operation associated with `origin_state`.
  // If `origin_state` is `VisibilityState::kQuestionAndAnswer`, re-asks the
  // question.
  // If `origin_state` is `VisibilityState::kSummaryAndOutlines`, regenerates
  // the summary & outlines.
  // NOTE: `origin_state` should not be `VisibilityState::kError`.
  void Retry(VisibilityState origin_state);

  // Sends `question` to the backend. `current_panel_content` determines if the
  // `question` is regarding the current content displayed on the panel.
  // `source` indicates where `question` is posted.
  // If `update_summary_after_answer_question` is true, a request to update the
  // summary view will be made when the answer is loaded.
  void SendQuestion(const std::u16string& question,
                    bool current_panel_content,
                    QuestionSource source,
                    bool update_summary_after_answer_question = false);

  // Sends requests to the backend to update summary and outlines.
  // `delegates_` will be notified of the updated summary and outlines when
  // requests are fulfilled.
  void UpdateSummaryAndOutlines();

  // Records histogram that tracks the amount of times the panel was opened
  // during an active session.
  void RecordTimesPanelOpenedMetric();

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  views::Widget* mahi_panel_widget() { return mahi_panel_widget_.get(); }

 private:
  void HandleError(const MahiUiError& error);

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

  // Invalidates pending summary/outline/QA requests on new request to avoid
  // racing.
  void InvalidatePendingRequests();

  // The current state. Use `VisibilityState::kSummaryAndOutlines` by default.
  VisibilityState visibility_state_ = VisibilityState::kSummaryAndOutlines;

  base::ObserverList<Delegate> delegates_;

  views::UniqueWidgetPtr mahi_panel_widget_;

  // Used to record metrics. The count will be increased by one every time the
  // panel is opened, and reset to zero when the metric is recorded, which
  // happens when the session is no longer active or on shutdown.
  int times_panel_opened_per_session_ = 0;

  // Indicates the params of the most recent question.
  // Set when the controller receives a request to send a question.
  // Reset when the content is refreshed.
  std::optional<MahiQuestionParams> most_recent_question_params_;

  // Indicates that we need to update summary after answer is fully loaded.
  bool update_summary_after_answer_question_ = false;

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
