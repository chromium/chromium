// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"

#include <stdint.h>

#include <algorithm>  // std::find
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/input_method/candidate_window_controller.h"
#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/devicemode.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/fake_ime_keyboard.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/ime_keyboard_impl.h"
#include "ui/base/ime/chromeos/input_method_delegate.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/chromeos/ime/input_method_menu_item.h"
#include "ui/chromeos/ime/input_method_menu_manager.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromeos {
namespace input_method {

namespace {

enum InputMethodCategory {
  INPUT_METHOD_CATEGORY_UNKNOWN = 0,
  INPUT_METHOD_CATEGORY_XKB,   // XKB input methods
  INPUT_METHOD_CATEGORY_ZH,    // Chinese input methods
  INPUT_METHOD_CATEGORY_JA,    // Japanese input methods
  INPUT_METHOD_CATEGORY_KO,    // Korean input methods
  INPUT_METHOD_CATEGORY_M17N,  // Multilingualization input methods
  INPUT_METHOD_CATEGORY_T13N,  // Transliteration input methods
  INPUT_METHOD_CATEGORY_ARC,   // ARC input methods
  INPUT_METHOD_CATEGORY_MAX
};

InputMethodCategory GetInputMethodCategory(const std::string& input_method_id) {
  const std::string component_id =
      extension_ime_util::GetComponentIDByInputMethodID(input_method_id);
  InputMethodCategory category = INPUT_METHOD_CATEGORY_UNKNOWN;
  if (base::StartsWith(component_id, "xkb:", base::CompareCase::SENSITIVE)) {
    category = INPUT_METHOD_CATEGORY_XKB;
  } else if (base::StartsWith(component_id, "zh-",
                              base::CompareCase::SENSITIVE)) {
    category = INPUT_METHOD_CATEGORY_ZH;
  } else if (base::StartsWith(component_id, "nacl_mozc_",
                              base::CompareCase::SENSITIVE)) {
    category = INPUT_METHOD_CATEGORY_JA;
  } else if (base::StartsWith(component_id, "hangul_",
                              base::CompareCase::SENSITIVE)) {
    category = INPUT_METHOD_CATEGORY_KO;
  } else if (base::StartsWith(component_id, "vkd_",
                              base::CompareCase::SENSITIVE)) {
    category = INPUT_METHOD_CATEGORY_M17N;
  } else if (component_id.find("-t-i0-") != std::string::npos) {
    category = INPUT_METHOD_CATEGORY_T13N;
  } else if (extension_ime_util::IsArcIME(input_method_id)) {
    category = INPUT_METHOD_CATEGORY_ARC;
  }

  return category;
}

std::string KeysetToString(mojom::ImeKeyset keyset) {
  switch (keyset) {
    case mojom::ImeKeyset::kNone:
      return "";
    case mojom::ImeKeyset::kEmoji:
      return "emoji";
    case mojom::ImeKeyset::kHandwriting:
      return "hwt";
    case mojom::ImeKeyset::kVoice:
      return "voice";
  }
}

}  // namespace

// ------------------------ InputMethodManagerImpl::StateImpl

InputMethodManagerImpl::StateImpl::StateImpl(InputMethodManagerImpl* manager,
                                             Profile* profile)
    : profile(profile), manager_(manager), menu_activated(false) {}

InputMethodManagerImpl::StateImpl::~StateImpl() = default;

void InputMethodManagerImpl::StateImpl::InitFrom(const StateImpl& other) {
  last_used_input_method = other.last_used_input_method;
  current_input_method = other.current_input_method;

  active_input_method_ids = other.active_input_method_ids;

  pending_input_method_id = other.pending_input_method_id;

  enabled_extension_imes = other.enabled_extension_imes;
  extra_input_methods = other.extra_input_methods;
  menu_activated = other.menu_activated;
  allowed_keyboard_layout_input_method_ids =
      other.allowed_keyboard_layout_input_method_ids;
  input_view_url = other.input_view_url;
}

bool InputMethodManagerImpl::StateImpl::IsActive() const {
  return manager_->state_.get() == this;
}

std::string InputMethodManagerImpl::StateImpl::Dump() const {
  std::ostringstream os;

  os << "################# "
     << (profile ? profile->GetProfileUserName() : std::string("NULL"))
     << " #################\n";

  os << "last_used_input_method: '"
     << last_used_input_method.GetPreferredKeyboardLayout() << "'\n";
  os << "current_input_method: '"
     << current_input_method.GetPreferredKeyboardLayout() << "'\n";
  os << "active_input_method_ids (size=" << active_input_method_ids.size()
     << "):";
  for (const auto& active_input_method_id : active_input_method_ids) {
    os << " '" << active_input_method_id << "',";
  }
  os << "\n";
  os << "enabled_extension_imes (size=" << enabled_extension_imes.size()
     << "):";
  for (const auto& enabled_extension_ime : enabled_extension_imes) {
    os << " '" << enabled_extension_ime << "'\n";
  }
  os << "\n";
  os << "extra_input_methods (size=" << extra_input_methods.size() << "):";
  for (const auto& entry : extra_input_methods) {
    os << " '" << entry.first << "' => '" << entry.second.id() << "',\n";
  }
  os << "pending_input_method_id: '" << pending_input_method_id << "'\n";
  os << "input_view_url: '" << input_view_url << "'\n";

  return os.str();
}

scoped_refptr<InputMethodManager::State>
InputMethodManagerImpl::StateImpl::Clone() const {
  scoped_refptr<StateImpl> new_state(new StateImpl(this->manager_, profile));
  new_state->InitFrom(*this);
  return scoped_refptr<InputMethodManager::State>(new_state.get());
}

std::unique_ptr<InputMethodDescriptors>
InputMethodManagerImpl::StateImpl::GetActiveInputMethods() const {
  std::unique_ptr<InputMethodDescriptors> result(new InputMethodDescriptors);
  // Build the active input method descriptors from the active input
  // methods cache |active_input_method_ids|.
  for (const auto& input_method_id : active_input_method_ids) {
    const InputMethodDescriptor* descriptor =
        manager_->util_.GetInputMethodDescriptorFromId(input_method_id);
    if (descriptor) {
      result->push_back(*descriptor);
    } else {
      const auto ix = extra_input_methods.find(input_method_id);
      if (ix != extra_input_methods.end())
        result->push_back(ix->second);
      else
        DVLOG(1) << "Descriptor is not found for: " << input_method_id;
    }
  }
  if (result->empty()) {
    // Initially |active_input_method_ids| is empty. browser_tests might take
    // this path.
    result->push_back(
        InputMethodUtil::GetFallbackInputMethodDescriptor());
  }
  return result;
}

const std::vector<std::string>&
InputMethodManagerImpl::StateImpl::GetActiveInputMethodIds() const {
  return active_input_method_ids;
}

size_t InputMethodManagerImpl::StateImpl::GetNumActiveInputMethods() const {
  return active_input_method_ids.size();
}

const InputMethodDescriptor*
InputMethodManagerImpl::StateImpl::GetInputMethodFromId(
    const std::string& input_method_id) const {
  const InputMethodDescriptor* ime =
      manager_->util_.GetInputMethodDescriptorFromId(input_method_id);
  if (!ime) {
    const auto ix = extra_input_methods.find(input_method_id);
    if (ix != extra_input_methods.end())
      ime = &ix->second;
  }
  return ime;
}

void InputMethodManagerImpl::StateImpl::EnableLoginLayouts(
    const std::string& language_code,
    const std::vector<std::string>& initial_layouts) {
  if (manager_->ui_session_ == STATE_TERMINATING)
    return;

  // First, hardware keyboard layout should be shown.
  std::vector<std::string> candidates =
      manager_->util_.GetHardwareLoginInputMethodIds();

  // Second, locale based input method should be shown.
  // Add input methods associated with the language.
  std::vector<std::string> layouts_from_locale;
  manager_->util_.GetInputMethodIdsFromLanguageCode(
      language_code, kKeyboardLayoutsOnly, &layouts_from_locale);
  candidates.insert(candidates.end(), layouts_from_locale.begin(),
                    layouts_from_locale.end());

  std::vector<std::string> layouts;
  // First, add the initial input method ID, if it's requested, to
  // layouts, so it appears first on the list of active input
  // methods at the input language status menu.
  for (const auto& initial_layout : initial_layouts) {
    if (manager_->util_.IsValidInputMethodId(initial_layout)) {
      if (manager_->IsLoginKeyboard(initial_layout)) {
        if (IsInputMethodAllowed(initial_layout)) {
          layouts.push_back(initial_layout);
        } else {
          DVLOG(1) << "EnableLoginLayouts: ignoring layout disallowd by policy:"
                   << initial_layout;
        }
      } else {
        DVLOG(1)
            << "EnableLoginLayouts: ignoring non-login initial keyboard layout:"
            << initial_layout;
      }
    } else if (!initial_layout.empty()) {
      DVLOG(1) << "EnableLoginLayouts: ignoring non-keyboard or invalid ID: "
               << initial_layout;
    }
  }

  // Add candidates to layouts, while skipping duplicates.
  for (const auto& candidate : candidates) {
    // Not efficient, but should be fine, as the two vectors are very
    // short (2-5 items).
    if (!base::Contains(layouts, candidate) &&
        manager_->IsLoginKeyboard(candidate) &&
        IsInputMethodAllowed(candidate)) {
      layouts.push_back(candidate);
    }
  }

  manager_->MigrateInputMethods(&layouts);
  active_input_method_ids.swap(layouts);

  if (IsActive()) {
    // Initialize candidate window controller and widgets such as
    // candidate window, infolist and mode indicator.  Note, mode
    // indicator is used by only keyboard layout input methods.
    if (active_input_method_ids.size() > 1)
      manager_->MaybeInitializeCandidateWindowController();

    // you can pass empty |initial_layout|.
    ChangeInputMethod(initial_layouts.empty()
                          ? std::string()
                          : extension_ime_util::GetInputMethodIDByEngineID(
                                initial_layouts[0]),
                      false);
  }
}

void InputMethodManagerImpl::StateImpl::EnableLockScreenLayouts() {
  std::set<std::string> added_ids;

  const std::vector<std::string>& hardware_keyboard_ids =
      manager_->util_.GetHardwareLoginInputMethodIds();

  std::vector<std::string> new_active_input_method_ids;
  for (const auto& input_method_id : active_input_method_ids) {
    // Skip if it's not a keyboard layout. Drop input methods including
    // extension ones. We need to keep all IMEs to support inputting on inline
    // reply on a notification if notifications on lock screen is enabled.
    if ((!ash::features::IsLockScreenInlineReplyEnabled() &&
         !manager_->IsLoginKeyboard(input_method_id)) ||
        added_ids.count(input_method_id)) {
      continue;
    }
    new_active_input_method_ids.push_back(input_method_id);
    added_ids.insert(input_method_id);
  }

  // We'll add the hardware keyboard if it's not included in
  // |active_input_method_ids| so that the user can always use the hardware
  // keyboard on the screen locker.
  for (const auto& hardware_keyboard_id : hardware_keyboard_ids) {
    if (added_ids.count(hardware_keyboard_id))
      continue;
    new_active_input_method_ids.push_back(hardware_keyboard_id);
    added_ids.insert(hardware_keyboard_id);
  }

  active_input_method_ids.swap(new_active_input_method_ids);

  // Re-check current_input_method.
  ChangeInputMethod(current_input_method.id(), false);
}

// Adds new input method to given list.
bool InputMethodManagerImpl::StateImpl::EnableInputMethodImpl(
    const std::string& input_method_id,
    std::vector<std::string>* new_active_input_method_ids) const {
  if (!IsInputMethodAllowed(input_method_id)) {
    DVLOG(1) << "EnableInputMethod: " << input_method_id << " is not allowed.";
    return false;
  }

  DCHECK(new_active_input_method_ids);
  if (!manager_->util_.IsValidInputMethodId(input_method_id)) {
    DVLOG(1) << "EnableInputMethod: Invalid ID: " << input_method_id;
    return false;
  }

  if (!base::Contains(*new_active_input_method_ids, input_method_id))
    new_active_input_method_ids->push_back(input_method_id);

  return true;
}

bool InputMethodManagerImpl::StateImpl::EnableInputMethod(
    const std::string& input_method_id) {
  if (!EnableInputMethodImpl(input_method_id, &active_input_method_ids))
    return false;

  manager_->ReconfigureIMFramework(this);
  return true;
}

bool InputMethodManagerImpl::StateImpl::ReplaceEnabledInputMethods(
    const std::vector<std::string>& new_active_input_method_ids) {
  if (manager_->ui_session_ == STATE_TERMINATING)
    return false;

  // Filter unknown or obsolete IDs.
  std::vector<std::string> new_active_input_method_ids_filtered;

  for (const auto& new_active_input_method_id : new_active_input_method_ids)
    EnableInputMethodImpl(new_active_input_method_id,
                          &new_active_input_method_ids_filtered);

  if (new_active_input_method_ids_filtered.empty()) {
    DVLOG(1) << "ReplaceEnabledInputMethods: No valid input method ID";
    return false;
  }

  // Copy extension IDs to |new_active_input_method_ids_filtered|. We have to
  // keep relative order of the extension input method IDs.
  for (const auto& input_method_id : active_input_method_ids) {
    if (extension_ime_util::IsExtensionIME(input_method_id))
      new_active_input_method_ids_filtered.push_back(input_method_id);
  }
  active_input_method_ids.swap(new_active_input_method_ids_filtered);
  manager_->MigrateInputMethods(&active_input_method_ids);

  manager_->ReconfigureIMFramework(this);

  // If |current_input_method| is no longer in |active_input_method_ids|,
  // ChangeInputMethod() picks the first one in |active_input_method_ids|.
  ChangeInputMethod(current_input_method.id(), false);

  // Record histogram for active input method count.
  UMA_HISTOGRAM_COUNTS_1M("InputMethod.ActiveCount",
                          active_input_method_ids.size());

  return true;
}

bool InputMethodManagerImpl::StateImpl::SetAllowedInputMethods(
    const std::vector<std::string>& new_allowed_input_method_ids,
    bool enable_allowed_input_methods) {
  allowed_keyboard_layout_input_method_ids.clear();
  for (auto input_method_id : new_allowed_input_method_ids) {
    std::string migrated_id =
        manager_->util_.MigrateInputMethod(input_method_id);
    if (manager_->util_.IsValidInputMethodId(migrated_id)) {
      allowed_keyboard_layout_input_method_ids.push_back(migrated_id);
    }
  }

  if (allowed_keyboard_layout_input_method_ids.empty()) {
    // None of the passed input methods were valid, so allow everything.
    return false;
  }

  std::vector<std::string> new_active_input_method_ids;
  if (enable_allowed_input_methods) {
    // Enable all allowed keyboard layout input methods. Leave all non-keyboard
    // input methods enabled.
    new_active_input_method_ids = allowed_keyboard_layout_input_method_ids;
    for (auto active_input_method_id : active_input_method_ids) {
      if (!manager_->util_.IsKeyboardLayout(active_input_method_id))
        new_active_input_method_ids.push_back(active_input_method_id);
    }
  } else {
    // Filter all currently active input methods and leave only non-keyboard or
    // allowed keyboard layouts. If no input method remains, take a fallback
    // keyboard layout.
    bool has_keyboard_layout = false;
    for (auto active_input_method_id : active_input_method_ids) {
      if (IsInputMethodAllowed(active_input_method_id)) {
        new_active_input_method_ids.push_back(active_input_method_id);
        has_keyboard_layout |=
            manager_->util_.IsKeyboardLayout(active_input_method_id);
      }
    }
    if (!has_keyboard_layout)
      new_active_input_method_ids.push_back(GetAllowedFallBackKeyboardLayout());
  }
  return ReplaceEnabledInputMethods(new_active_input_method_ids);
}

const std::vector<std::string>&
InputMethodManagerImpl::StateImpl::GetAllowedInputMethods() {
  return allowed_keyboard_layout_input_method_ids;
}

bool InputMethodManagerImpl::StateImpl::IsInputMethodAllowed(
    const std::string& input_method_id) const {
  // Every input method is allowed if SetAllowedKeyboardLayoutInputMethods has
  // not been called.
  if (allowed_keyboard_layout_input_method_ids.empty())
    return true;

  // We only restrict keyboard layouts.
  if (!manager_->util_.IsKeyboardLayout(input_method_id) &&
      !extension_ime_util::IsArcIME(input_method_id)) {
    return true;
  }

  return base::Contains(allowed_keyboard_layout_input_method_ids,
                        input_method_id) ||
         base::Contains(allowed_keyboard_layout_input_method_ids,
                        manager_->util_.MigrateInputMethod(input_method_id));
}

std::string
InputMethodManagerImpl::StateImpl::GetAllowedFallBackKeyboardLayout() const {
  for (const std::string& hardware_id :
       manager_->util_.GetHardwareInputMethodIds()) {
    if (IsInputMethodAllowed(hardware_id))
      return hardware_id;
  }
  return allowed_keyboard_layout_input_method_ids[0];
}

void InputMethodManagerImpl::StateImpl::ChangeInputMethod(
    const std::string& input_method_id,
    bool show_message) {
  if (manager_->ui_session_ == STATE_TERMINATING)
    return;

  bool notify_menu = false;

  // Always lookup input method, even if it is the same as
  // |current_input_method| because If it is no longer in
  // |active_input_method_ids|, pick the first one in
  // |active_input_method_ids|.
  const InputMethodDescriptor* descriptor =
      manager_->LookupInputMethod(input_method_id, this);
  if (!descriptor) {
    descriptor = manager_->LookupInputMethod(
        manager_->util_.MigrateInputMethod(input_method_id), this);
    if (!descriptor) {
      LOG(ERROR) << "Can't find InputMethodDescriptor for \"" << input_method_id
                 << "\"";
      return;
    }
  }

  // For 3rd party IME, when the user just logged in, SetEnabledExtensionImes
  // happens after activating the 3rd party IME.
  // So here to record the 3rd party IME to be activated, and activate it
  // when SetEnabledExtensionImes happens later.
  if (MethodAwaitsExtensionLoad(input_method_id))
    pending_input_method_id = input_method_id;

  if (descriptor->id() != current_input_method.id()) {
    last_used_input_method = current_input_method;
    current_input_method = *descriptor;
    notify_menu = true;
  }

  // Always change input method even if it is the same.
  // TODO(komatsu): Revisit if this is neccessary.
  if (IsActive())
    manager_->ChangeInputMethodInternal(*descriptor, profile, show_message,
                                        notify_menu);
  manager_->RecordInputMethodUsage(current_input_method.id());
}

void InputMethodManagerImpl::StateImpl::ChangeInputMethodToJpKeyboard() {
  ChangeInputMethod(
      extension_ime_util::GetInputMethodIDByEngineID("xkb:jp::jpn"), true);
}

void InputMethodManagerImpl::StateImpl::ChangeInputMethodToJpIme() {
  ChangeInputMethod(
      extension_ime_util::GetInputMethodIDByEngineID("nacl_mozc_jp"), true);
}

void InputMethodManagerImpl::StateImpl::ToggleInputMethodForJpIme() {
  std::string jp_ime_id =
      extension_ime_util::GetInputMethodIDByEngineID("nacl_mozc_jp");
  ChangeInputMethod(
      GetCurrentInputMethod().id() == jp_ime_id
          ? extension_ime_util::GetInputMethodIDByEngineID("xkb:jp::jpn")
          : jp_ime_id,
      true);
}

bool InputMethodManagerImpl::StateImpl::MethodAwaitsExtensionLoad(
    const std::string& input_method_id) const {
  // For 3rd party IME, when the user just logged in, SetEnabledExtensionImes
  // happens after activating the 3rd party IME.
  // So here to record the 3rd party IME to be activated, and activate it
  // when SetEnabledExtensionImes happens later.
  return !InputMethodIsActivated(input_method_id) &&
         extension_ime_util::IsExtensionIME(input_method_id);
}

void InputMethodManagerImpl::StateImpl::AddInputMethodExtension(
    const std::string& extension_id,
    const InputMethodDescriptors& descriptors,
    ui::IMEEngineHandlerInterface* engine) {
  if (manager_->ui_session_ == STATE_TERMINATING)
    return;

  DCHECK(engine);

  manager_->engine_map_[profile][extension_id] = engine;
  VLOG(1) << "Add an engine for \"" << extension_id << "\"";

  bool contain = false;
  for (const auto& descriptor : descriptors) {
    const std::string& id = descriptor.id();
    extra_input_methods[id] = descriptor;
    if (base::Contains(enabled_extension_imes, id)) {
      if (!base::Contains(active_input_method_ids, id)) {
        active_input_method_ids.push_back(id);
      } else {
        DVLOG(1) << "AddInputMethodExtension: already added: " << id << ", "
                 << descriptor.name();
      }
      contain = true;
    }
  }

  if (IsActive()) {
    if (extension_id == extension_ime_util::GetExtensionIDFromInputMethodID(
                            current_input_method.id())) {
      ui::IMEBridge::Get()->SetCurrentEngineHandler(engine);
      engine->Enable(extension_ime_util::GetComponentIDByInputMethodID(
          current_input_method.id()));
    }

    // Ensure that the input method daemon is running.
    if (contain)
      manager_->MaybeInitializeCandidateWindowController();
  }

  manager_->NotifyImeMenuListChanged();
  manager_->NotifyInputMethodExtensionAdded(extension_id);
}

void InputMethodManagerImpl::StateImpl::RemoveInputMethodExtension(
    const std::string& extension_id) {
  // Remove the active input methods with |extension_id|.
  std::vector<std::string> new_active_input_method_ids;
  for (const auto& active_input_method_id : active_input_method_ids) {
    if (extension_id != extension_ime_util::GetExtensionIDFromInputMethodID(
                            active_input_method_id))
      new_active_input_method_ids.push_back(active_input_method_id);
  }
  active_input_method_ids.swap(new_active_input_method_ids);

  // Remove the extra input methods with |extension_id|.
  std::map<std::string, InputMethodDescriptor> new_extra_input_methods;
  for (const auto& entry : extra_input_methods) {
    if (extension_id !=
        extension_ime_util::GetExtensionIDFromInputMethodID(entry.first))
      new_extra_input_methods[entry.first] = entry.second;
  }
  extra_input_methods.swap(new_extra_input_methods);

  if (IsActive()) {
    if (ui::IMEBridge::Get()->GetCurrentEngineHandler() ==
        manager_->engine_map_[profile][extension_id]) {
      ui::IMEBridge::Get()->SetCurrentEngineHandler(NULL);
    }
    manager_->engine_map_[profile].erase(extension_id);
  }

  // If |current_input_method| is no longer in |active_input_method_ids|,
  // switch to the first one in |active_input_method_ids|.
  ChangeInputMethod(current_input_method.id(), false);
  manager_->NotifyInputMethodExtensionRemoved(extension_id);
}

void InputMethodManagerImpl::StateImpl::GetInputMethodExtensions(
    InputMethodDescriptors* result) {
  // Build the extension input method descriptors from the extra input
  // methods cache |extra_input_methods|.
  for (const auto& entry : extra_input_methods) {
    if (extension_ime_util::IsExtensionIME(entry.first) ||
        extension_ime_util::IsArcIME(entry.first)) {
      result->push_back(entry.second);
    }
  }
}

void InputMethodManagerImpl::StateImpl::SetEnabledExtensionImes(
    std::vector<std::string>* ids) {
  enabled_extension_imes.clear();
  enabled_extension_imes.insert(
      enabled_extension_imes.end(), ids->begin(), ids->end());
  bool active_imes_changed = false;
  bool switch_to_pending = false;

  for (const auto& entry : extra_input_methods) {
    if (extension_ime_util::IsComponentExtensionIME(entry.first))
      continue;  // Do not filter component extension.

    if (pending_input_method_id == entry.first)
      switch_to_pending = true;

    const auto active_iter =
        std::find(active_input_method_ids.begin(),
                  active_input_method_ids.end(), entry.first);

    bool active = active_iter != active_input_method_ids.end();
    bool enabled = base::Contains(enabled_extension_imes, entry.first);

    if (active && !enabled)
      active_input_method_ids.erase(active_iter);

    if (!active && enabled)
      active_input_method_ids.push_back(entry.first);

    if (active == !enabled)
      active_imes_changed = true;
  }

  if (IsActive() && active_imes_changed) {
    manager_->MaybeInitializeCandidateWindowController();

    if (switch_to_pending) {
      ChangeInputMethod(pending_input_method_id, false);
      pending_input_method_id.clear();
    } else {
      // If |current_input_method| is no longer in |active_input_method_ids_|,
      // switch to the first one in |active_input_method_ids_|.
      ChangeInputMethod(current_input_method.id(), false);
    }
  }
}

void InputMethodManagerImpl::StateImpl::SetInputMethodLoginDefaultFromVPD(
    const std::string& locale,
    const std::string& oem_layout) {
  std::string layout;
  if (!oem_layout.empty()) {
    // If the OEM layout information is provided, use it.
    layout = oem_layout;
  } else {
    // Otherwise, determine the hardware keyboard from the locale.
    std::vector<std::string> input_method_ids;
    if (manager_->util_.GetInputMethodIdsFromLanguageCode(
            locale,
            chromeos::input_method::kKeyboardLayoutsOnly,
            &input_method_ids)) {
      // The output list |input_method_ids| is sorted by popularity, hence
      // input_method_ids[0] now contains the most popular keyboard layout
      // for the given locale.
      DCHECK_GE(input_method_ids.size(), 1U);
      layout = input_method_ids[0];
    }
  }

  if (layout.empty())
    return;

  std::vector<std::string> layouts = base::SplitString(
      layout, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  manager_->MigrateInputMethods(&layouts);

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kHardwareKeyboardLayout,
                   base::JoinString(layouts, ","));

  // This asks the file thread to save the prefs (i.e. doesn't block).
  // The latest values of Local State reside in memory so we can safely
  // get the value of kHardwareKeyboardLayout even if the data is not
  // yet saved to disk.
  prefs->CommitPendingWrite();

  manager_->util_.UpdateHardwareLayoutCache();

  EnableLoginLayouts(locale, layouts);
  manager_->LoadNecessaryComponentExtensions(this);
}

void InputMethodManagerImpl::StateImpl::SetInputMethodLoginDefault() {
  // Set up keyboards. For example, when |locale| is "en-US", enable US qwerty
  // and US dvorak keyboard layouts.
  if (g_browser_process && g_browser_process->local_state()) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    std::vector<std::string> input_methods_to_be_enabled;
    if (!GetAllowedInputMethods().empty()) {
      // Prefer policy-set input methods.
      input_methods_to_be_enabled = GetAllowedInputMethods();
    } else {
      // If the preferred keyboard for the login screen has been saved, use it.
      PrefService* prefs = g_browser_process->local_state();
      std::string initial_input_method_id =
          prefs->GetString(chromeos::language_prefs::kPreferredKeyboardLayout);
      if (initial_input_method_id.empty()) {
        // If kPreferredKeyboardLayout is not specified, use the hardware
        // layout.
        input_methods_to_be_enabled =
            manager_->util_.GetHardwareLoginInputMethodIds();
      } else {
        input_methods_to_be_enabled.push_back(initial_input_method_id);
      }
    }
    EnableLoginLayouts(locale, input_methods_to_be_enabled);
    manager_->LoadNecessaryComponentExtensions(this);
  }
}

