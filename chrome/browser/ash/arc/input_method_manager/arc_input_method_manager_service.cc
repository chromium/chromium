// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_service.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/keyboard/arc/arc_input_method_bounds_tracker.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_bridge_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/mojom/ime_mojom_traits.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/input_method_observer.h"

namespace arc {

namespace {

// The Android IME id of the pre-installed IME to proxy Chrome OS IME's actions
// to inside the container.
// Please refer to ArcImeService for the implementation details.
constexpr char kChromeOSIMEIdInArcContainer[] =
    "org.chromium.arc.ime/.ArcInputMethodService";

// The name of the proxy IME extension that is used when registering ARC IMEs to
// InputMethodManager.
constexpr char kArcIMEProxyExtensionName[] =
    "org.chromium.arc.inputmethod.proxy";

void SwitchImeToCallback(const std::string& ime_id,
                         const std::string& component_id,
                         bool success) {
  if (success)
    return;

  // TODO(yhanana): We should prevent InputMethodManager from changing current
  // input method until this callback is called with true and once it's done the
  // IME switching code below can be removed.
  LOG(ERROR) << "Switch the active IME to \"" << ime_id << "\"(component_id=\""
             << component_id << "\") failed";
  auto* imm = chromeos::input_method::InputMethodManager::Get();
  if (imm && imm->GetActiveIMEState()) {
    for (const auto& id : imm->GetActiveIMEState()->GetActiveInputMethodIds()) {
      if (!chromeos::extension_ime_util::IsArcIME(id)) {
        imm->GetActiveIMEState()->ChangeInputMethod(id,
                                                    false /* show_message */);
        return;
      }
    }
  }
  NOTREACHED() << "There is no enabled non-ARC IME.";
}

void SetKeyboardDisabled(bool disabled) {
  if (disabled) {
    ChromeKeyboardControllerClient::Get()->SetEnableFlag(
        keyboard::KeyboardEnableFlag::kAndroidDisabled);
  } else {
    ChromeKeyboardControllerClient::Get()->ClearEnableFlag(
        keyboard::KeyboardEnableFlag::kAndroidDisabled);
  }
}

// Singleton factory for ArcInputMethodManagerService
class ArcInputMethodManagerServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInputMethodManagerService,
          ArcInputMethodManagerServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase
  static constexpr const char* kName = "ArcInputMethodManagerServiceFactory";

  static ArcInputMethodManagerServiceFactory* GetInstance() {
    return base::Singleton<ArcInputMethodManagerServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcInputMethodManagerServiceFactory>;
  ArcInputMethodManagerServiceFactory() = default;
  ~ArcInputMethodManagerServiceFactory() override = default;
};

class ArcInputMethodStateDelegateImpl : public ArcInputMethodState::Delegate {
 public:
  explicit ArcInputMethodStateDelegateImpl(Profile* profile)
      : profile_(profile) {}
  ArcInputMethodStateDelegateImpl(const ArcInputMethodStateDelegateImpl&) =
      delete;
  ArcInputMethodStateDelegateImpl& operator=(
      const ArcInputMethodStateDelegateImpl& state) = delete;
  ~ArcInputMethodStateDelegateImpl() override = default;

  // Returns whether ARC IMEs should be allowed now or not.
  // It depends on tablet mode state and a11y keyboard option.
  bool ShouldArcIMEAllowed() const override {
    const bool is_command_line_flag_enabled =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            keyboard::switches::kEnableVirtualKeyboard);
    const bool is_normal_vk_enabled =
        !profile_->GetPrefs()->GetBoolean(
            ash::prefs::kAccessibilityVirtualKeyboardEnabled) &&
        ash::TabletMode::Get()->InTabletMode();
    return is_command_line_flag_enabled || is_normal_vk_enabled;
  }

  chromeos::input_method::InputMethodDescriptor BuildInputMethodDescriptor(
      const mojom::ImeInfoPtr& info) const override {
    // We don't care too much about |layout| at this point since the feature is
    // for tablet mode.
    const std::string layout("us");

    // Set the fake language so that the IME is shown in the special section in
    // chrome://settings.
    const std::vector<std::string> languages{
        chromeos::extension_ime_util::kArcImeLanguage};

    const std::string display_name = info->display_name;

    const std::string proxy_ime_extension_id =
        crx_file::id_util::GenerateId(kArcIMEProxyExtensionName);
    const std::string& input_method_id =
        chromeos::extension_ime_util::GetArcInputMethodID(
            proxy_ime_extension_id, info->ime_id);
    // TODO(yhanada): Set the indicator string after the UI spec is finalized.
    return chromeos::input_method::InputMethodDescriptor(
        input_method_id, display_name, std::string() /* indicator */, layout,
        languages, false /* is_login_keyboard */, GURL(info->settings_url),
        GURL() /* input_view_url */);
  }

