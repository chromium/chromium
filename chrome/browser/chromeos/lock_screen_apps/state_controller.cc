// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"

#include <utility>

#include "ash/public/cpp/stylus_utils.h"
#include "ash/public/mojom/constants.mojom.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager_impl.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_window_metrics_tracker.h"
#include "chrome/browser/chromeos/lock_screen_apps/first_app_run_toast_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/focus_cycler_delegate.h"
#include "chrome/browser/chromeos/lock_screen_apps/lock_screen_profile_creator_impl.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_contents.h"
#include "crypto/symmetric_key.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/wm/core/window_animations.h"

using ash::mojom::CloseLockScreenNoteReason;
using ash::mojom::LockScreenNoteOrigin;
using ash::mojom::TrayActionState;

namespace lock_screen_apps {

namespace {

// Key for user pref that contains the 256 bit AES key that should be used to
// encrypt persisted user data created on the lock screen.
constexpr char kDataCryptoKeyPref[] = "lockScreenAppDataCryptoKey";

StateController* g_state_controller_instance = nullptr;

// Generates a random 256 bit AES key. Returns an empty string on error.
std::string GenerateCryptoKey() {
  std::unique_ptr<crypto::SymmetricKey> symmetric_key =
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES, 256);
  if (!symmetric_key)
    return "";
  return symmetric_key->key();
}

}  // namespace

// static
StateController* StateController::Get() {
  DCHECK(g_state_controller_instance);
  return g_state_controller_instance;
}

// static
void StateController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kDataCryptoKeyPref, "");
}

StateController::StateController() {
  DCHECK(!g_state_controller_instance);

  g_state_controller_instance = this;
}

StateController::~StateController() {
  DCHECK_EQ(g_state_controller_instance, this);
  g_state_controller_instance = nullptr;
}

void StateController::SetTrayActionForTesting(
    mojo::PendingRemote<ash::mojom::TrayAction> tray_action) {
  tray_action_.Bind(std::move(tray_action));
}

void StateController::FlushTrayActionForTesting() {
  tray_action_.FlushForTesting();
}

void StateController::SetReadyCallbackForTesting(
    const base::Closure& ready_callback) {
  DCHECK(ready_callback_.is_null());

  ready_callback_ = ready_callback;
}

void StateController::SetTickClockForTesting(const base::TickClock* clock) {
  DCHECK(!tick_clock_);
  tick_clock_ = clock;
}

void StateController::SetAppManagerForTesting(
    std::unique_ptr<AppManager> app_manager) {
  DCHECK(!app_manager_);
  app_manager_ = std::move(app_manager);
}

void StateController::SetLockScreenLockScreenProfileCreatorForTesting(
    std::unique_ptr<LockScreenProfileCreator> profile_creator) {
  DCHECK(!lock_screen_profile_creator_);
  lock_screen_profile_creator_ = std::move(profile_creator);
}

void StateController::Initialize() {
  if (!tick_clock_)
    tick_clock_ = base::DefaultTickClock::GetInstance();

  // The tray action ptr might be set previously if the client was being created
  // for testing.
  if (!tray_action_) {
    content::GetSystemConnector()->Connect(
        ash::mojom::kServiceName, tray_action_.BindNewPipeAndPassReceiver());
  }
  mojo::PendingRemote<ash::mojom::TrayActionClient> client;
  receiver_.Bind(client.InitWithNewPipeAndPassReceiver());
  tray_action_->SetClient(std::move(client), lock_screen_note_state_);
}

void StateController::SetPrimaryProfile(Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || !user->HasGaiaAccount()) {
    if (!ready_callback_.is_null())
      std::move(ready_callback_).Run();
    return;
  }

  std::string key;
  if (!GetUserCryptoKey(profile, &key)) {
    LOG(ERROR) << "Failed to get crypto key for user lock screen apps.";
    return;
  }

  InitializeWithCryptoKey(profile, key);
}