bool InputMethodManagerImpl::StateImpl::CanCycleInputMethod() const {
  // Sanity checks.
  if (active_input_method_ids.empty()) {
    DVLOG(1) << "active input method is empty";
    return false;
  }

  if (current_input_method.id().empty()) {
    DVLOG(1) << "current_input_method is unknown";
    return false;
  }

  return active_input_method_ids.size() > 1;
}

void InputMethodManagerImpl::StateImpl::SwitchToNextInputMethod() {
  if (!CanCycleInputMethod())
    return;

  // Find the next input method and switch to it.
  SwitchToNextInputMethodInternal(active_input_method_ids,
                                  current_input_method.id());
}

void InputMethodManagerImpl::StateImpl::SwitchToLastUsedInputMethod() {
  if (!CanCycleInputMethod())
    return;

  if (last_used_input_method.id().empty() ||
      last_used_input_method.id() == current_input_method.id()) {
    SwitchToNextInputMethod();
    return;
  }

  const auto iter =
      std::find(active_input_method_ids.begin(), active_input_method_ids.end(),
                last_used_input_method.id());
  if (iter == active_input_method_ids.end()) {
    // last_used_input_method is not supported.
    SwitchToNextInputMethod();
    return;
  }
  ChangeInputMethod(*iter, true);
}

