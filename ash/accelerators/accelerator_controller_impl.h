// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_IMPL_H_
#define ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_IMPL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ash/accelerators/accelerator_capslock_state_machine.h"
#include "ash/accelerators/accelerator_history_impl.h"
#include "ash/accelerators/accelerator_launcher_state_machine.h"
#include "ash/accelerators/accelerator_prefs.h"
#include "ash/accelerators/accelerator_shift_disable_capslock_state_machine.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/accelerators/exit_warning_handler.h"
#include "ash/accelerators/suspend_state_machine.h"
#include "ash/accelerators/tablet_volume_controller.h"
#include "ash/accelerators/top_row_key_usage_recorder.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_map.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ui {
class AcceleratorManager;
}

namespace ash {

struct AcceleratorData;
class ExitWarningHandler;
class DebugDelegate;

// AcceleratorControllerImpl provides functions for registering or unregistering
// global keyboard accelerators, which are handled earlier than any windows. It
// also implements several handlers as an accelerator target.
class ASH_EXPORT AcceleratorControllerImpl
    : public ui::AcceleratorTarget,
      public AcceleratorController,
      public input_method::InputMethodManager::Observer,
      public AshAcceleratorConfiguration::Observer,
      public AcceleratorPrefs::Observer {
 public:
  // TestApi is used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(AcceleratorControllerImpl* controller);

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    ~TestApi() = default;

    // If |controller_->tablet_mode_volume_adjust_timer_| is running, stops it,
    // runs its task, and returns true. Otherwise returns false.
    [[nodiscard]] bool TriggerTabletModeVolumeAdjustTimer();

    // Registers the specified accelerators.
    void RegisterAccelerators(base::span<const AcceleratorData> accelerators);

    // Start observing changes made to the ash accelerator list.
    void ObserveAcceleratorUpdates();

    // Returns whether the action for this accelerator is enabled.
    bool IsActionForAcceleratorEnabled(const ui::Accelerator& accelerator);

    // Returns the corresponding accelerator data if |action| maps to a
    // deprecated accelerator, otherwise return nullptr.
    const DeprecatedAcceleratorData* GetDeprecatedAcceleratorData(
        AcceleratorAction action);

    // Accessor to accelerator confirmation dialog.
    AccessibilityConfirmationDialog* GetConfirmationDialog();

    // Provides access to the ExitWarningHandler.
    ExitWarningHandler* GetExitWarningHandler();

    const TabletVolumeController::SideVolumeButtonLocation&
    GetSideVolumeButtonLocation();
    void SetSideVolumeButtonFilePath(base::FilePath path);
    void SetSideVolumeButtonLocation(const std::string& region,
                                     const std::string& side);

    void SetCanHandleLauncher(bool can_handle);
    void SetCanHandleCapsLock(bool can_handle);

   private:
    raw_ptr<AcceleratorControllerImpl, DanglingUntriaged>
        controller_;  // Not owned.
  };

  explicit AcceleratorControllerImpl(AshAcceleratorConfiguration* config);
  AcceleratorControllerImpl(const AcceleratorControllerImpl&) = delete;
  AcceleratorControllerImpl& operator=(const AcceleratorControllerImpl&) =
      delete;
  ~AcceleratorControllerImpl() override;

  // A list of possible ways in which an accelerator should be restricted before
  // processing. Any target registered with this controller should respect
  // restrictions by calling GetAcceleratorProcessingRestriction() during
  // processing.
  enum AcceleratorProcessingRestriction {
    // Process the accelerator normally.
    RESTRICTION_NONE,

    // Don't process the accelerator.
    RESTRICTION_PREVENT_PROCESSING,

    // Don't process the accelerator and prevent propagation to other targets.
    RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION
  };

  // input_method::InputMethodManager::Observer overrides:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // AshAcceleratorConfiguration::Observer overrides:
  void OnAcceleratorsUpdated() override;

  // AcceleratorPrefs::Observer overrides:
  void OnShortcutPolicyUpdated() override;

  // Registers global keyboard accelerators for the specified target. If
  // multiple targets are registered for any given accelerator, a target
  // registered later has higher priority.
  void Register(const std::vector<ui::Accelerator>& accelerators,
                ui::AcceleratorTarget* target);

  // Unregisters the specified keyboard accelerator for the specified target.
  void Unregister(const ui::Accelerator& accelerator,
                  ui::AcceleratorTarget* target);

  // Unregisters all keyboard accelerators for the specified target.
  void UnregisterAll(ui::AcceleratorTarget* target);

  // AcceleratorControllerImpl:
  bool Process(const ui::Accelerator& accelerator) override;
  bool IsDeprecated(const ui::Accelerator& accelerator) const override;
  bool PerformActionIfEnabled(AcceleratorAction action,
                              const ui::Accelerator& accelerator) override;
  bool OnMenuAccelerator(const ui::Accelerator& accelerator) override;
  bool IsRegistered(const ui::Accelerator& accelerator) const override;
  AcceleratorHistoryImpl* GetAcceleratorHistory() override;
  bool DoesAcceleratorMatchAction(const ui::Accelerator& accelerator,
                                  AcceleratorAction action) override;
  void ApplyAcceleratorForTesting(const ui::Accelerator& accelerator) override;

  // Returns true if the |accelerator| is preferred. A preferred accelerator
  // is handled before being passed to an window/web contents, unless
  // the window is in fullscreen state.
  bool IsPreferred(const ui::Accelerator& accelerator) const;

  // Returns true if the |accelerator| is reserved. A reserved accelerator
  // is always handled and will never be passed to an window/web contents.
  bool IsReserved(const ui::Accelerator& accelerator) const;