void StateController::Shutdown() {
  session_observer_.RemoveAll();
  lock_screen_data_.reset();
  if (app_manager_) {
    app_manager_->Stop();
    ResetNoteTakingWindowAndMoveToNextState(
        true /*close_window*/, CloseLockScreenNoteReason::kShutdown);
    app_manager_.reset();
  }
  first_app_run_toast_manager_.reset();
  lock_screen_profile_creator_.reset();
  focus_cycler_delegate_ = nullptr;
  power_manager_client_observer_.RemoveAll();
  input_devices_observer_.RemoveAll();
  receiver_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool StateController::GetUserCryptoKey(Profile* profile, std::string* key) {
  *key = profile->GetPrefs()->GetString(kDataCryptoKeyPref);
  if (!key->empty() && base::Base64Decode(*key, key))
    return true;

  *key = GenerateCryptoKey();

  if (key->empty())
    return false;

  std::string base64_encoded_key;
  base::Base64Encode(*key, &base64_encoded_key);

  profile->GetPrefs()->SetString(kDataCryptoKeyPref, base64_encoded_key);
  return true;
}

void StateController::InitializeWithCryptoKey(Profile* profile,
                                              const std::string& crypto_key) {
  base::FilePath base_path;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &base_path)) {
    LOG(ERROR) << "Failed to get base storage dir for lock screen app data.";
    return;
  }

  lock_screen_data_ =
      std::make_unique<extensions::lock_screen_data::LockScreenItemStorage>(
          profile, g_browser_process->local_state(), crypto_key,
          base_path.AppendASCII("lock_screen_app_data"),
          base_path.AppendASCII("lock_screen_app_data_v2"));
  lock_screen_data_->SetSessionLocked(false);

  chromeos::NoteTakingHelper::Get()->SetProfileWithEnabledLockScreenApps(
      profile);

  // Lock screen profile creator might have been set by a test.
  if (!lock_screen_profile_creator_) {
    lock_screen_profile_creator_ =
        std::make_unique<LockScreenProfileCreatorImpl>(profile, tick_clock_);
  }
  lock_screen_profile_creator_->Initialize();

  // App manager might have been set previously by a test.
  if (!app_manager_)
    app_manager_ = std::make_unique<AppManagerImpl>(tick_clock_);
  app_manager_->Initialize(profile, lock_screen_profile_creator_.get());

  first_app_run_toast_manager_ =
      std::make_unique<FirstAppRunToastManager>(profile);

  input_devices_observer_.Add(ui::DeviceDataManager::GetInstance());

  // Do not start state controller if stylus input is not present as lock
  // screen notes apps are geared towards stylus.
  // State controller will observe inpt device changes and continue
  // initialization if stylus input is found.
  if (!ash::stylus_utils::HasStylusInput()) {
    stylus_input_missing_ = true;

    if (!ready_callback_.is_null())
      std::move(ready_callback_).Run();
    return;
  }

  InitializeWithStylusInputPresent();
}

void StateController::InitializeWithStylusInputPresent() {
  stylus_input_missing_ = false;

  power_manager_client_observer_.Add(chromeos::PowerManagerClient::Get());
  session_observer_.Add(session_manager::SessionManager::Get());
  OnSessionStateChanged();

  // SessionController is fully initialized at this point.
  if (!ready_callback_.is_null())
    std::move(ready_callback_).Run();
}

void StateController::AddObserver(StateObserver* observer) {
  observers_.AddObserver(observer);
}