void InputMethodManagerImpl::StateImpl::SwitchToNextInputMethodInternal(
    const std::vector<std::string>& input_method_ids,
    const std::string& current_input_method_id) {
  auto iter = std::find(input_method_ids.begin(), input_method_ids.end(),
                        current_input_method_id);
  if (iter != input_method_ids.end())
    ++iter;
  if (iter == input_method_ids.end())
    iter = input_method_ids.begin();
  ChangeInputMethod(*iter, true);
}

InputMethodDescriptor InputMethodManagerImpl::StateImpl::GetCurrentInputMethod()
    const {
  if (current_input_method.id().empty())
    return InputMethodUtil::GetFallbackInputMethodDescriptor();

  return current_input_method;
}

bool InputMethodManagerImpl::StateImpl::InputMethodIsActivated(
    const std::string& input_method_id) const {
  return base::Contains(active_input_method_ids, input_method_id);
}

void InputMethodManagerImpl::StateImpl::EnableInputView() {
  input_view_url = current_input_method.input_view_url();
}

void InputMethodManagerImpl::StateImpl::DisableInputView() {
  input_view_url = GURL();
}

const GURL& InputMethodManagerImpl::StateImpl::GetInputViewUrl() const {
  return input_view_url;
}

void InputMethodManagerImpl::StateImpl::ConnectMojoManager(
    mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver) {
  if (!ime_service_connector_) {
    ime_service_connector_ = std::make_unique<ImeServiceConnector>(profile);
  }
  ime_service_connector_->SetupImeService(std::move(receiver));
}