  // Provides access to the ExitWarningHandler for testing.
  ExitWarningHandler* GetExitWarningHandlerForTest() {
    return &exit_warning_handler_;
  }

  // Overridden from ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // If set to true, all accelerators will not be processed.
  void SetPreventProcessingAccelerators(bool prevent_processing_accelerators);
  bool ShouldPreventProcessingAccelerators() const;

  AshAcceleratorConfiguration* accelerator_configuration() {
    return accelerator_configuration_;
  }

  // Sets |DebugDelegate| which is implemented by lacros-chrome.
  void SetDebugDelegate(DebugDelegate* delegate);

 private:
  // A map for looking up actions from accelerators.
  using AcceleratorActionMap = ui::AcceleratorMap<AcceleratorAction>;

  // Initializes the accelerators this class handles as a target.
  void Init();

  // Registers the specified accelerators.
  void RegisterAccelerators(base::span<const AcceleratorData> accelerators);

  // Registers the specified accelerators from a list of accelerators.
  void RegisterAccelerators(std::vector<ui::Accelerator> accelerators);

  // Returns true if there is an action for |accelerator| and it is enabled.
  bool IsActionForAcceleratorEnabled(const ui::Accelerator& accelerator) const;

  // Returns whether |action| can be performed. The |accelerator| may provide
  // additional data the action needs.
  bool CanPerformAction(AcceleratorAction action,
                        const ui::Accelerator& accelerator) const;

  // Performs the specified action. The |accelerator| may provide additional
  // data the action needs.
  void PerformAction(AcceleratorAction action,
                     const ui::Accelerator& accelerator);

  // Performs |action| on |DebugDelegate|s if registered and debug accelerators
  // are enabled.
  void PerformDebugActionOnDelegateIfEnabled(AcceleratorAction action);

  // Returns whether performing |action| should consume the key event.
  bool ShouldActionConsumeKeyEvent(AcceleratorAction action);

  // Get the accelerator restriction for the given action. Supply an |action|
  // of -1 to get restrictions that apply for the current context.
  AcceleratorProcessingRestriction GetAcceleratorProcessingRestriction(
      int action) const;

  // If |accelerator| is a deprecated accelerator, it performs the appropriate
  // deprecated accelerator pre-handling.
  // Returns PROCEED if the accelerator's action should be performed (i.e. if
  // |accelerator| is not a deprecated accelerator, or it's an enabled
  // deprecated accelerator), and STOP otherwise (if the accelerator is a
  // disabled deprecated accelerator).
  enum class AcceleratorProcessingStatus { PROCEED, STOP };
  AcceleratorProcessingStatus MaybeDeprecatedAcceleratorPressed(
      AcceleratorAction action,
      const ui::Accelerator& accelerator) const;

  // Records when the user changes the output volume via keyboard to metrics.
  void RecordVolumeSource();

  std::unique_ptr<ui::AcceleratorManager> accelerator_manager_;

  // A tracker for the current and previous accelerators.
  std::unique_ptr<AcceleratorHistoryImpl> accelerator_history_;
  std::unique_ptr<AcceleratorLauncherStateMachine> launcher_state_machine_;
  std::unique_ptr<AcceleratorCapslockStateMachine> capslock_state_machine_;
  std::unique_ptr<AcceleratorShiftDisableCapslockStateMachine>
      shift_disable_state_machine_;
  std::unique_ptr<SuspendStateMachine> suspend_state_machine_;

  // Metrics recorders that listen to the input stream to emit metrics.
  std::unique_ptr<TopRowKeyUsageRecorder> top_row_key_usage_recorder_;

  // Manages all accelerator mappings.
  raw_ptr<AshAcceleratorConfiguration> accelerator_configuration_;

  // Handles the exit accelerator which requires a double press to exit and
  // shows a popup with an explanation.
  ExitWarningHandler exit_warning_handler_;

  // Handle the orientation of volume buttons in tablet mode.
  TabletVolumeController tablet_volume_controller_;

  // Actions allowed when the user is not signed in.
  std::set<int> actions_allowed_at_login_screen_;
  // Actions allowed when the screen is locked.
  std::set<int> actions_allowed_at_lock_screen_;
  // Actions allowed when the power menu is opened.
  std::set<int> actions_allowed_at_power_menu_;
  // Actions allowed when a modal window is up.
  std::set<int> actions_allowed_at_modal_window_;
  // Preferred actions. See accelerator_table.h for details.
  std::set<int> preferred_actions_;
  // Reserved actions. See accelerator_table.h for details.
  std::set<int> reserved_actions_;
  // Actions which will be repeated while holding the accelerator key.
  std::set<int> repeatable_actions_;
  // Actions allowed in app mode.
  std::set<int> actions_allowed_in_app_mode_;
  // Actions allowed in pinned mode.
  std::set<int> actions_allowed_in_pinned_mode_;
  // Actions disallowed if there are no windows.
  std::set<int> actions_needing_window_;
  // Actions that can be performed without closing the menu (if one is present).
  std::set<int> actions_keeping_menu_open_;

  // Prevents the processing of all KB shortcuts in the controller.
  bool prevent_processing_accelerators_ = false;

  // Timer used to prevent the input gain from recording each time the user
  // presses a volume key while setting the desired volume.
  base::DelayTimer output_volume_metric_delay_timer_;

  // Please refer to the comment on |DebugInterfaceAsh| for the lifetime of this
  // pointer.
  raw_ptr<DebugDelegate> debug_delegate_ = nullptr;

  std::unique_ptr<InputDeviceSettingsNotificationController>
      notification_controller_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_IMPL_H_