void StateController::RemoveObserver(StateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void StateController::SetFocusCyclerDelegate(FocusCyclerDelegate* delegate) {
  DCHECK(!focus_cycler_delegate_ || !delegate);

  if (focus_cycler_delegate_ && note_app_window_)
    focus_cycler_delegate_->UnregisterLockScreenAppFocusHandler();

  focus_cycler_delegate_ = delegate;

  if (focus_cycler_delegate_ && note_app_window_) {
    focus_cycler_delegate_->RegisterLockScreenAppFocusHandler(base::Bind(
        &StateController::FocusAppWindow, weak_ptr_factory_.GetWeakPtr()));
  }
}

TrayActionState StateController::GetLockScreenNoteState() const {
  return lock_screen_note_state_;
}

void StateController::RequestNewLockScreenNote(LockScreenNoteOrigin origin) {
  if (lock_screen_note_state_ != TrayActionState::kAvailable)
    return;

  DCHECK(app_manager_->IsNoteTakingAppAvailable());

  UMA_HISTOGRAM_ENUMERATION("Apps.LockScreen.NoteTakingApp.LaunchRequestReason",
                            origin);

  // Update state to launching even if app fails to launch - this is to notify
  // listeners that a lock screen note request was handled.
  UpdateLockScreenNoteState(TrayActionState::kLaunching);

  if (!app_manager_->LaunchNoteTaking()) {
    UpdateLockScreenNoteState(TrayActionState::kAvailable);
    return;
  }

  note_app_window_metrics_->AppLaunchRequested();
}

void StateController::CloseLockScreenNote(CloseLockScreenNoteReason reason) {
  ResetNoteTakingWindowAndMoveToNextState(true /*close_window*/, reason);
}

void StateController::OnSessionStateChanged() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    lock_screen_data_->SetSessionLocked(false);
    app_manager_->Stop();
    ResetNoteTakingWindowAndMoveToNextState(
        true /*close_window*/, CloseLockScreenNoteReason::kSessionUnlock);
    note_app_window_metrics_.reset();
    return;
  }

  // base::Unretained is safe here because |app_manager_| is owned by |this|,
  // and the callback will not be invoked after |app_manager_| goes out of
  // scope.
  app_manager_->Start(
      base::Bind(&StateController::OnNoteTakingAvailabilityChanged,
                 base::Unretained(this)));
  note_app_window_metrics_ =
      std::make_unique<AppWindowMetricsTracker>(tick_clock_);
  lock_screen_data_->SetSessionLocked(true);
  OnNoteTakingAvailabilityChanged();
}

void StateController::OnWindowVisibilityChanged(aura::Window* window,
                                                bool visible) {
  if (lock_screen_note_state_ != TrayActionState::kLaunching)
    return;

  if (window != note_app_window_->GetNativeWindow() || !window->IsVisible())
    return;

  note_window_observer_.Remove(window);

  UpdateLockScreenNoteState(TrayActionState::kActive);
  if (focus_cycler_delegate_) {
    focus_cycler_delegate_->RegisterLockScreenAppFocusHandler(base::Bind(
        &StateController::FocusAppWindow, weak_ptr_factory_.GetWeakPtr()));
  }
}

void StateController::OnWindowDestroying(aura::Window* window) {
  if (window != note_app_window_->GetNativeWindow())
    return;
  ResetNoteTakingWindowAndMoveToNextState(
      false /*close_window*/, CloseLockScreenNoteReason::kAppWindowClosed);
}

void StateController::OnAppWindowAdded(extensions::AppWindow* app_window) {
  if (note_app_window_ != app_window)
    return;
  note_window_observer_.Add(note_app_window_->GetNativeWindow());
  first_app_run_toast_manager_->RunForAppWindow(note_app_window_);
  note_app_window_metrics_->AppWindowCreated(app_window);
}

void StateController::OnAppWindowRemoved(extensions::AppWindow* app_window) {
  if (note_app_window_ != app_window)
    return;
  ResetNoteTakingWindowAndMoveToNextState(
      false /*close_window*/, CloseLockScreenNoteReason::kAppWindowClosed);
}

void StateController::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if ((input_device_types & ui::InputDeviceEventObserver::kTouchscreen) &&
      stylus_input_missing_ && ash::stylus_utils::HasStylusInput()) {
    InitializeWithStylusInputPresent();
  }
}

void StateController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  ResetNoteTakingWindowAndMoveToNextState(true /*close_window*/,
                                          CloseLockScreenNoteReason::kSuspend);
}

extensions::AppWindow* StateController::CreateAppWindowForLockScreenAction(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    extensions::api::app_runtime::ActionType action,
    std::unique_ptr<extensions::AppDelegate> app_delegate) {
  if (action != extensions::api::app_runtime::ACTION_TYPE_NEW_NOTE)
    return nullptr;

  if (note_app_window_)
    return nullptr;

  if (lock_screen_note_state_ != TrayActionState::kLaunching)
    return nullptr;

  // StateController should not be able to get into kLaunching state if the
  // lock screen profile has not been loaded, and |lock_screen_profile_creator_|
  // has |lock_screen_profile| set to null - if the lock screen profile is not
  // loaded, |app_manager_| should not report that note taking app is available,
  // so state controller should not allow note launch attempt.
  // Thus, it should be safe to assume lock screen profile is set at this point.
  DCHECK(lock_screen_profile_creator_->lock_screen_profile());

  if (!lock_screen_profile_creator_->lock_screen_profile()->IsSameProfile(
          Profile::FromBrowserContext(context))) {
    return nullptr;
  }

  if (!extension || app_manager_->GetNoteTakingAppId() != extension->id())
    return nullptr;

  // The ownership of the window is passed to the caller of this method.
  note_app_window_ =
      new extensions::AppWindow(context, app_delegate.release(), extension);
  app_window_observer_.Add(extensions::AppWindowRegistry::Get(
      lock_screen_profile_creator_->lock_screen_profile()));
  return note_app_window_;
}