// ------------------------ InputMethodManagerImpl
bool InputMethodManagerImpl::IsLoginKeyboard(
    const std::string& layout) const {
  return util_.IsLoginKeyboard(layout);
}

bool InputMethodManagerImpl::MigrateInputMethods(
    std::vector<std::string>* input_method_ids) {
  return util_.MigrateInputMethods(input_method_ids);
}

// Starts or stops the system input method framework as needed.
void InputMethodManagerImpl::ReconfigureIMFramework(
    InputMethodManagerImpl::StateImpl* state) {
  LoadNecessaryComponentExtensions(state);

  // Initialize candidate window controller and widgets such as
  // candidate window, infolist and mode indicator.  Note, mode
  // indicator is used by only keyboard layout input methods.
  if (state_.get() == state)
    MaybeInitializeCandidateWindowController();
}

void InputMethodManagerImpl::SetState(
    scoped_refptr<InputMethodManager::State> state) {
  DCHECK(state.get());
  auto* new_impl_state =
      static_cast<InputMethodManagerImpl::StateImpl*>(state.get());

  state_ = new_impl_state;

  if (state_.get() && state_->active_input_method_ids.size()) {
    // Initialize candidate window controller and widgets such as
    // candidate window, infolist and mode indicator.  Note, mode
    // indicator is used by only keyboard layout input methods.
    MaybeInitializeCandidateWindowController();

    // Always call ChangeInputMethodInternal even when the input method id
    // remain unchanged, because onActivate event needs to be sent to IME
    // extension to update the current screen type correctly.
    ChangeInputMethodInternal(state_->current_input_method, state_->profile,
                              false /* show_message */, true /* notify_menu */);
  }
}