 private:
  Profile* const profile_;
};

}  // namespace

class ArcInputMethodManagerService::ArcInputMethodBoundsObserver
    : public ash::ArcInputMethodBoundsTracker::Observer {
 public:
  explicit ArcInputMethodBoundsObserver(ArcInputMethodManagerService* owner)
      : owner_(owner) {
    ash::ArcInputMethodBoundsTracker* tracker =
        ash::ArcInputMethodBoundsTracker::Get();
    if (tracker)
      tracker->AddObserver(this);
  }
  ArcInputMethodBoundsObserver(const ArcInputMethodBoundsObserver&) = delete;
  ~ArcInputMethodBoundsObserver() override {
    ash::ArcInputMethodBoundsTracker* tracker =
        ash::ArcInputMethodBoundsTracker::Get();
    if (tracker)
      tracker->RemoveObserver(this);
  }

  void OnArcInputMethodBoundsChanged(const gfx::Rect& bounds) override {
    owner_->OnArcInputMethodBoundsChanged(bounds);
  }

 private:
  ArcInputMethodManagerService* owner_;
};

class ArcInputMethodManagerService::InputMethodEngineObserver
    : public chromeos::InputMethodEngineBase::Observer {
 public:
  explicit InputMethodEngineObserver(ArcInputMethodManagerService* owner)
      : owner_(owner) {}
  ~InputMethodEngineObserver() override = default;

  // chromeos::InputMethodEngineBase::Observer overrides:
  void OnActivate(const std::string& engine_id) override {
    owner_->is_arc_ime_active_ = true;
    // TODO(yhanada): Remove this line after we migrate to SPM completely.
    owner_->OnInputContextHandlerChanged();
  }
  void OnFocus(
      int context_id,
      const ui::IMEEngineHandlerInterface::InputContext& context) override {
    owner_->Focus(context_id);
  }
  void OnBlur(int context_id) override { owner_->Blur(); }
  void OnKeyEvent(
      const std::string& engine_id,
      const ui::KeyEvent& event,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback key_data) override {
    if (event.key_code() == ui::VKEY_BROWSER_BACK &&
        event.type() == ui::ET_KEY_PRESSED &&
        owner_->IsVirtualKeyboardShown()) {
      // Back button on the shelf is pressed. We should consume only "keydown"
      // events here to make sure that Android side receives "keyup" events
      // always to prevent never-ending key repeat from happening.
      owner_->SendHideVirtualKeyboard();
      std::move(key_data).Run(true);
      return;
    }
    std::move(key_data).Run(false);
  }
  void OnReset(const std::string& engine_id) override {}
  void OnDeactivated(const std::string& engine_id) override {
    owner_->is_arc_ime_active_ = false;
    // TODO(yhanada): Remove this line after we migrate to SPM completely.
    owner_->OnInputContextHandlerChanged();
  }
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const std::u16string& text,
                                int cursor_pos,
                                int anchor_pos,
                                int offset_pos) override {
    owner_->UpdateTextInputState();
  }
  void OnCandidateClicked(
      const std::string& component_id,
      int candidate_id,
      chromeos::InputMethodEngineBase::MouseButtonEvent button) override {}
  void OnMenuItemActivated(const std::string& component_id,
                           const std::string& menu_id) override {}
  void OnScreenProjectionChanged(bool is_projected) override {}
  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {}
  void OnInputMethodOptionsChanged(const std::string& engine_id) override {}

 private:
  ArcInputMethodManagerService* const owner_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodEngineObserver);
};

class ArcInputMethodManagerService::InputMethodObserver
    : public ui::InputMethodObserver {
 public:
  explicit InputMethodObserver(ArcInputMethodManagerService* owner)
      : owner_(owner) {}
  ~InputMethodObserver() override = default;

  // ui::InputMethodObserver overrides:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {
    owner_->input_method_ = nullptr;
  }
  void OnShowVirtualKeyboardIfEnabled() override {
    owner_->SendShowVirtualKeyboard();
  }

 private:
  ArcInputMethodManagerService* const owner_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodObserver);
};

