// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_IMPL_H_
#define ASH_ACCELERATORS_ACCELERATOR_CONTROLLER_IMPL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "ash/accelerators/accelerator_confirmation_dialog.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accelerators/exit_warning_handler.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_history.h"

namespace ui {
class AcceleratorManager;
}

namespace ash {

struct AcceleratorData;
class ExitWarningHandler;

// See TabletModeVolumeAdjustType at tools/metrics/histograms/enums.xml.
enum class TabletModeVolumeAdjustType {
  kAccidentalAdjustWithSwapEnabled = 0,
  kNormalAdjustWithSwapEnabled = 1,
  kAccidentalAdjustWithSwapDisabled = 2,
  kNormalAdjustWithSwapDisabled = 3,
  kMaxValue = kNormalAdjustWithSwapDisabled,
};

// Histogram for volume adjustment in tablet mode.
ASH_EXPORT extern const char kTabletCountOfVolumeAdjustType[];

// Identifiers for toggling accelerator notifications.
ASH_EXPORT extern const char kHighContrastToggleAccelNotificationId[];
ASH_EXPORT extern const char kDockedMagnifierToggleAccelNotificationId[];
ASH_EXPORT extern const char kFullscreenMagnifierToggleAccelNotificationId[];

// AcceleratorControllerImpl provides functions for registering or unregistering
// global keyboard accelerators, which are handled earlier than any windows. It
// also implements several handlers as an accelerator target.
class ASH_EXPORT AcceleratorControllerImpl : public ui::AcceleratorTarget,
                                             public AcceleratorController {
 public:
  // Some Chrome OS devices have volume up and volume down buttons on their
  // side. We want the button that's closer to the top/right to increase the
  // volume and the button that's closer to the bottom/left to decrease the
  // volume, so we use the buttons' location and the device orientation to
  // determine whether the buttons should be swapped.
  struct SideVolumeButtonLocation {
    // The button can be at the side of the keyboard or the display. Then value
    // of the region could be kVolumeButtonRegionKeyboard or
    // kVolumeButtonRegionScreen.
    std::string region;
    // Side info of region. The value could be kVolumeButtonSideLeft,
    // kVolumeButtonSideRight, kVolumeButtonSideTop or kVolumeButtonSideBottom.
    std::string side;
  };

  // TestApi is used for tests to get internal implementation details.
  class TestApi {
   public:
    explicit TestApi(AcceleratorControllerImpl* controller);
    ~TestApi() = default;

    // If |controller_->tablet_mode_volume_adjust_timer_| is running, stops it,
    // runs its task, and returns true. Otherwise returns false.
    bool TriggerTabletModeVolumeAdjustTimer() WARN_UNUSED_RESULT;

    // Registers the specified accelerators.
    void RegisterAccelerators(const AcceleratorData accelerators[],
                              size_t accelerators_length);

    // Returns the corresponding accelerator data if |action| maps to a
    // deprecated accelerator, otherwise return nullptr.
    const DeprecatedAcceleratorData* GetDeprecatedAcceleratorData(
        AcceleratorAction action);

    // Accessor to accelerator confirmation dialog.
    AcceleratorConfirmationDialog* GetConfirmationDialog();

    AcceleratorControllerImpl::SideVolumeButtonLocation
    side_volume_button_location() {
      return controller_->side_volume_button_location_;
    }
    void SetSideVolumeButtonFilePath(base::FilePath path);
    void SetSideVolumeButtonLocation(const std::string& region,
                                     const std::string& side);

   private:
    AcceleratorControllerImpl* controller_;  // Not owned.

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Fields of the side volume button location info.
  static constexpr const char* kVolumeButtonRegion = "region";
  static constexpr const char* kVolumeButtonSide = "side";

  // Values of kVolumeButtonRegion.
  static constexpr const char* kVolumeButtonRegionKeyboard = "keyboard";
  static constexpr const char* kVolumeButtonRegionScreen = "screen";
  // Values of kVolumeButtonSide.
  static constexpr const char* kVolumeButtonSideLeft = "left";
  static constexpr const char* kVolumeButtonSideRight = "right";
  static constexpr const char* kVolumeButtonSideTop = "top";
  static constexpr const char* kVolumeButtonSideBottom = "bottom";

  AcceleratorControllerImpl();
  ~AcceleratorControllerImpl() override;

  // A list of possible ways in which an accelerator should be restricted before
  // processing. Any target registered with this controller should respect
  // restrictions by calling |GetCurrentAcceleratorRestriction| during
  // processing.
  enum AcceleratorProcessingRestriction {
    // Process the accelerator normally.
    RESTRICTION_NONE,

    // Don't process the accelerator.
    RESTRICTION_PREVENT_PROCESSING,

    // Don't process the accelerator and prevent propagation to other targets.
    RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION
  };

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

  // Returns true if there is an action for |accelerator| and it is enabled.
  bool IsActionForAcceleratorEnabled(const ui::Accelerator& accelerator) const;

  // AcceleratorControllerImpl:
  bool Process(const ui::Accelerator& accelerator) override;
  bool IsDeprecated(const ui::Accelerator& accelerator) const override;
  bool PerformActionIfEnabled(AcceleratorAction action,
                              const ui::Accelerator& accelerator) override;
  bool OnMenuAccelerator(const ui::Accelerator& accelerator) override;
  bool IsRegistered(const ui::Accelerator& accelerator) const override;
  ui::AcceleratorHistory* GetAcceleratorHistory() override;

  // Returns true if the |accelerator| is preferred. A preferred accelerator
  // is handled before being passed to an window/web contents, unless
  // the window is in fullscreen state.
  bool IsPreferred(const ui::Accelerator& accelerator) const;

  // Returns true if the |accelerator| is reserved. A reserved accelerator
  // is always handled and will never be passed to an window/web contents.
  bool IsReserved(const ui::Accelerator& accelerator) const;

  // Returns the restriction for the current context.
  AcceleratorProcessingRestriction GetCurrentAcceleratorRestriction();

  // Provides access to the ExitWarningHandler for testing.
  ExitWarningHandler* GetExitWarningHandlerForTest() {
    return &exit_warning_handler_;
  }

  ui::AcceleratorHistory* accelerator_history() {
    return accelerator_history_.get();
  }

  // Overridden from ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // A confirmation dialog will be shown the first time an accessibility feature
  // is enabled using the specified accelerator key sequence. Only one dialog
  // will be shown at a time, and will not be shown again if the user has
  // selected "accept" on a given dialog. The dialog was added to ensure that
  // users would be aware of the shortcut they have just enabled, and to prevent
  // users from accidentally triggering the feature. The dialog is currently
  // shown when enabling the following features: high contrast, full screen
  // magnifier, docked magnifier and screen rotation. The shown dialog is stored
  // as a weak pointer in the variable |confirmation_dialog_| below.
  void MaybeShowConfirmationDialog(int window_title_text_id,
                                   int dialog_text_id,
                                   base::OnceClosure on_accept_callback,
                                   base::OnceClosure on_cancel_callback);

  // Read the side volume button location info from local file under
  // kSideVolumeButtonLocationFilePath, parse and write it into
  // |side_volume_button_location_|.
  void ParseSideVolumeButtonLocationInfo();

 private:
  // Initializes the accelerators this class handles as a target.
  void Init();

  // Registers the specified accelerators.
  void RegisterAccelerators(const AcceleratorData accelerators[],
                            size_t accelerators_length);

  // Registers the deprecated accelerators and their replacing new ones.
  void RegisterDeprecatedAccelerators();

  // Returns whether |action| can be performed. The |accelerator| may provide
  // additional data the action needs.
  bool CanPerformAction(AcceleratorAction action,
                        const ui::Accelerator& accelerator) const;

  // Performs the specified action. The |accelerator| may provide additional
  // data the action needs.
  void PerformAction(AcceleratorAction action,
                     const ui::Accelerator& accelerator);

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

  // Returns true if |source_device_id| corresponds to the internal keyboard or
  // an internal uncategorized input device.
  bool IsInternalKeyboardOrUncategorizedDevice(int source_device_id) const;

  // Returns true if |side_volume_button_location_| is in agreed format and
  // values.
  bool IsValidSideVolumeButtonLocation() const;

  // Returns true if the side volume buttons should be swapped. See
  // SideVolumeButonLocation for the details.
  bool ShouldSwapSideVolumeButtons(int source_device_id) const;

  // The metrics recorded include accidental volume adjustments (defined as a
  // sequence of volume button events in close succession starting with a
  // volume-up event but ending with an overall-decreased volume, or vice versa)
  // or normal volume adjustments w/o SwapSideVolumeButtonsForOrientation
  // feature enabled.
  void UpdateTabletModeVolumeAdjustHistogram();

  // Starts |tablet_mode_volume_adjust_timer_| while see VOLUME_UP or
  // VOLUME_DOWN acceleration action when in tablet mode.
  void StartTabletModeVolumeAdjustTimer(AcceleratorAction action);

  std::unique_ptr<ui::AcceleratorManager> accelerator_manager_;

  // A tracker for the current and previous accelerators.
  std::unique_ptr<ui::AcceleratorHistory> accelerator_history_;

  // Handles the exit accelerator which requires a double press to exit and
  // shows a popup with an explanation.
  ExitWarningHandler exit_warning_handler_;

  // A map from accelerators to the AcceleratorAction values, which are used in
  // the implementation.
  std::map<ui::Accelerator, AcceleratorAction> accelerators_;

  std::map<AcceleratorAction, const DeprecatedAcceleratorData*>
      actions_with_deprecations_;
  std::set<ui::Accelerator> deprecated_accelerators_;

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

  // Holds a weak pointer to the accelerator confirmation dialog.
  base::WeakPtr<AcceleratorConfirmationDialog> confirmation_dialog_;

  // Path of the file that contains the side volume button location info. It
  // should always be kSideVolumeButtonLocationFilePath. But it is allowed to be
  // set to different paths in test.
  base::FilePath side_volume_button_location_file_path_;

  // Stores the location info of side volume buttons.
  SideVolumeButtonLocation side_volume_button_location_;

  // Started when VOLUME_DOWN or VOLUME_UP accelerator action is seen while in
  // tablet mode. Runs UpdateTabletModeVolumeAdjustHistogram() to record
  // metrics.
  base::OneShotTimer tablet_mode_volume_adjust_timer_;

  // True if volume adjust starts with VOLUME_UP action.
  bool volume_adjust_starts_with_up_ = false;

  // The initial volume percentage when volume adjust starts.
  int initial_volume_percent_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerImpl);
};

}  // namespace ash

#endif  // ASH_ACCELERATOR_CONTROLLER_IMPL_H_