scoped_refptr<InputMethodManager::State>
InputMethodManagerImpl::GetActiveIMEState() {
  return scoped_refptr<InputMethodManager::State>(state_.get());
}

InputMethodManagerImpl::InputMethodManagerImpl(
    std::unique_ptr<InputMethodDelegate> delegate,
    bool enable_extension_loading)
    : delegate_(std::move(delegate)),
      ui_session_(STATE_LOGIN_SCREEN),
      util_(delegate_.get()),
      component_extension_ime_manager_(new ComponentExtensionIMEManager()),
      enable_extension_loading_(enable_extension_loading),
      features_enabled_state_(InputMethodManager::FEATURE_ALL) {
  if (IsRunningAsSystemCompositor()) {
    keyboard_ = std::make_unique<ImeKeyboardImpl>(
        ui::OzonePlatform::GetInstance()->GetInputController());
  } else {
    keyboard_ = std::make_unique<FakeImeKeyboard>();
  }
  // Initializes the system IME list.
  std::unique_ptr<ComponentExtensionIMEManagerDelegate> comp_delegate(
      new ComponentExtensionIMEManagerImpl());
  component_extension_ime_manager_->Initialize(std::move(comp_delegate));
  const InputMethodDescriptors& descriptors =
      component_extension_ime_manager_->GetAllIMEAsInputMethodDescriptor();
  util_.ResetInputMethods(descriptors);
  chromeos::UserAddingScreen::Get()->AddObserver(this);
}

