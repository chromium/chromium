// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_service.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_bridge_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/pref_names.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/mojom/ime_mojom_traits.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/ime_bridge.h"
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

}  // namespace

class ArcInputMethodManagerService::InputMethodEngineObserver
    : public input_method::InputMethodEngineBase::Observer {
 public:
  explicit InputMethodEngineObserver(ArcInputMethodManagerService* owner)
      : owner_(owner) {}
  ~InputMethodEngineObserver() override = default;

  // input_method::InputMethodEngineBase::Observer overrides:
  void OnActivate(const std::string& engine_id) override {
    owner_->is_arc_ime_active_ = true;
    // TODO(yhanada): Remove this line after we migrate to SPM completely.
    owner_->OnInputContextHandlerChanged();
  }
  void OnFocus(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {
    owner_->Focus(context.id);
  }
  void OnBlur(int context_id) override { owner_->Blur(); }
  void OnKeyEvent(
      const std::string& engine_id,
      const input_method::InputMethodEngineBase::KeyboardEvent& event,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback key_data) override {
    if (event.key_code == ui::VKEY_BROWSER_BACK &&
        owner_->IsVirtualKeyboardShown()) {
      // Back button on the shelf is pressed.
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
                                const std::string& text,
                                int cursor_pos,
                                int anchor_pos,
                                int offset_pos) override {
    owner_->UpdateTextInputState();
  }
  void OnInputContextUpdate(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {
    owner_->UpdateTextInputState();
  }
  void OnCandidateClicked(
      const std::string& component_id,
      int candidate_id,
      input_method::InputMethodEngineBase::MouseButtonEvent button) override {}
  void OnMenuItemActivated(const std::string& component_id,
                           const std::string& menu_id) override {}
  void OnScreenProjectionChanged(bool is_projected) override {}

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
    owner_->UpdateArcIMEAllowed();
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
      is_virtual_keyboard_shown_(false),
      is_updating_imm_entry_(false),
      proxy_ime_extension_id_(
          crx_file::id_util::GenerateId(kArcIMEProxyExtensionName)),
      proxy_ime_engine_(std::make_unique<chromeos::InputMethodEngine>()),
      tablet_mode_observer_(std::make_unique<TabletModeObserver>(this)),
      input_method_observer_(std::make_unique<InputMethodObserver>(this)) {
  auto* imm = chromeos::input_method::InputMethodManager::Get();
  imm->AddObserver(this);
  imm->AddImeMenuObserver(this);

  proxy_ime_engine_->Initialize(
      std::make_unique<InputMethodEngineObserver>(this),
      proxy_ime_extension_id_.c_str(), profile_);

  ash::TabletMode::Get()->AddObserver(tablet_mode_observer_.get());

  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
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
  RemoveArcIMEFromPrefs();
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
  const std::string component_id =
      chromeos::extension_ime_util::GetArcInputMethodID(proxy_ime_extension_id_,
                                                        ime_id);
  // Remove the IME from the prefs to disable it.
  const std::string active_ime_ids =
      profile_->GetPrefs()->GetString(prefs::kLanguageEnabledImes);
  std::vector<base::StringPiece> active_ime_list = base::SplitStringPiece(
      active_ime_ids, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::Erase(active_ime_list, component_id);
  profile_->GetPrefs()->SetString(prefs::kLanguageEnabledImes,
                                  base::JoinString(active_ime_list, ","));

  // Note: Since this is not about uninstalling the IME, this method does not
  // modify InputMethodManager::State.
}

void ArcInputMethodManagerService::OnImeInfoChanged(
    std::vector<mojom::ImeInfoPtr> ime_info_array) {
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

  // Convert ime_info_array to InputMethodDescriptors.
  InputMethodDescriptors descriptors;
  std::vector<std::string> enabled_input_method_ids;
  ime_ids_allowed_in_clamshell_mode_.clear();
  for (const auto& ime_info : ime_info_array) {
    const InputMethodDescriptor& descriptor =
        BuildInputMethodDescriptor(ime_info.get());
    descriptors.push_back(descriptor);
    if (ime_info->enabled)
      enabled_input_method_ids.push_back(descriptor.id());
    if (ime_info->is_allowed_in_clamshell_mode)
      ime_ids_allowed_in_clamshell_mode_.insert(descriptor.id());
  }
  if (descriptors.empty()) {
    // If no ARC IME is installed, remove ARC IME entry from preferences.
    RemoveArcIMEFromPrefs();
    return;
  }
  // Add the proxy IME entry to InputMethodManager if any ARC IME is installed.
  state->AddInputMethodExtension(proxy_ime_extension_id_, descriptors,
                                 proxy_ime_engine_.get());

  // Enable IMEs that are already enabled in the container.
  const std::string active_ime_ids =
      profile_->GetPrefs()->GetString(prefs::kLanguageEnabledImes);
  std::vector<std::string> active_ime_list = base::SplitString(
      active_ime_ids, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // TODO(crbug.com/845079): We should keep the order of the IMEs as same as in
  // chrome://settings
  for (const auto& input_method_id : enabled_input_method_ids) {
    if (!base::Contains(active_ime_list, input_method_id))
      active_ime_list.push_back(input_method_id);
  }
  // Disable IMEs that are already disable in the container.
  base::EraseIf(active_ime_list, [&enabled_input_method_ids](const auto& id) {
    return chromeos::extension_ime_util::IsArcIME(id) &&
           !base::Contains(enabled_input_method_ids, id);
  });
  profile_->GetPrefs()->SetString(prefs::kLanguageEnabledImes,
                                  base::JoinString(active_ime_list, ","));

  // Refresh allowed IME list.
  UpdateArcIMEAllowed();

  InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
      active_ime_id, false);
  is_updating_imm_entry_ = false;

  // Call ImeMenuListChanged() here to notify the latest state.
  ImeMenuListChanged();
  // If the active input method is changed, call InputMethodChanged() here.
  if (active_ime_id != state->GetCurrentInputMethod().id())
    InputMethodChanged(InputMethodManager::Get(), nullptr, false);

  UMA_HISTOGRAM_COUNTS_100("Arc.ImeCount", descriptors.size());
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
  std::set<std::string> active_ime_ids_on_prefs;
  {
    const std::string active_ime_ids =
        profile_->GetPrefs()->GetString(prefs::kLanguageEnabledImes);
    std::vector<base::StringPiece> active_ime_list = base::SplitStringPiece(
        active_ime_ids, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& id : active_ime_list)
      active_ime_ids_on_prefs.insert(id.as_string());
  }

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
    const chromeos::AccessibilityStatusEventDetails& event_details) {
  if (event_details.notification_type !=
      chromeos::ACCESSIBILITY_TOGGLE_VIRTUAL_KEYBOARD) {
    // This class is not interested in a11y events except toggling virtual
    // keyboard event.
    return;
  }

  UpdateArcIMEAllowed();
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
  mojom::InputConnectionPtr connection_ptr;
  active_connection_->Bind(&connection_ptr);

  imm_bridge_->SendFocus(std::move(connection_ptr),
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

chromeos::input_method::InputMethodDescriptor
ArcInputMethodManagerService::BuildInputMethodDescriptor(
    const mojom::ImeInfo* info) {
  // We don't care too much about |layouts| at this point since the feature is
  // for tablet mode.
  const std::vector<std::string> layouts{"us"};

  // Set the fake language so that the IME is shown in the special section in
  // chrome://settings.
  const std::vector<std::string> languages{
      chromeos::extension_ime_util::kArcImeLanguage};

  const std::string display_name = info->display_name;

  const std::string& input_method_id =
      chromeos::extension_ime_util::GetArcInputMethodID(proxy_ime_extension_id_,
                                                        info->ime_id);
  // TODO(yhanada): Set the indicator string after the UI spec is finalized.
  return chromeos::input_method::InputMethodDescriptor(
      input_method_id, display_name, std::string() /* indicator */, layouts,
      languages, false /* is_login_keyboard */, GURL(info->settings_url),
      GURL() /* input_view_url */);
}

void ArcInputMethodManagerService::RemoveArcIMEFromPrefs() {
  RemoveArcIMEFromPref(prefs::kLanguageEnabledImes);
  RemoveArcIMEFromPref(prefs::kLanguagePreloadEngines);

  PrefService* prefs = profile_->GetPrefs();
  if (chromeos::extension_ime_util::IsArcIME(
          prefs->GetString(prefs::kLanguageCurrentInputMethod))) {
    prefs->SetString(prefs::kLanguageCurrentInputMethod, std::string());
  }
  if (chromeos::extension_ime_util::IsArcIME(
          prefs->GetString(prefs::kLanguagePreviousInputMethod))) {
    prefs->SetString(prefs::kLanguagePreviousInputMethod, std::string());
  }
}

void ArcInputMethodManagerService::RemoveArcIMEFromPref(const char* pref_name) {
  const std::string ime_ids = profile_->GetPrefs()->GetString(pref_name);
  std::vector<base::StringPiece> ime_id_list = base::SplitStringPiece(
      ime_ids, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  base::EraseIf(ime_id_list, [](base::StringPiece id) {
    return chromeos::extension_ime_util::IsArcIME(id.as_string());
  });
  profile_->GetPrefs()->SetString(pref_name,
                                  base::JoinString(ime_id_list, ","));
}

void ArcInputMethodManagerService::UpdateArcIMEAllowed() {
  const bool allowed = ShouldArcIMEAllowed();

  auto* manager = chromeos::input_method::InputMethodManager::Get();
  std::set<std::string> allowed_method_ids_set;
  {
    std::vector<std::string> allowed_method_ids =
        manager->GetActiveIMEState()->GetAllowedInputMethods();
    allowed_method_ids_set = std::set<std::string>(allowed_method_ids.begin(),
                                                   allowed_method_ids.end());
  }
  chromeos::input_method::InputMethodDescriptors installed_imes;
  if (manager->GetComponentExtensionIMEManager()) {
    installed_imes = manager->GetComponentExtensionIMEManager()
                         ->GetAllIMEAsInputMethodDescriptor();
  }
  {
    chromeos::input_method::InputMethodDescriptors installed_extensions;
    manager->GetActiveIMEState()->GetInputMethodExtensions(
        &installed_extensions);
    installed_imes.insert(installed_imes.end(), installed_extensions.begin(),
                          installed_extensions.end());
  }

  std::vector<std::string> ime_ids_to_enable;
  if (allowed) {
    if (!allowed_method_ids_set.empty()) {
      // Some IMEs are not allowed now. Add ARC IMEs to
      // |allowed_method_ids_set|.
      for (const auto& desc : installed_imes) {
        if (chromeos::extension_ime_util::IsArcIME(desc.id()))
          allowed_method_ids_set.insert(desc.id());
      }
    }

    // Re-enable ARC IMEs that were auto-disabled when toggling to laptop mode.
    const std::string active_ime_ids =
        profile_->GetPrefs()->GetString(prefs::kLanguageEnabledImes);
    std::vector<base::StringPiece> active_ime_list = base::SplitStringPiece(
        active_ime_ids, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& id : active_ime_list) {
      if (chromeos::extension_ime_util::IsArcIME(id.as_string()))
        ime_ids_to_enable.push_back(id.as_string());
    }
  } else {
    // Disallow Arc IMEs.
    if (allowed_method_ids_set.empty()) {
      // Currently there is no restriction. Add all IMEs except ARC IMEs to
      // |allowed_method_ids_set|.
      for (const auto& desc : installed_imes) {
        if (!chromeos::extension_ime_util::IsArcIME(desc.id()) ||
            ime_ids_allowed_in_clamshell_mode_.count(desc.id())) {
          allowed_method_ids_set.insert(desc.id());
        }
      }
    } else {
      // Remove ARC IMEs from |allowed_method_ids_set|.
      base::EraseIf(allowed_method_ids_set, [this](const std::string& id) {
        return chromeos::extension_ime_util::IsArcIME(id) &&
               !ime_ids_allowed_in_clamshell_mode_.count(id);
      });

      // Add back IMEs allowed in clamshell mode.
      for (const auto& ime_id : ime_ids_allowed_in_clamshell_mode_)
        allowed_method_ids_set.insert(ime_id);
    }

    DCHECK(!allowed_method_ids_set.empty());
  }

  manager->GetActiveIMEState()->SetAllowedInputMethods(
      std::vector<std::string>(allowed_method_ids_set.begin(),
                               allowed_method_ids_set.end()),
      false /* enable_allowed_input_methods */);

  // This has to be called after SetAllowedInputMethods() because enabling an
  // IME that is disallowed always fails.
  for (const auto& id : ime_ids_to_enable)
    manager->GetActiveIMEState()->EnableInputMethod(id);
}

bool ArcInputMethodManagerService::ShouldArcIMEAllowed() const {
  const bool is_command_line_flag_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          keyboard::switches::kEnableVirtualKeyboard);
  const bool is_normal_vk_enabled =
      !profile_->GetPrefs()->GetBoolean(
          ash::prefs::kAccessibilityVirtualKeyboardEnabled) &&
      ash::TabletMode::Get()->InTabletMode();
  return is_command_line_flag_enabled || is_normal_vk_enabled;
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
  // TODO(yhanada): Should observe IME window size changes.
  is_virtual_keyboard_shown_ = true;

  NotifyVirtualKeyboardVisibilityChange(true);
}

void ArcInputMethodManagerService::SendHideVirtualKeyboard() {
  if (!is_arc_ime_active_)
    return;

  imm_bridge_->SendHideVirtualKeyboard();
  // TODO(yhanada): Should observe IME window size changes.
  is_virtual_keyboard_shown_ = false;

  NotifyVirtualKeyboardVisibilityChange(false);
}

void ArcInputMethodManagerService::NotifyVirtualKeyboardVisibilityChange(
    bool visible) {
  for (auto& observer : observers_)
    observer.OnAndroidVirtualKeyboardVisibilityChanged(visible);
}

}  // namespace arc
