// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_CONTROLLER_H_
#define ASH_SCANNER_SCANNER_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "ash/scanner/scanner_session.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

class PrefRegistrySimple;

namespace manta::proto {
class ScannerAction;
}

namespace ash {

class ScannerCommandDelegateImpl;
class ScannerDelegate;
class ScreenPinningController;
class SessionControllerImpl;

// This is the top level controller used for Scanner. It acts as a mediator
// between Scanner and any consuming features.
class ASH_EXPORT ScannerController : public SessionObserver {
 public:
  using OnActionFinishedCallback = base::OnceCallback<void(bool success)>;

  // `screen_pinning_controller` must outlive this class.
  // It must only be null in tests.
  explicit ScannerController(
      std::unique_ptr<ScannerDelegate> delegate,
      SessionControllerImpl& session_controller,
      const ScreenPinningController* screen_pinning_controller);
  ScannerController(const ScannerController&) = delete;
  ScannerController& operator=(const ScannerController&) = delete;
  ~ScannerController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns whether Scanner-related UI can be shown. This function checks
  // `CanShowUi` for the `Shell`-global `ScannerController`.
  //
  // Do NOT use this method if your feature is using
  // `SunfishScannerFeatureWatcher`, use its `CanShowScannerUi` method instead.
  static bool CanShowUiForShell();

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Checks system level constraints (e.g. feature flags) and returns
  // true if the constraints allow the scanner UI to show.
  // Note that this ignores consent status.
  bool CanShowUi();

  // Checks system level constraints (e.g. feature flags) and returns
  // true if the constraints allow a Scanner settings toggle to be shown.
  bool CanShowFeatureSettingsToggle();

  // Checks system level constraints (e.g. prefs, feature flags) and returns
  // true if the constraints allow a Scanner session to be created.
  bool CanStartSession();

  // Creates a new ScannerSession and returns a pointer to the created session.
  // If the Scanner cannot be initialized due to system level constraints (e.g.
  // pref disabled, feature not allowed), then no session is created and
  // `nullptr` is returned instead. Note that calling `StartNewSession` will end
  // the current session if there is one.
  ScannerSession* StartNewSession();

  // Fetches Scanner actions that are available based on the current
  // `scanner_session_` and the contents of `jpeg_bytes`, and returns whether a
  // session was active. The actions are returned via `callback`. If no session
  // is active, then `callback` will be run with an empty list of actions.
  bool FetchActionsForImage(scoped_refptr<base::RefCountedMemory> jpeg_bytes,
                            ScannerSession::FetchActionsCallback callback);

  // Should be called when the user has finished interacting with a Scanner
  // session. This will trigger relevant cleanup and eventually destroy the
  // scanner session.
  void OnSessionUIClosed();

  // Executes the action described by `scanner_action`.
  void ExecuteAction(const ScannerActionViewModel& scanner_action);

  // Opens a feedback dialog for an action that has been performed, and the
  // (resized) screenshot which initiated the action.
  // WARNING: This function does not check whether the account has feedback
  // enabled or not!
  void OpenFeedbackDialog(const AccountId& account_id,
                          manta::proto::ScannerAction action,
                          scoped_refptr<base::RefCountedMemory> screenshot);

  // Sets mock ScannerOutput data for testing.
  void SetScannerResponsesForTesting(const std::vector<std::string> responses);

  bool HasActiveSessionForTesting() const;

  ScannerDelegate* delegate_for_testing() { return delegate_.get(); }

  void SetOnActionFinishedForTesting(OnActionFinishedCallback callback);

 private:
  // Should be called when an action finishes execution.
  void OnActionFinished(
      manta::proto::ScannerAction::ActionCase action_case,
      scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes,
      manta::proto::ScannerAction populated_action,
      bool success);

  std::unique_ptr<ScannerDelegate> delegate_;

  // Delegate to handle Scanner commands for actions fetched during a session.
  // `command_delegate_` should outlive `scanner_session_`, to allow commands to
  // be completed in the background after the session UI has been closed.
  std::unique_ptr<ScannerCommandDelegateImpl> command_delegate_;

  // May hold an active Scanner session, to allow access to the Scanner feature.
  std::unique_ptr<ScannerSession> scanner_session_;

  OnActionFinishedCallback on_action_finished_for_testing_;

  // External dependencies not owned by this class:
  // Session controller, stored in `Shell`. Always outlives this class.
  raw_ref<SessionControllerImpl> session_controller_;
  // Screen pinning controller, stored in `Shell`. Always outlives this class.
  // If this pointer is null, then we are in a test.
  const raw_ptr<const ScreenPinningController> screen_pinning_controller_;

  // Holds mock ScannerOutput data for testing.
  std::vector<std::string> mock_scanner_responses_for_testing_;

  ScopedSessionObserver session_observer_{this};

  base::WeakPtrFactory<ScannerController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_CONTROLLER_H_