InputMethodManagerImpl::~InputMethodManagerImpl() {
  if (candidate_window_controller_.get())
    candidate_window_controller_->RemoveObserver(this);
  chromeos::UserAddingScreen::Get()->RemoveObserver(this);
}

void InputMethodManagerImpl::RecordInputMethodUsage(
    const std::string& input_method_id) {
  UMA_HISTOGRAM_ENUMERATION("InputMethod.Category",
                            GetInputMethodCategory(input_method_id),
                            INPUT_METHOD_CATEGORY_MAX);
  base::UmaHistogramSparse("InputMethod.ID2",
                           static_cast<int32_t>(base::Hash(input_method_id)));
}

void InputMethodManagerImpl::AddObserver(
    InputMethodManager::Observer* observer) {
  observers_.AddObserver(observer);
  observer->OnExtraInputEnabledStateChange(
      // TODO(shuchen): Remove this parameter - ex features::kEHVInputOnImeMenu
      true, features_enabled_state_ & InputMethodManager::FEATURE_EMOJI,
      features_enabled_state_ & InputMethodManager::FEATURE_HANDWRITING,
      features_enabled_state_ & InputMethodManager::FEATURE_VOICE);
}

void InputMethodManagerImpl::AddCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {
  candidate_window_observers_.AddObserver(observer);
}

void InputMethodManagerImpl::AddImeMenuObserver(
    InputMethodManager::ImeMenuObserver* observer) {
  ime_menu_observers_.AddObserver(observer);
}