class ArcInputMethodManagerService::TabletModeObserver
    : public ash::TabletModeObserver {
 public:
  explicit TabletModeObserver(ArcInputMethodManagerService* owner)
      : owner_(owner) {}
  ~TabletModeObserver() override = default;

  // ash::TabletModeObserver overrides:
  void OnTabletModeStarted() override { OnTabletModeToggled(true); }
  void OnTabletModeEnded() override { OnTabletModeToggled(false); }

 private:
  void OnTabletModeToggled(bool enabled) {
    owner_->OnTabletModeToggled(enabled);
    owner_->NotifyInputMethodManagerObservers(enabled);
  }

  ArcInputMethodManagerService* owner_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeObserver);
};

// static
ArcInputMethodManagerService*
ArcInputMethodManagerService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInputMethodManagerServiceFactory::GetForBrowserContext(context);
}

// static
ArcInputMethodManagerService*
ArcInputMethodManagerService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcInputMethodManagerServiceFactory::GetForBrowserContextForTesting(
      context);
}

// static
BrowserContextKeyedServiceFactory* ArcInputMethodManagerService::GetFactory() {
  return ArcInputMethodManagerServiceFactory::GetInstance();
}

ArcInputMethodManagerService::ArcInputMethodManagerService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : profile_(Profile::FromBrowserContext(context)),
      imm_bridge_(
          std::make_unique<ArcInputMethodManagerBridgeImpl>(this,
                                                            bridge_service)),
      arc_ime_state_delegate_(
          std::make_unique<ArcInputMethodStateDelegateImpl>(profile_)),
      arc_ime_state_(arc_ime_state_delegate_.get()),
      prefs_(profile_),
      is_virtual_keyboard_shown_(false),
      is_updating_imm_entry_(false),
      proxy_ime_extension_id_(
          crx_file::id_util::GenerateId(kArcIMEProxyExtensionName)),
      proxy_ime_engine_(std::make_unique<chromeos::InputMethodEngine>()),
      tablet_mode_observer_(std::make_unique<TabletModeObserver>(this)),
      input_method_observer_(std::make_unique<InputMethodObserver>(this)),
      input_method_bounds_observer_(
          std::make_unique<ArcInputMethodBoundsObserver>(this)) {
  auto* imm = chromeos::input_method::InputMethodManager::Get();
  imm->AddObserver(this);
  imm->AddImeMenuObserver(this);

  proxy_ime_engine_->Initialize(
      std::make_unique<InputMethodEngineObserver>(this),
      proxy_ime_extension_id_.c_str(), profile_);

  ash::TabletMode::Get()->AddObserver(tablet_mode_observer_.get());

  auto* accessibility_manager = ash::AccessibilityManager::Get();
  if (accessibility_manager) {
    // accessibility_status_subscription_ ensures the callback is removed when
    // ArcInputMethodManagerService is destroyed, so it's safe to use
    // base::Unretained(this) here.
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &ArcInputMethodManagerService::OnAccessibilityStatusChanged,
            base::Unretained(this)));
  }

  DCHECK(ui::IMEBridge::Get());
  ui::IMEBridge::Get()->AddObserver(this);
}

ArcInputMethodManagerService::~ArcInputMethodManagerService() = default;

void ArcInputMethodManagerService::SetInputMethodManagerBridgeForTesting(
    std::unique_ptr<ArcInputMethodManagerBridge> test_bridge) {
  imm_bridge_ = std::move(test_bridge);
}

void ArcInputMethodManagerService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcInputMethodManagerService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcInputMethodManagerService::Shutdown() {
  // Remove any Arc IME entry from preferences before shutting down.
  // IME states (installed/enabled/disabled) are stored in Android's settings,
  // that will be restored after Arc container starts next time.
  prefs_.UpdateEnabledImes({});
  profile_->GetPrefs()->CommitPendingWrite();

  if (input_method_) {
    input_method_->RemoveObserver(input_method_observer_.get());
    input_method_ = nullptr;
  }

  if (ui::IMEBridge::Get())
    ui::IMEBridge::Get()->RemoveObserver(this);

  if (ash::TabletMode::Get())
    ash::TabletMode::Get()->RemoveObserver(tablet_mode_observer_.get());

  auto* imm = chromeos::input_method::InputMethodManager::Get();
  imm->RemoveImeMenuObserver(this);
  imm->RemoveObserver(this);
}