bool StateController::HandleTakeFocus(content::WebContents* web_contents,
                                      bool reverse) {
  if (!focus_cycler_delegate_ ||
      GetLockScreenNoteState() != TrayActionState::kActive ||
      note_app_window_->web_contents() != web_contents) {
    return false;
  }

  focus_cycler_delegate_->HandleLockScreenAppFocusOut(reverse);
  return true;
}

void StateController::OnNoteTakingAvailabilityChanged() {
  if (!app_manager_->IsNoteTakingAppAvailable() ||
      (note_app_window_ && note_app_window_->GetExtension()->id() !=
                               app_manager_->GetNoteTakingAppId())) {
    ResetNoteTakingWindowAndMoveToNextState(
        true /*close_window*/,
        CloseLockScreenNoteReason::kAppLockScreenSupportDisabled);
    return;
  }

  if (GetLockScreenNoteState() == TrayActionState::kNotAvailable)
    UpdateLockScreenNoteState(TrayActionState::kAvailable);
}

void StateController::FocusAppWindow(bool reverse) {
  // If the app window is not active, pass the focus on to the delegate..
  if (GetLockScreenNoteState() != TrayActionState::kActive) {
    focus_cycler_delegate_->HandleLockScreenAppFocusOut(reverse);
    return;
  }

  note_app_window_->web_contents()->FocusThroughTabTraversal(reverse);
  note_app_window_->GetBaseWindow()->Activate();
  note_app_window_->web_contents()->Focus();
}

void StateController::ResetNoteTakingWindowAndMoveToNextState(
    bool close_window,
    CloseLockScreenNoteReason reason) {
  note_window_observer_.RemoveAll();
  app_window_observer_.RemoveAll();
  if (first_app_run_toast_manager_)
    first_app_run_toast_manager_->Reset();

  if (note_app_window_metrics_)
    note_app_window_metrics_->Reset();

  if (lock_screen_note_state_ != TrayActionState::kAvailable &&
      lock_screen_note_state_ != TrayActionState::kNotAvailable) {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.LockScreen.NoteTakingApp.NoteTakingExitReason", reason);
  }

  if (focus_cycler_delegate_ &&
      lock_screen_note_state_ == TrayActionState::kActive) {
    focus_cycler_delegate_->UnregisterLockScreenAppFocusHandler();
  }

  if (note_app_window_) {
    if (close_window && note_app_window_->GetBaseWindow()) {
      // Whenever we close the window we want to immediately hide it without
      // animating, as the underlying UI implements a special animation. If we
      // also animate the window the animations will conflict.
      ::wm::SetWindowVisibilityAnimationTransition(
          note_app_window_->GetNativeWindow(), ::wm::ANIMATE_NONE);
      note_app_window_->GetBaseWindow()->Close();
    }
    note_app_window_ = nullptr;
  }

  UpdateLockScreenNoteState(app_manager_ &&
                                    app_manager_->IsNoteTakingAppAvailable()
                                ? TrayActionState::kAvailable
                                : TrayActionState::kNotAvailable);
}

bool StateController::UpdateLockScreenNoteState(TrayActionState state) {
  const TrayActionState old_state = GetLockScreenNoteState();
  if (old_state == state)
    return false;

  lock_screen_note_state_ = state;
  NotifyLockScreenNoteStateChanged();
  return true;
}

void StateController::NotifyLockScreenNoteStateChanged() {
  for (auto& observer : observers_)
    observer.OnLockScreenNoteStateChanged(lock_screen_note_state_);

  tray_action_->UpdateLockScreenNoteState(lock_screen_note_state_);
}

}  // namespace lock_screen_apps