void InputMethodManagerImpl::RemoveObserver(
    InputMethodManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InputMethodManagerImpl::RemoveCandidateWindowObserver(
    InputMethodManager::CandidateWindowObserver* observer) {
  candidate_window_observers_.RemoveObserver(observer);
}

void InputMethodManagerImpl::RemoveImeMenuObserver(
    InputMethodManager::ImeMenuObserver* observer) {
  ime_menu_observers_.RemoveObserver(observer);
}

InputMethodManager::UISessionState InputMethodManagerImpl::GetUISessionState() {
  return ui_session_;
}

void InputMethodManagerImpl::SetUISessionState(UISessionState new_ui_session) {
  ui_session_ = new_ui_session;
  if (ui_session_ == STATE_TERMINATING && candidate_window_controller_.get())
    candidate_window_controller_.reset();
}

void InputMethodManagerImpl::OnUserAddingStarted() {
  if (ui_session_ == STATE_BROWSER_SCREEN)
    SetUISessionState(STATE_SECONDARY_LOGIN_SCREEN);
}

void InputMethodManagerImpl::OnUserAddingFinished() {
  if (ui_session_ == STATE_SECONDARY_LOGIN_SCREEN)
    SetUISessionState(STATE_BROWSER_SCREEN);
}

std::unique_ptr<InputMethodDescriptors>
InputMethodManagerImpl::GetSupportedInputMethods() const {
  return std::unique_ptr<InputMethodDescriptors>(new InputMethodDescriptors);
}

const InputMethodDescriptor* InputMethodManagerImpl::LookupInputMethod(
    const std::string& input_method_id,
    InputMethodManagerImpl::StateImpl* state) {
  DCHECK(state);

  std::string input_method_id_to_switch = input_method_id;

  // Sanity check
  if (!state->InputMethodIsActivated(input_method_id)) {
    std::unique_ptr<InputMethodDescriptors> input_methods(
        state->GetActiveInputMethods());
    DCHECK(!input_methods->empty());
    input_method_id_to_switch = input_methods->at(0).id();
    if (!input_method_id.empty()) {
      DVLOG(1) << "Can't change the current input method to "
               << input_method_id << " since the engine is not enabled. "
               << "Switch to " << input_method_id_to_switch << " instead.";
    }
  }

  const InputMethodDescriptor* descriptor = NULL;
  if (extension_ime_util::IsExtensionIME(input_method_id_to_switch) ||
      extension_ime_util::IsArcIME(input_method_id_to_switch)) {
    DCHECK(state->extra_input_methods.find(input_method_id_to_switch) !=
           state->extra_input_methods.end());
    descriptor = &(state->extra_input_methods[input_method_id_to_switch]);
  } else {
    descriptor =
        util_.GetInputMethodDescriptorFromId(input_method_id_to_switch);
    if (!descriptor)
      LOG(ERROR) << "Unknown input method id: " << input_method_id_to_switch;
  }
  DCHECK(descriptor);
  return descriptor;
}

void InputMethodManagerImpl::ChangeInputMethodInternal(
    const InputMethodDescriptor& descriptor,
    Profile* profile,
    bool show_message,
    bool notify_menu) {
  // No need to switch input method when terminating.
  if (ui_session_ == STATE_TERMINATING) {
    VLOG(1) << "No need to switch input method when terminating.";
    return;
  }

  if (candidate_window_controller_.get())
    candidate_window_controller_->Hide();

  if (notify_menu) {
    // Clear property list.  Property list would be updated by
    // extension IMEs via IMEEngineHandlerInterface::(Set|Update)MenuItems.
    // If the current input method is a keyboard layout, empty
    // properties are sufficient.
    const ui::ime::InputMethodMenuItemList empty_menu_item_list;
    ui::ime::InputMethodMenuManager* input_method_menu_manager =
        ui::ime::InputMethodMenuManager::GetInstance();
    input_method_menu_manager->SetCurrentInputMethodMenuItemList(
            empty_menu_item_list);
  }

  // Disable the current engine handler.
  ui::IMEEngineHandlerInterface* engine =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();
  if (engine)
    engine->Disable();

  // Configure the next engine handler.
  // This must be after |current_input_method| has been set to new input
  // method, because engine's Enable() method needs to access it.
  const std::string& extension_id =
      extension_ime_util::GetExtensionIDFromInputMethodID(descriptor.id());
  const std::string& component_id =
      extension_ime_util::GetComponentIDByInputMethodID(descriptor.id());
  if (!engine_map_.count(profile) ||
      !engine_map_[profile].count(extension_id)) {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "IMEEngine for \"" << extension_id << "\" is not registered";
  }
  engine = engine_map_[profile][extension_id];

  ui::IMEBridge::Get()->SetCurrentEngineHandler(engine);

  if (engine) {
    engine->Enable(component_id);
  } else {
    // If no engine to enable, cancel the virtual keyboard url override so that
    // it can use the fallback system virtual keyboard UI.
    state_->DisableInputView();
    ReloadKeyboard();
  }

  // Change the keyboard layout to a preferred layout for the input method.
  if (!keyboard_->SetCurrentKeyboardLayoutByName(
          descriptor.GetPreferredKeyboardLayout())) {
    LOG(ERROR) << "Failed to change keyboard layout to "
               << descriptor.GetPreferredKeyboardLayout();
  }

  // Update input method indicators (e.g. "US", "DV") in Chrome windows.
  for (auto& observer : observers_)
    observer.InputMethodChanged(this, profile, show_message);
  // Update the current input method in IME menu.
  NotifyImeMenuListChanged();
}

void InputMethodManagerImpl::LoadNecessaryComponentExtensions(
    InputMethodManagerImpl::StateImpl* state) {
  // Load component extensions but also update |active_input_method_ids| as
  // some component extension IMEs may have been removed from the Chrome OS
  // image. If specified component extension IME no longer exists, falling back
  // to an existing IME.
  DCHECK(state);
  std::vector<std::string> unfiltered_input_method_ids;
  unfiltered_input_method_ids.swap(state->active_input_method_ids);
  for (const auto& unfiltered_input_method_id : unfiltered_input_method_ids) {
    if (!extension_ime_util::IsComponentExtensionIME(
            unfiltered_input_method_id)) {
      // Legacy IMEs or xkb layouts are alwayes active.
      state->active_input_method_ids.push_back(unfiltered_input_method_id);
    } else if (component_extension_ime_manager_->IsWhitelisted(
                   unfiltered_input_method_id)) {
      if (enable_extension_loading_) {
        component_extension_ime_manager_->LoadComponentExtensionIME(
            state->profile, unfiltered_input_method_id);
      }

      state->active_input_method_ids.push_back(unfiltered_input_method_id);
    }
  }
}

void InputMethodManagerImpl::ActivateInputMethodMenuItem(
    const std::string& key) {
  DCHECK(!key.empty());

  if (ui::ime::InputMethodMenuManager::GetInstance()->
      HasInputMethodMenuItemForKey(key)) {
    ui::IMEEngineHandlerInterface* engine =
        ui::IMEBridge::Get()->GetCurrentEngineHandler();
    if (engine)
      engine->PropertyActivate(key);
    return;
  }

  DVLOG(1) << "ActivateInputMethodMenuItem: unknown key: " << key;
}

void InputMethodManagerImpl::ConnectInputEngineManager(
    mojo::PendingReceiver<chromeos::ime::mojom::InputEngineManager> receiver) {
  DCHECK(state_);
  state_->ConnectMojoManager(std::move(receiver));
}

bool InputMethodManagerImpl::IsISOLevel5ShiftUsedByCurrentInputMethod() const {
  return keyboard_->IsISOLevel5ShiftAvailable();
}

bool InputMethodManagerImpl::IsAltGrUsedByCurrentInputMethod() const {
  return keyboard_->IsAltGrAvailable();
}

ImeKeyboard* InputMethodManagerImpl::GetImeKeyboard() {
  return keyboard_.get();
}

InputMethodUtil* InputMethodManagerImpl::GetInputMethodUtil() {
  return &util_;
}

ComponentExtensionIMEManager*
    InputMethodManagerImpl::GetComponentExtensionIMEManager() {
  return component_extension_ime_manager_.get();
}

scoped_refptr<InputMethodManager::State> InputMethodManagerImpl::CreateNewState(
    Profile* profile) {
  auto* new_state = new StateImpl(this, profile);

  // Active IM should be set to owner/user's default.
  PrefService* prefs = g_browser_process->local_state();
  PrefService* user_prefs = profile ? profile->GetPrefs() : nullptr;
  std::string initial_input_method_id;
  if (user_prefs) {
    initial_input_method_id =
        user_prefs->GetString(prefs::kLanguageCurrentInputMethod);
  }
  if (initial_input_method_id.empty()) {
    initial_input_method_id =
        prefs->GetString(chromeos::language_prefs::kPreferredKeyboardLayout);
  }

  const InputMethodDescriptor* descriptor =
      GetInputMethodUtil()->GetInputMethodDescriptorFromId(
          initial_input_method_id.empty()
              ? GetInputMethodUtil()->GetFallbackInputMethodDescriptor().id()
              : initial_input_method_id);
  if (descriptor) {
    new_state->active_input_method_ids.push_back(descriptor->id());
    new_state->current_input_method = *descriptor;
  }
  return scoped_refptr<InputMethodManager::State>(new_state);
}

void InputMethodManagerImpl::SetCandidateWindowControllerForTesting(
    CandidateWindowController* candidate_window_controller) {
  candidate_window_controller_.reset(candidate_window_controller);
  candidate_window_controller_->AddObserver(this);
}

void InputMethodManagerImpl::SetImeKeyboardForTesting(ImeKeyboard* keyboard) {
  keyboard_.reset(keyboard);
}

void InputMethodManagerImpl::InitializeComponentExtensionForTesting(
    std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate) {
  component_extension_ime_manager_->Initialize(std::move(delegate));
  util_.ResetInputMethods(
      component_extension_ime_manager_->GetAllIMEAsInputMethodDescriptor());
}

void InputMethodManagerImpl::CandidateClicked(int index) {
  ui::IMEEngineHandlerInterface* engine =
      ui::IMEBridge::Get()->GetCurrentEngineHandler();
  if (engine)
    engine->CandidateClicked(index);
}

void InputMethodManagerImpl::CandidateWindowOpened() {
  for (auto& observer : candidate_window_observers_)
    observer.CandidateWindowOpened(this);
}

void InputMethodManagerImpl::CandidateWindowClosed() {
  for (auto& observer : candidate_window_observers_)
    observer.CandidateWindowClosed(this);
}

void InputMethodManagerImpl::ImeMenuActivationChanged(bool is_active) {
  // Saves the state that whether the expanded IME menu has been activated by
  // users. This method is only called when the preference is changing.
  state_->menu_activated = is_active;
  MaybeNotifyImeMenuActivationChanged();
}

void InputMethodManagerImpl::NotifyInputMethodExtensionAdded(
    const std::string& extension_id) {
  for (auto& observer : observers_)
    observer.OnInputMethodExtensionAdded(extension_id);
}

void InputMethodManagerImpl::NotifyInputMethodExtensionRemoved(
    const std::string& extension_id) {
  for (auto& observer : observers_)
    observer.OnInputMethodExtensionRemoved(extension_id);
}

void InputMethodManagerImpl::NotifyImeMenuListChanged() {
  for (auto& observer : ime_menu_observers_)
    observer.ImeMenuListChanged();
}

void InputMethodManagerImpl::MaybeInitializeCandidateWindowController() {
  if (candidate_window_controller_.get())
    return;

  candidate_window_controller_.reset(
      CandidateWindowController::CreateCandidateWindowController());
  candidate_window_controller_->AddObserver(this);
}

void InputMethodManagerImpl::NotifyImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<InputMethodManager::MenuItem>& items) {
  for (auto& observer : ime_menu_observers_)
    observer.ImeMenuItemsChanged(engine_id, items);
}

