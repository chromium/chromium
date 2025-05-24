// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_SUNFISH_SCANNER_FEATURE_WATCHER_H_
#define ASH_CAPTURE_MODE_SUNFISH_SCANNER_FEATURE_WATCHER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {

class SessionControllerImpl;
class Shell;

// A BEST-EFFORT source of Sunfish and Scanner feature states that observers can
// observe. Not all feature state updates may be sent to observers automatically
// - notably, some Scanner checks are NOT observed by this class, and in tests,
// Sunfish enterprise policies are NOT observed (as they are obtained from
// `TestCaptureModeDelegate`, not from preferences).
//
// Use `UpdateFeatureStates` to update the state of the feature states when
// accuracy is required. If this class is used, it should always be used as the
// source of truth to ensure that a consistent view of the feature states is
// seen.
class ASH_EXPORT SunfishScannerFeatureWatcher : public SessionObserver,
                                                public ShellObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever `CanShowSunfishUi()` or `CanShowScannerUi()` changes.
    //
    // This MUST NOT delete `source`.
    // If this call synchronously updates Sunfish / Scanner feature state, this
    // may be re-entrantly called again.
    virtual void OnSunfishScannerFeatureStatesChanged(
        SunfishScannerFeatureWatcher& source) = 0;
  };

  // `session_controller` and `shell` must outlive this class.
  explicit SunfishScannerFeatureWatcher(
      SessionControllerImpl& session_controller,
      Shell& shell);
  ~SunfishScannerFeatureWatcher() override;

  SunfishScannerFeatureWatcher(const SunfishScannerFeatureWatcher&) = delete;
  SunfishScannerFeatureWatcher& operator=(const SunfishScannerFeatureWatcher&) =
      delete;
  SunfishScannerFeatureWatcher(SunfishScannerFeatureWatcher&&) = delete;
  SunfishScannerFeatureWatcher& operator=(SunfishScannerFeatureWatcher&&) =
      delete;

  // Adds an observer. Does not automatically call the observer.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

  // Returns whether Sunfish-related UI can be shown.
  // This reflects the state of `ash::CanShowSunfishUi()` when
  // `UpdateFeatureStates()` was last called, and may not reflect the current
  // state.
  bool CanShowSunfishUi() const { return can_show_sunfish_ui_; }

  // Returns whether Scanner-related UI can be shown.
  // This reflects the state of `ash::ScannerController::CanShowUiForShell()`
  // when `UpdateFeatureStates()` was last called, and may not reflect the
  // current state.
  bool CanShowScannerUi() const { return can_show_scanner_ui_; }

  // Returns whether Scanner-related UI or Scanner-related UI can be shown.
  // This reflects the state of `ash::CanShowSunfishOrScannerUi()` when
  // `UpdateFeatureStates()` was last called, and may not reflect the current
  // state.
  bool CanShowSunfishOrScannerUi() const {
    return can_show_sunfish_ui_ || can_show_scanner_ui_;
  }

  // Updates `sunfish_ui_shown` and `scanner_ui_shown`, and synchronously call
  // observers if the feature states have changed.
  //
  // Manually call this method if accuracy is required. This should not call
  // observers in most cases.
  void UpdateFeatureStates();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // ShellObserver:
  void OnPinnedStateChanged(aura::Window* pinned_window) override;

 private:
  bool can_show_sunfish_ui_;
  bool can_show_scanner_ui_;

  base::ObserverList<Observer> observers_;

  // Observations:
  PrefChangeRegistrar pref_change_registrar_;
  base::ScopedObservation<SessionControllerImpl, SessionObserver>
      session_controller_observation_{this};
  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};

  base::WeakPtrFactory<SunfishScannerFeatureWatcher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_SUNFISH_SCANNER_FEATURE_WATCHER_H_