void ArcInputMethodManagerService::OnActiveImeChanged(
    const std::string& ime_id) {
  if (ime_id == kChromeOSIMEIdInArcContainer) {
    // Chrome OS Keyboard is selected in Android side.
    auto* imm = chromeos::input_method::InputMethodManager::Get();
    // Create a list of active Chrome OS IMEs.
    auto active_imes = imm->GetActiveIMEState()->GetActiveInputMethodIds();
    base::EraseIf(active_imes, chromeos::extension_ime_util::IsArcIME);
    DCHECK(!active_imes.empty());
    imm->GetActiveIMEState()->ChangeInputMethod(active_imes[0],
                                                false /* show_message */);
    return;
  }

  // an ARC IME is selected.
  auto* imm = chromeos::input_method::InputMethodManager::Get();
  imm->GetActiveIMEState()->ChangeInputMethod(
      chromeos::extension_ime_util::GetArcInputMethodID(proxy_ime_extension_id_,
                                                        ime_id),
      false /* show_message */);
}

void ArcInputMethodManagerService::OnImeDisabled(const std::string& ime_id) {
  arc_ime_state_.DisableInputMethod(ime_id);

  // Remove the IME from the prefs to disable it.
  prefs_.UpdateEnabledImes(arc_ime_state_.GetEnabledInputMethods());

  // Note: Since this is not about uninstalling the IME, this method does not
  // modify InputMethodManager::State.
}

void ArcInputMethodManagerService::OnImeInfoChanged(
    std::vector<mojom::ImeInfoPtr> ime_info_array) {
  arc_ime_state_.InitializeWithImeInfo(proxy_ime_extension_id_, ime_info_array);
  UpdateInputMethodEntryWithImeInfo();
}

void ArcInputMethodManagerService::UpdateInputMethodEntryWithImeInfo() {
  using chromeos::input_method::InputMethodDescriptor;
  using chromeos::input_method::InputMethodDescriptors;
  using chromeos::input_method::InputMethodManager;

  InputMethodManager* imm = InputMethodManager::Get();
  if (!imm || !imm->GetActiveIMEState()) {
    LOG(WARNING) << "InputMethodManager is not ready yet.";
    return;
  }

  base::AutoReset<bool> in_updating(&is_updating_imm_entry_, true);
  scoped_refptr<InputMethodManager::State> state = imm->GetActiveIMEState();
  const std::string active_ime_id = state->GetCurrentInputMethod().id();

  // Remove the old registered entry.
  state->RemoveInputMethodExtension(proxy_ime_extension_id_);

  const InputMethodDescriptors installed_imes =
      arc_ime_state_.GetActiveInputMethods();
  if (installed_imes.empty()) {
    // If no ARC IME is installed or allowed, remove ARC IME entry from
    // preferences.
    prefs_.UpdateEnabledImes({});
    return;
  }

  // Add the proxy IME entry to InputMethodManager if any ARC IME is installed.
  state->AddInputMethodExtension(proxy_ime_extension_id_, installed_imes,
                                 proxy_ime_engine_.get());

  // Enable IMEs that are already enabled in the container.
  // TODO(crbug.com/845079): We should keep the order of the IMEs as same as in
  // chrome://settings
  prefs_.UpdateEnabledImes(arc_ime_state_.GetEnabledInputMethods());

  for (const auto& descriptor : arc_ime_state_.GetEnabledInputMethods())
    state->EnableInputMethod(descriptor.id());

  state->ChangeInputMethod(active_ime_id, false);
  is_updating_imm_entry_ = false;

  // Call ImeMenuListChanged() here to notify the latest state.
  ImeMenuListChanged();
  // If the active input method is changed, call InputMethodChanged() here.
  if (active_ime_id != state->GetCurrentInputMethod().id())
    InputMethodChanged(InputMethodManager::Get(), nullptr, false);

  UMA_HISTOGRAM_COUNTS_100("Arc.ImeCount", installed_imes.size());
}

void ArcInputMethodManagerService::OnConnectionClosed() {
  // Remove all ARC IMEs from the list and prefs.
  const bool opted_out = !arc::IsArcPlayStoreEnabledForProfile(profile_);
  VLOG(1) << "Lost InputMethodManagerInstance. Reason="
          << (opted_out ? "opt-out" : "unknown");
  // TODO(yhanada): Handle prefs better. For example, when this method is called
  // because of the container crash (rather then opt-out), we might not want to
  // modify the preference at all.
  OnImeInfoChanged({});
}