void InputMethodManagerImpl::MaybeNotifyImeMenuActivationChanged() {
  if (is_ime_menu_activated_ == state_->menu_activated)
    return;

  is_ime_menu_activated_ = state_->menu_activated;
  for (auto& observer : ime_menu_observers_)
    observer.ImeMenuActivationChanged(is_ime_menu_activated_);
  UMA_HISTOGRAM_BOOLEAN("InputMethod.ImeMenu.ActivationChanged",
                        is_ime_menu_activated_);
}

void InputMethodManagerImpl::OverrideKeyboardKeyset(mojom::ImeKeyset keyset) {
  GURL url = state_->GetInputViewUrl();

  // If fails to find ref or tag "id" in the ref, it means the current IME is
  // not system IME, and we don't support show emoji, handwriting or voice
  // input for such IME extension.
  if (!url.has_ref())
    return;
  std::string overridden_ref = url.ref();

  auto i = overridden_ref.find("id=");
  if (i == std::string::npos)
    return;

  if (keyset == mojom::ImeKeyset::kNone) {
    // Resets the url as the input method default url and notify the hash
    // changed to VK.
    state_->input_view_url = state_->current_input_method.input_view_url();
    ReloadKeyboard();
    return;
  }

  // For system IME extension, the input view url is overridden as:
  // chrome-extension://${extension_id}/inputview.html#id=us.compact.qwerty
  // &language=en-US&passwordLayout=us.compact.qwerty&name=keyboard_us
  // Fow emoji, handwriting and voice input, we append the keyset to the end of
  // id like: id=${keyset}.emoji/hwt/voice.
  auto j = overridden_ref.find("&", i + 1);
  if (j == std::string::npos) {
    overridden_ref += "." + KeysetToString(keyset);
  } else {
    overridden_ref.replace(j, 0, "." + KeysetToString(keyset));
  }

  GURL::Replacements replacements;
  replacements.SetRefStr(overridden_ref);
  state_->input_view_url = url.ReplaceComponents(replacements);

  ReloadKeyboard();
}

void InputMethodManagerImpl::SetImeMenuFeatureEnabled(ImeMenuFeature feature,
                                                      bool enabled) {
  const uint32_t original_state = features_enabled_state_;
  if (enabled)
    features_enabled_state_ |= feature;
  else
    features_enabled_state_ &= ~feature;
  if (original_state != features_enabled_state_)
    NotifyObserversImeExtraInputStateChange();
}

bool InputMethodManagerImpl::GetImeMenuFeatureEnabled(
    ImeMenuFeature feature) const {
  return features_enabled_state_ & feature;
}

void InputMethodManagerImpl::NotifyObserversImeExtraInputStateChange() {
  for (auto& observer : observers_) {
    const bool is_emoji_enabled =
        (features_enabled_state_ & InputMethodManager::FEATURE_EMOJI);
    const bool is_handwriting_enabled =
        (features_enabled_state_ & InputMethodManager::FEATURE_HANDWRITING);
    const bool is_voice_enabled =
        (features_enabled_state_ & InputMethodManager::FEATURE_VOICE);
    observer.OnExtraInputEnabledStateChange(
        true, is_emoji_enabled, is_handwriting_enabled, is_voice_enabled);
  }
}

ui::InputMethodKeyboardController*
InputMethodManagerImpl::GetInputMethodKeyboardController() {
  // Callers expect a nullptr when the keyboard is disabled. See
  // https://crbug.com/850020.
  if (!keyboard::KeyboardUIController::HasInstance() ||
      !keyboard::KeyboardUIController::Get()->IsEnabled()) {
    return nullptr;
  }
  return keyboard::KeyboardUIController::Get()
      ->input_method_keyboard_controller();
}

void InputMethodManagerImpl::ReloadKeyboard() {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_enabled())
    keyboard_client->ReloadKeyboardIfNeeded();
}

}  // namespace input_method
}  // namespace chromeos
