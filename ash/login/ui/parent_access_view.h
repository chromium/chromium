// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_
#define ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class LabelButton;
class Textfield;
}  // namespace views

namespace ash {
class ArrowButtonView;
class LoginButton;
class LoginPinView;
class NonAccessibleView;

enum class ParentAccessRequestReason;

// The view that allows for input of parent access code to authorize certain
// actions on child's device.
class ASH_EXPORT ParentAccessView : public views::DialogDelegateView,
                                    public views::ButtonListener,
                                    public TabletModeObserver {
 public:
  // ParentAccessView state.
  enum class State {
    kNormal,  // View with default texts and colors.
    kError    // View with texts and color signalizing input error.
  };

  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(ParentAccessView* view);
    ~TestApi();

    LoginButton* back_button();
    views::Label* title_label();
    views::Label* description_label();
    views::View* access_code_view();
    views::LabelButton* help_button();
    ArrowButtonView* submit_button();
    LoginPinView* pin_keyboard_view();

    views::Textfield* GetInputTextField(int index);

    State state() const;

   private:
    ParentAccessView* const view_;
  };

  using OnFinished = base::RepeatingCallback<void(bool access_granted)>;

  // Parent access view callbacks.
  struct Callbacks {
    Callbacks();
    Callbacks(const Callbacks& other);
    ~Callbacks();

    // Called when ParentAccessView finshed processing and should be dismissed.
    // If access code was successfully validated, |access_granted| will
    // contain true. If access code was not entered or not successfully
    // validated and user pressed back button, |access_granted| will contain
    // false.
    OnFinished on_finished;
  };

  // Actions that originated in parent access dialog. These values are persisted
  // to metrics. Entries should not be renumbered and numeric values should
  // never be reused.
  enum class UMAAction {
    kValidationSuccess = 0,
    kValidationError = 1,
    kCanceledByUser = 2,
    kGetHelp = 3,
    kMaxValue = kGetHelp,
  };

  // Context in which parent access code was used. These values are persisted to
  // metrics. Entries should not be reordered and numeric values should never be
  // reused.
  enum class UMAUsage {
    kTimeLimits = 0,
    kTimeChangeLoginScreen = 1,
    kTimeChangeInSession = 2,
    kTimezoneChange = 3,
    kMaxValue = kTimezoneChange,
  };

  // Histogram to log actions that originated in parent access dialog.
  static constexpr char kUMAParentAccessCodeAction[] =
      "Supervision.ParentAccessCode.Action";

  // Histogram to log context in which parent access code was used.
  static constexpr char kUMAParentAccessCodeUsage[] =
      "Supervision.ParentAccessCode.Usage";

  // Creates parent access view that will validate the parent access code for a
  // specific child, when |account_id| is set, or to any child signed in the
  // device, when it is empty. |callbacks| will be called when user performs
  // certain actions. |reason| contains information about why the parent access
  // view is necessary, it is used to modify the view appearance by changing the
  // title and description strings and background color. |validation_time| is
  // the time that will be used to validate the code, if null the system's
  // current time will be used.
  ParentAccessView(const AccountId& account_id,
                   const Callbacks& callbacks,
                   ParentAccessRequestReason reason,
                   base::Time validation_time);
  ~ParentAccessView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void RequestFocus() override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::DialogDelegateView:
  ui::ModalType GetModalType() const override;
  views::View* GetInitiallyFocusedView() override;
  base::string16 GetAccessibleWindowTitle() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

 private:
  class FocusableLabelButton;
  class AccessCodeInput;

  // Submits access code for validation.
  void SubmitCode();

  // Updates state of the view.
  void UpdateState(State state);

  // Updates view's preferred size.
  void UpdatePreferredSize();

  // Moves focus to |submit_button_|.
  void FocusSubmitButton();

  // Called when access code input changes. |complete| brings information
  // whether current input code is complete. |last_field_active| contains
  // information whether last input field is currently active.
  void OnInputChange(bool complete, bool last_field_active);

  // Callbacks to be called when user performs certain actions.
  const Callbacks callbacks_;

  // Account id of the user that parent access code is processed for. When
  // empty, the code is processed for all the children signed in the device.
  const AccountId account_id_;

  // Indicates what action will be authorized with parent access code.
  // The appearance of the view depends on |request_reason_|.
  const ParentAccessRequestReason request_reason_;

  // Time used to validate the code. When this is null, the current system time
  // is used.
  const base::Time validation_time_;

  State state_ = State::kNormal;

  // Auto submit code when the last input has been inserted.
  bool auto_submit_enabled_ = true;

  views::Label* title_label_ = nullptr;
  views::Label* description_label_ = nullptr;
  AccessCodeInput* access_code_view_ = nullptr;
  LoginPinView* pin_keyboard_view_ = nullptr;
  LoginButton* back_button_ = nullptr;
  FocusableLabelButton* help_button_ = nullptr;
  ArrowButtonView* submit_button_ = nullptr;
  NonAccessibleView* pin_keyboard_to_footer_spacer_ = nullptr;

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  base::WeakPtrFactory<ParentAccessView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ParentAccessView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_PARENT_ACCESS_VIEW_H_