void ArcInputMethodManagerService::ImeMenuListChanged() {
  // Ignore ime menu list change while updating the old entry in
  // |OnImeInfoChanged| not to expose temporary state to ARC++ container.
  if (is_updating_imm_entry_)
    return;

  auto* manager = chromeos::input_method::InputMethodManager::Get();
  if (!manager || !manager->GetActiveIMEState()) {
    LOG(WARNING) << "InputMethodManager is not ready yet";
    return;
  }

  auto new_active_ime_ids =
      manager->GetActiveIMEState()->GetActiveInputMethodIds();

  // Filter out non ARC IME ids.
  std::set<std::string> new_arc_active_ime_ids;
  std::copy_if(
      new_active_ime_ids.begin(), new_active_ime_ids.end(),
      std::inserter(new_arc_active_ime_ids, new_arc_active_ime_ids.end()),
      [](const auto& id) {
        return chromeos::extension_ime_util::IsArcIME(id);
      });

  // TODO(yhanada|yusukes): Instead of observing ImeMenuListChanged(), it's
  // probably better to just observe the pref (and not disabling ones still
  // in the prefs.) See also the comment below in the second for-loop.
  const std::set<std::string> active_ime_ids_on_prefs = prefs_.GetEnabledImes();

  for (const auto& id : new_arc_active_ime_ids) {
    // Enable the IME which is not currently enabled.
    if (!active_arc_ime_ids_.count(id))
      EnableIme(id, true /* enable */);
  }

  for (const auto& id : active_arc_ime_ids_) {
    if (!new_arc_active_ime_ids.count(id) &&
        !active_ime_ids_on_prefs.count(id)) {
      // This path is taken in the following two cases:
      // 1) The device is in tablet mode, and the user disabled the IME via
      //    chrome://settings.
      // 2) The device was just switched to laptop mode, and this service
      //    disallowed Android IMEs.
      // In the former case, |active_ime_ids_on_prefs| doesn't have the IME,
      // but in the latter case, the set still has it. Here, disable the IME
      // only for the former case so that the temporary deactivation of the
      // IME on laptop mode wouldn't be propagated to the container. Otherwise,
      // the IME confirmation dialog will be shown again next time when you
      // use the IME in tablet mode.
      // TODO(yhanada|yusukes): Only observe the prefs and remove the hack.
      EnableIme(id, false /* enable */);
    }
  }
  active_arc_ime_ids_.swap(new_arc_active_ime_ids);
}

void ArcInputMethodManagerService::InputMethodChanged(
    chromeos::input_method::InputMethodManager* manager,
    Profile* /* profile */,
    bool /* show_message */) {
  // Ignore input method change while updating the entry in |OnImeInfoChanged|
  // not to expose temporary state to ARC++ container.
  if (is_updating_imm_entry_)
    return;

  scoped_refptr<chromeos::input_method::InputMethodManager::State> state =
      manager->GetActiveIMEState();
  if (!state)
    return;
  SwitchImeTo(state->GetCurrentInputMethod().id());

  if (chromeos::extension_ime_util::IsArcIME(
          state->GetCurrentInputMethod().id())) {
    // Disable fallback virtual keyboard while Android IME is activated.
    SetKeyboardDisabled(true);
  } else {
    // Stop overriding virtual keyboard availability.
    SetKeyboardDisabled(false);
  }
}

void ArcInputMethodManagerService::OnInputContextHandlerChanged() {
  if (ui::IMEBridge::Get()->GetInputContextHandler() == nullptr) {
    if (input_method_)
      input_method_->RemoveObserver(input_method_observer_.get());
    input_method_ = nullptr;
    return;
  }

  if (input_method_)
    input_method_->RemoveObserver(input_method_observer_.get());
  input_method_ =
      ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (input_method_)
    input_method_->AddObserver(input_method_observer_.get());
}

void ArcInputMethodManagerService::OnAccessibilityStatusChanged(
    const ash::AccessibilityStatusEventDetails& event_details) {
  if (event_details.notification_type !=
      ash::AccessibilityNotificationType::kToggleVirtualKeyboard) {
    // This class is not interested in a11y events except toggling virtual
    // keyboard event.
    return;
  }

  UpdateInputMethodEntryWithImeInfo();
}

void ArcInputMethodManagerService::OnArcInputMethodBoundsChanged(
    const gfx::Rect& bounds) {
  if (is_virtual_keyboard_shown_ == !bounds.IsEmpty())
    return;
  is_virtual_keyboard_shown_ = !bounds.IsEmpty();
  NotifyVirtualKeyboardVisibilityChange(is_virtual_keyboard_shown_);
}

InputConnectionImpl*
ArcInputMethodManagerService::GetInputConnectionForTesting() {
  return active_connection_.get();
}

void ArcInputMethodManagerService::EnableIme(const std::string& ime_id,
                                             bool enable) {
  auto component_id =
      chromeos::extension_ime_util::GetComponentIDByInputMethodID(ime_id);

  // TODO(yhanada): Disable the IME in Chrome OS side if it fails.
  imm_bridge_->SendEnableIme(
      component_id, enable,
      base::BindOnce(
          [](const std::string& ime_id, bool enable, bool success) {
            if (!success) {
              LOG(ERROR) << (enable ? "Enabling" : "Disabling") << " \""
                         << ime_id << "\" failed";
            }
          },
          ime_id, enable));
}

void ArcInputMethodManagerService::SwitchImeTo(const std::string& ime_id) {
  namespace ceiu = chromeos::extension_ime_util;

  std::string component_id = ceiu::GetComponentIDByInputMethodID(ime_id);
  if (!ceiu::IsArcIME(ime_id))
    component_id = kChromeOSIMEIdInArcContainer;
  imm_bridge_->SendSwitchImeTo(
      component_id, base::BindOnce(&SwitchImeToCallback, ime_id, component_id));
}

void ArcInputMethodManagerService::Focus(int context_id) {
  if (!is_arc_ime_active_)
    return;

  DCHECK(!active_connection_);
  active_connection_ = std::make_unique<InputConnectionImpl>(
      proxy_ime_engine_.get(), imm_bridge_.get(), context_id);
  mojo::PendingRemote<mojom::InputConnection> connection_remote;
  active_connection_->Bind(&connection_remote);

  imm_bridge_->SendFocus(std::move(connection_remote),
                         active_connection_->GetTextInputState(false));
}

void ArcInputMethodManagerService::Blur() {
  active_connection_.reset();
  is_virtual_keyboard_shown_ = false;
}

void ArcInputMethodManagerService::UpdateTextInputState() {
  if (!is_arc_ime_active_ || !active_connection_)
    return;
  active_connection_->UpdateTextInputState(
      false /* is_input_state_update_requested */);
}

void ArcInputMethodManagerService::OnTabletModeToggled(bool /* enabled */) {
  UpdateInputMethodEntryWithImeInfo();
}

void ArcInputMethodManagerService::NotifyInputMethodManagerObservers(
    bool is_tablet_mode) {
  // Togging the mode may enable or disable all the ARC IMEs. To dynamically
  // reflect the potential state changes to chrome://settings, notify the
  // manager's observers here.
  // TODO(yusukes): This is a temporary workaround for supporting ARC IMEs
  // and supports neither Chrome OS extensions nor state changes enforced by
  // the policy. The better way to do this is to add a dedicated event to
  // language_settings_private.idl and send the new event to the JS side
  // instead.
  auto* manager = chromeos::input_method::InputMethodManager::Get();
  if (!manager)
    return;
  if (is_tablet_mode)
    manager->NotifyInputMethodExtensionRemoved(proxy_ime_extension_id_);
  else
    manager->NotifyInputMethodExtensionAdded(proxy_ime_extension_id_);
}

bool ArcInputMethodManagerService::IsVirtualKeyboardShown() const {
  return is_virtual_keyboard_shown_;
}

void ArcInputMethodManagerService::SendShowVirtualKeyboard() {
  if (!is_arc_ime_active_)
    return;

  imm_bridge_->SendShowVirtualKeyboard();
}

void ArcInputMethodManagerService::SendHideVirtualKeyboard() {
  if (!is_arc_ime_active_)
    return;

  imm_bridge_->SendHideVirtualKeyboard();
}

void ArcInputMethodManagerService::NotifyVirtualKeyboardVisibilityChange(
    bool visible) {
  if (!is_arc_ime_active_)
    return;
  for (auto& observer : observers_)
    observer.OnAndroidVirtualKeyboardVisibilityChanged(visible);
}

}  // namespace arc
