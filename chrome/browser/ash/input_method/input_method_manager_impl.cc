// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/input_method/input_method_manager_impl.h"

#include <stdint.h>

#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/i18n/string_compare.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/input_method/assistive_window_controller.h"
#include "chrome/browser/ash/input_method/candidate_window_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_item.h"
#include "chrome/browser/ui/ash/input_method/input_method_menu_manager.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/language_preferences/language_preferences.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/component_extension_ime_manager_delegate.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/ime_keyboard_impl.h"
#include "ui/base/ime/ash/input_method_delegate.h"
#include "ui/base/ui_base_features.h"

namespace ash {
namespace input_method {

namespace {

const char* const kNonPositionalLayouts[] = {
    "de(neo)",    "gb(dvorak)", "tr(f)",       "us(colemak)",
    "us(dvorak)", "us(dvp)",    "us(workman)", "us(workman-intl)",
};

const size_t kNonPositionalLayoutsLength = std::size(kNonPositionalLayouts);

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

const ImeKeyset kKeysets[] = {ImeKeyset::kEmoji, ImeKeyset::kHandwriting,
                              ImeKeyset::kVoice};

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
                              base::CompareCase::SENSITIVE) ||
             component_id == "ko-t-i0-und") {
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

std::string KeysetToString(ImeKeyset keyset) {
  switch (keyset) {
    case ImeKeyset::kNone:
      return "";
    case ImeKeyset::kEmoji:
      return "emoji";
    case ImeKeyset::kHandwriting:
      return "hwt";
    case ImeKeyset::kVoice:
      return "voice";
  }
}

bool IsShuttingDown() {
  return !g_browser_process || g_browser_process->IsShuttingDown();
}

}  // namespace

// ------------------------ InputMethodManagerImpl::StateImpl

InputMethodManagerImpl::StateImpl::StateImpl(
    InputMethodManagerImpl* manager,
    Profile* profile,
    const InputMethodDescriptor* initial_input_method)
    : profile_(profile), manager_(manager) {
  if (initial_input_method) {
    enabled_input_method_ids_.push_back(initial_input_method->id());
    current_input_method_ = *initial_input_method;
  }
}

InputMethodManagerImpl::StateImpl::~StateImpl() = default;

bool InputMethodManagerImpl::StateImpl::IsActive() const {
  return manager_->state_.get() == this;
}

scoped_refptr<InputMethodManager::State>
InputMethodManagerImpl::StateImpl::Clone() const {
  scoped_refptr<StateImpl> new_state(new StateImpl(manager_, profile_));

  new_state->last_used_input_method_id_ = last_used_input_method_id_;
  new_state->current_input_method_ = current_input_method_;

  new_state->enabled_input_method_ids_ = enabled_input_method_ids_;
  new_state->allowed_keyboard_layout_input_method_ids_ =
      allowed_keyboard_layout_input_method_ids_;

  new_state->pending_input_method_id_ = pending_input_method_id_;

  new_state->enabled_extension_imes_ = enabled_extension_imes_;
  new_state->available_input_methods_ = available_input_methods_;
  new_state->menu_activated_ = menu_activated_;
  new_state->input_view_url_ = input_view_url_;
  new_state->input_view_url_overridden_ = input_view_url_overridden_;
  new_state->ui_style_ = ui_style_;

  return scoped_refptr<InputMethodManager::State>(new_state.get());
}

InputMethodDescriptors
InputMethodManagerImpl::StateImpl::GetEnabledInputMethods() const {
  InputMethodDescriptors result;
  // Build the enabled input method descriptors from the enabled input
  // methods cache |enabled_input_method_ids_|.
  for (const auto& input_method_id : enabled_input_method_ids_) {
    const InputMethodDescriptor* descriptor =
        manager_->util_.GetInputMethodDescriptorFromId(input_method_id);
    if (descriptor) {
      result.push_back(*descriptor);
    } else {
      const auto ix = available_input_methods_.find(input_method_id);
      if (ix != available_input_methods_.end()) {
        result.push_back(ix->second);
      } else {
        DVLOG(1) << "Descriptor is not found for: " << input_method_id;
      }
    }
  }
  if (result.empty()) {
    // Initially |enabled_input_method_ids_| is empty. browser_tests might take
    // this path.
    result.push_back(InputMethodUtil::GetFallbackInputMethodDescriptor());
  }

  return result;
}

InputMethodDescriptors InputMethodManagerImpl::StateImpl::
    GetEnabledInputMethodsSortedByLocalizedDisplayNames() const {
  InputMethodDescriptors result = GetEnabledInputMethods();

  UErrorCode error_code = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));  // use current ICU locale
  DCHECK(U_SUCCESS(error_code));
  const InputMethodUtil& util = manager_->util_;

  std::sort(
      result.begin(), result.end(),
      [&collator, &util](const InputMethodDescriptor& a,
                         const InputMethodDescriptor& b) {
        std::u16string a16 = base::UTF8ToUTF16(util.GetLocalizedDisplayName(a));
        std::u16string b16 = base::UTF8ToUTF16(util.GetLocalizedDisplayName(b));
        return base::i18n::CompareString16WithCollator(*collator, a16, b16) < 0;
      });

  return result;
}

const std::vector<std::string>&
InputMethodManagerImpl::StateImpl::GetEnabledInputMethodIds() const {
  return enabled_input_method_ids_;
}

size_t InputMethodManagerImpl::StateImpl::GetNumEnabledInputMethods() const {
  return enabled_input_method_ids_.size();
}

const InputMethodDescriptor*
InputMethodManagerImpl::StateImpl::GetInputMethodFromId(
    const std::string& input_method_id) const {
  const InputMethodDescriptor* ime =
      manager_->util_.GetInputMethodDescriptorFromId(input_method_id);
  if (!ime) {
    const auto ix = available_input_methods_.find(input_method_id);
    if (ix != available_input_methods_.end()) {
      ime = &ix->second;
    }
  }
  return ime;
}

void InputMethodManagerImpl::StateImpl::EnableLoginLayouts(
    const std::string& language_code,
    const std::vector<std::string>& initial_layouts) {
  if (IsShuttingDown()) {
    return;
  }

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
  // layouts, so it appears first on the list of enabled input
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

  manager_->GetMigratedInputMethodIDs(&layouts);
  enabled_input_method_ids_.swap(layouts);

  if (IsActive()) {
    // Initialize candidate window controller and widgets such as
    // candidate window, infolist and mode indicator.  Note, mode
    // indicator is used by only keyboard layout input methods.
    if (enabled_input_method_ids_.size() > 1) {
      manager_->MaybeInitializeCandidateWindowController();
    }

    // you can pass empty |initial_layout|.
    ChangeInputMethod(initial_layouts.empty()
                          ? std::string()
                          : extension_ime_util::GetInputMethodIDByEngineID(
                                initial_layouts[0]),
                      false);
  }
}

void InputMethodManagerImpl::StateImpl::DisableNonLockScreenLayouts() {
  std::set<std::string> added_ids;

  const std::vector<std::string>& hardware_keyboard_ids =
      manager_->util_.GetHardwareLoginInputMethodIds();

  std::vector<std::string> new_enabled_input_method_ids;
  for (const auto& input_method_id : enabled_input_method_ids_) {
    // Skip if it's not a keyboard layout. Drop input methods including
    // extension ones. We need to keep all IMEs to support inputting on inline
    // reply on a notification if notifications on lock screen is enabled.
    if ((!ash::features::IsLockScreenInlineReplyEnabled() &&
         !manager_->IsLoginKeyboard(input_method_id)) ||
        added_ids.count(input_method_id)) {
      continue;
    }
    new_enabled_input_method_ids.push_back(input_method_id);
    added_ids.insert(input_method_id);
  }

  // We'll add the hardware keyboard if it's not included in
  // |enabled_input_method_ids_| so that the user can always use the hardware
  // keyboard on the screen locker.
  for (const auto& hardware_keyboard_id : hardware_keyboard_ids) {
    if (added_ids.count(hardware_keyboard_id)) {
      continue;
    }
    new_enabled_input_method_ids.push_back(hardware_keyboard_id);
    added_ids.insert(hardware_keyboard_id);
  }

  enabled_input_method_ids_.swap(new_enabled_input_method_ids);

  // Re-check current_input_method_.
  ChangeInputMethod(current_input_method_.id(), false);
}

// Adds new input method to given list.
bool InputMethodManagerImpl::StateImpl::EnableInputMethodImpl(
    const std::string& input_method_id,
    std::vector<std::string>* new_enabled_input_method_ids) const {
  if (!IsInputMethodAllowed(input_method_id)) {
    DVLOG(1) << "EnableInputMethod: " << input_method_id << " is not allowed.";
    return false;
  }

  DCHECK(new_enabled_input_method_ids);
  if (!manager_->util_.IsValidInputMethodId(input_method_id)) {
    DVLOG(1) << "EnableInputMethod: Invalid ID: " << input_method_id;
    return false;
  }

  if (!base::Contains(*new_enabled_input_method_ids, input_method_id)) {
    new_enabled_input_method_ids->push_back(input_method_id);
  }

  return true;
}

bool InputMethodManagerImpl::StateImpl::EnableInputMethod(
    const std::string& input_method_id) {
  if (!EnableInputMethodImpl(input_method_id, &enabled_input_method_ids_)) {
    return false;
  }

  manager_->ReconfigureIMFramework(this);
  return true;
}

bool InputMethodManagerImpl::StateImpl::ReplaceEnabledInputMethods(
    const std::vector<std::string>& new_enabled_input_method_ids) {
  if (IsShuttingDown()) {
    return false;
  }

  // Filter unknown or obsolete IDs.
  std::vector<std::string> new_enabled_input_method_ids_filtered;

  for (const auto& new_enabled_input_method_id : new_enabled_input_method_ids) {
    EnableInputMethodImpl(new_enabled_input_method_id,
                          &new_enabled_input_method_ids_filtered);
  }

  if (new_enabled_input_method_ids_filtered.empty()) {
    DVLOG(1) << "ReplaceEnabledInputMethods: No valid input method ID";
    return false;
  }

  // Copy extension IDs to |new_enabled_input_method_ids_filtered|. We have to
  // keep relative order of the extension input method IDs.
  for (const auto& input_method_id : enabled_input_method_ids_) {
    if (extension_ime_util::IsExtensionIME(input_method_id)) {
      new_enabled_input_method_ids_filtered.push_back(input_method_id);
    }
  }
  enabled_input_method_ids_.swap(new_enabled_input_method_ids_filtered);
  manager_->GetMigratedInputMethodIDs(&enabled_input_method_ids_);

  manager_->ReconfigureIMFramework(this);

  // If |current_input_method_| is no longer in |enabled_input_method_ids_|,
  // ChangeInputMethod() picks the first one in |enabled_input_method_ids_|.
  ChangeInputMethod(current_input_method_.id(), false);

  // Record histogram for enabled input method count; "active" in the metric
  // name is a legacy misnomer; "active" should refer to just the single current
  // aka. activated input method that's one of the enabled input methods whose
  // total count is being tracked by this metric.
  UMA_HISTOGRAM_COUNTS_1M("InputMethod.ActiveCount",
                          enabled_input_method_ids_.size());

  return true;
}

bool InputMethodManagerImpl::StateImpl::SetAllowedInputMethods(
    const std::vector<std::string>& new_allowed_input_method_ids) {
  allowed_keyboard_layout_input_method_ids_.clear();
  for (auto input_method_id : new_allowed_input_method_ids) {
    std::string migrated_id =
        manager_->util_.GetMigratedInputMethod(input_method_id);
    if (manager_->util_.IsValidInputMethodId(migrated_id)) {
      allowed_keyboard_layout_input_method_ids_.push_back(migrated_id);
      // Kiosk users are not able to go to the settings and manually enable
      // allowed input methods, thus it has to be done automatically for
      // non-empty list.
      DCHECK(user_manager::UserManager::Get());
      if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
        EnableInputMethod(migrated_id);
      }
    }
  }

  if (allowed_keyboard_layout_input_method_ids_.empty()) {
    // None of the passed input methods were valid, so allow everything.
    return false;
  }
  return true;
}

const std::vector<std::string>&
InputMethodManagerImpl::StateImpl::GetAllowedInputMethodIds() const {
  return allowed_keyboard_layout_input_method_ids_;
}

bool InputMethodManagerImpl::StateImpl::IsInputMethodAllowed(
    const std::string& input_method_id) const {
  // Every input method is allowed if SetAllowedKeyboardLayoutInputMethods has
  // not been called.
  if (allowed_keyboard_layout_input_method_ids_.empty()) {
    return true;
  }

  return base::Contains(allowed_keyboard_layout_input_method_ids_,
                        input_method_id) ||
         base::Contains(
             allowed_keyboard_layout_input_method_ids_,
             manager_->util_.GetMigratedInputMethod(input_method_id));
}

std::string
InputMethodManagerImpl::StateImpl::GetAllowedFallBackKeyboardLayout() const {
  for (const std::string& hardware_id :
       manager_->util_.GetHardwareInputMethodIds()) {
    if (IsInputMethodAllowed(hardware_id)) {
      return hardware_id;
    }
  }
  return allowed_keyboard_layout_input_method_ids_[0];
}

void InputMethodManagerImpl::StateImpl::ChangeInputMethod(
    const std::string& input_method_id,
    bool show_message) {
  if (IsShuttingDown()) {
    return;
  }

  bool notify_menu = false;

  // Always lookup input method, even if it is the same as
  // |current_input_method_| because If it is no longer in
  // |enabled_input_method_ids_|, pick the first one in
  // |enabled_input_method_ids_|.
  const InputMethodDescriptor* descriptor = LookupInputMethod(input_method_id);
  if (!descriptor) {
    descriptor = LookupInputMethod(
        manager_->util_.GetMigratedInputMethod(input_method_id));
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
  if (!InputMethodIsEnabled(input_method_id) &&
      extension_ime_util::IsExtensionIME(input_method_id)) {
    pending_input_method_id_ = input_method_id;
  }

  if (descriptor->id() != current_input_method_.id()) {
    last_used_input_method_id_ = current_input_method_.id();
    current_input_method_ = *descriptor;
    notify_menu = true;
  }

  // Always change input method even if it is the same.
  // TODO(komatsu): Revisit if this is necessary.
  if (IsActive()) {
    manager_->ChangeInputMethodInternalFromActiveState(show_message,
                                                       notify_menu);
  }

  manager_->RecordInputMethodUsage(current_input_method_.id());
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

void InputMethodManagerImpl::StateImpl::AddInputMethodExtension(
    const std::string& extension_id,
    const InputMethodDescriptors& descriptors,
    TextInputMethod* engine) {
  if (IsShuttingDown()) {
    return;
  }

  DCHECK(engine);

  manager_->engine_map_[profile_][extension_id] = engine;
  VLOG(1) << "Add an engine for \"" << extension_id << "\"";

  bool contain = false;
  for (const auto& descriptor : descriptors) {
    const std::string& id = descriptor.id();
    available_input_methods_[id] = descriptor;
    if (base::Contains(enabled_extension_imes_, id)) {
      if (!base::Contains(enabled_input_method_ids_, id)) {
        enabled_input_method_ids_.push_back(id);
      } else {
        DVLOG(1) << "AddInputMethodExtension: already added: " << id << ", "
                 << descriptor.name();
      }
      contain = true;
    }
  }

  if (IsActive()) {
    if (extension_id == extension_ime_util::GetExtensionIDFromInputMethodID(
                            current_input_method_.id())) {
      IMEBridge::Get()->SetCurrentEngineHandler(engine);
      engine->Enable(extension_ime_util::GetComponentIDByInputMethodID(
          current_input_method_.id()));
    }

    // Ensure that the input method daemon is running.
    if (contain) {
      manager_->MaybeInitializeCandidateWindowController();
    }
  }

  manager_->NotifyImeMenuListChanged();
  manager_->NotifyInputMethodExtensionAdded(extension_id);
}

void InputMethodManagerImpl::StateImpl::RemoveInputMethodExtension(
    const std::string& extension_id) {
  // Remove the enabled input methods with |extension_id|.
  std::vector<std::string> new_enabled_input_method_ids;
  for (const auto& enabled_input_method_id : enabled_input_method_ids_) {
    if (extension_id != extension_ime_util::GetExtensionIDFromInputMethodID(
                            enabled_input_method_id)) {
      new_enabled_input_method_ids.push_back(enabled_input_method_id);
    }
  }
  enabled_input_method_ids_.swap(new_enabled_input_method_ids);

  // Remove the input methods registered by `extension_id`.
  std::map<std::string, InputMethodDescriptor> new_available_input_methods;
  for (const auto& entry : available_input_methods_) {
    if (extension_id !=
        extension_ime_util::GetExtensionIDFromInputMethodID(entry.first)) {
      new_available_input_methods[entry.first] = entry.second;
    }
  }
  available_input_methods_.swap(new_available_input_methods);

  if (IsActive()) {
    if (IMEBridge::Get()->GetCurrentEngineHandler() ==
        manager_->engine_map_[profile_][extension_id]) {
      IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
    }
    manager_->engine_map_[profile_].erase(extension_id);
  }

  // If |current_input_method_| is no longer in |enabled_input_method_ids_|,
  // switch to the first one in |enabled_input_method_ids_|.
  ChangeInputMethod(current_input_method_.id(), false);
  manager_->NotifyInputMethodExtensionRemoved(extension_id);
}

void InputMethodManagerImpl::StateImpl::GetInputMethodExtensions(
    InputMethodDescriptors* result) {
  // Build the extension input method descriptors from the input methods cache
  // `available_input_methods_`.
  for (const auto& entry : available_input_methods_) {
    if (extension_ime_util::IsExtensionIME(entry.first) ||
        extension_ime_util::IsArcIME(entry.first)) {
      result->push_back(entry.second);
    }
  }
}

void InputMethodManagerImpl::StateImpl::SetEnabledExtensionImes(
    base::span<const std::string> ids) {
  enabled_extension_imes_.clear();
  enabled_extension_imes_.insert(enabled_extension_imes_.end(), ids.begin(),
                                 ids.end());
  bool enabled_imes_changed = false;
  bool switch_to_pending = false;

  for (const auto& entry : available_input_methods_) {
    if (extension_ime_util::IsComponentExtensionIME(entry.first)) {
      continue;  // Do not filter component extension.
    }

    if (pending_input_method_id_ == entry.first) {
      switch_to_pending = true;
    }

    const auto currently_enabled_iter =
        base::ranges::find(enabled_input_method_ids_, entry.first);

    bool currently_enabled =
        currently_enabled_iter != enabled_input_method_ids_.end();
    bool enabled = base::Contains(enabled_extension_imes_, entry.first);

    if (currently_enabled && !enabled) {
      enabled_input_method_ids_.erase(currently_enabled_iter);
    }

    if (!currently_enabled && enabled) {
      enabled_input_method_ids_.push_back(entry.first);
    }

    if (currently_enabled == !enabled) {
      enabled_imes_changed = true;
    }
  }

  if (IsActive() && enabled_imes_changed) {
    manager_->MaybeInitializeCandidateWindowController();

    if (switch_to_pending) {
      ChangeInputMethod(pending_input_method_id_, false);
      pending_input_method_id_.clear();
    } else {
      // If |current_input_method_| is no longer in |enabled_input_method_ids_|,
      // switch to the first one in |enabled_input_method_ids_|.
      ChangeInputMethod(current_input_method_.id(), false);
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
            locale, kKeyboardLayoutsOnly, &input_method_ids)) {
      // The output list |input_method_ids| is sorted by popularity, hence
      // input_method_ids[0] now contains the most popular keyboard layout
      // for the given locale.
      DCHECK_GE(input_method_ids.size(), 1U);
      layout = input_method_ids[0];
    }
  }

  if (layout.empty()) {
    return;
  }

  std::vector<std::string> layouts = base::SplitString(
      layout, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  manager_->GetMigratedInputMethodIDs(&layouts);

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
  LoadNecessaryComponentExtensions();
}

void InputMethodManagerImpl::StateImpl::SetInputMethodLoginDefault() {
  // Set up keyboards. For example, when |locale| is "en-US", enable US qwerty
  // and US dvorak keyboard layouts.
  if (g_browser_process && g_browser_process->local_state()) {
    const std::string locale = g_browser_process->GetApplicationLocale();
    std::vector<std::string> input_method_ids_to_be_enabled;
    if (!GetAllowedInputMethodIds().empty()) {
      // Prefer policy-set input methods.
      input_method_ids_to_be_enabled = GetAllowedInputMethodIds();
    } else {
      // If the preferred keyboard for the login screen has been saved, use it.
      PrefService* prefs = g_browser_process->local_state();
      std::string initial_input_method_id =
          prefs->GetString(language_prefs::kPreferredKeyboardLayout);
      if (initial_input_method_id.empty()) {
        // If kPreferredKeyboardLayout is not specified, use the hardware
        // layout.
        input_method_ids_to_be_enabled =
            manager_->util_.GetHardwareLoginInputMethodIds();
      } else {
        input_method_ids_to_be_enabled.push_back(initial_input_method_id);
      }
    }
    EnableLoginLayouts(locale, input_method_ids_to_be_enabled);
    LoadNecessaryComponentExtensions();
  }
}

void InputMethodManagerImpl::StateImpl::SwitchToNextInputMethod() {
  if (enabled_input_method_ids_.size() <= 1 ||
      current_input_method_.id().empty()) {
    return;
  }

  const std::string& current_input_method_id = current_input_method_.id();
  InputMethodDescriptors sorted_enabled_input_methods =
      GetEnabledInputMethodsSortedByLocalizedDisplayNames();

  auto iter =
      base::ranges::find(sorted_enabled_input_methods, current_input_method_id,
                         &InputMethodDescriptor::id);

  if (iter != sorted_enabled_input_methods.end()) {
    ++iter;
  }
  if (iter == sorted_enabled_input_methods.end()) {
    iter = sorted_enabled_input_methods.begin();
  }

  ChangeInputMethod(iter->id(), true);
}

void InputMethodManagerImpl::StateImpl::SwitchToLastUsedInputMethod() {
  if (enabled_input_method_ids_.size() <= 1 ||
      current_input_method_.id().empty()) {
    return;
  }

  if (last_used_input_method_id_.empty() ||
      last_used_input_method_id_ == current_input_method_.id()) {
    SwitchToNextInputMethod();
    return;
  }

  const auto iter =
      base::ranges::find(enabled_input_method_ids_, last_used_input_method_id_);
  if (iter == enabled_input_method_ids_.end()) {
    // last_used_input_method_id_ is not supported.
    SwitchToNextInputMethod();
    return;
  }
  ChangeInputMethod(*iter, true);
}

InputMethodDescriptor InputMethodManagerImpl::StateImpl::GetCurrentInputMethod()
    const {
  if (current_input_method_.id().empty()) {
    return InputMethodUtil::GetFallbackInputMethodDescriptor();
  }

  return current_input_method_;
}

bool InputMethodManagerImpl::StateImpl::InputMethodIsEnabled(
    const std::string& input_method_id) const {
  return base::Contains(enabled_input_method_ids_, input_method_id);
}

void InputMethodManagerImpl::StateImpl::EnableInputView() {
  if (!input_view_url_overridden_) {
    input_view_url_ = current_input_method_.input_view_url();
  }
}

void InputMethodManagerImpl::StateImpl::DisableInputView() {
  input_view_url_ = GURL();
}

const GURL& InputMethodManagerImpl::StateImpl::GetInputViewUrl() const {
  return input_view_url_;
}

InputMethodManager::UIStyle InputMethodManagerImpl::StateImpl::GetUIStyle()
    const {
  return ui_style_;
}

void InputMethodManagerImpl::StateImpl::SetUIStyle(
    InputMethodManager::UIStyle ui_style) {
  ui_style_ = ui_style;
}

void InputMethodManagerImpl::StateImpl::OverrideInputViewUrl(const GURL& url) {
  input_view_url_ = url;
  input_view_url_overridden_ = true;
}

void InputMethodManagerImpl::StateImpl::ResetInputViewUrl() {
  input_view_url_ = current_input_method_.input_view_url();
  input_view_url_overridden_ = false;
}

void InputMethodManagerImpl::StateImpl::LoadNecessaryComponentExtensions() {
  // Load component extensions but also update |enabled_input_method_ids_| as
  // some component extension IMEs may have been removed from the Chrome OS
  // image. If specified component extension IME no longer exists, falling back
  // to an existing IME.
  TRACE_EVENT0(
      "ime",
      "InputMethodManagerImpl::StateImpl::LoadNecessaryComponentExtensions");
  std::vector<std::string> unfiltered_input_method_ids;
  unfiltered_input_method_ids.swap(enabled_input_method_ids_);
  std::set<std::string> ext_loaded;
  for (const auto& unfiltered_input_method_id : unfiltered_input_method_ids) {
    if (!extension_ime_util::IsComponentExtensionIME(
            unfiltered_input_method_id)) {
      // Legacy IMEs or xkb layouts are alwayes enabled.
      enabled_input_method_ids_.push_back(unfiltered_input_method_id);
    } else if (manager_->component_extension_ime_manager_->IsAllowlisted(
                   unfiltered_input_method_id)) {
      if (manager_->enable_extension_loading_) {
        manager_->component_extension_ime_manager_->LoadComponentExtensionIME(
            profile_, unfiltered_input_method_id, &ext_loaded);
      }

      enabled_input_method_ids_.push_back(unfiltered_input_method_id);
    }
  }
}

const InputMethodDescriptor*
InputMethodManagerImpl::StateImpl::LookupInputMethod(
    const std::string& input_method_id) {
  std::string input_method_id_to_switch = input_method_id;

  // Sanity check
  if (!InputMethodIsEnabled(input_method_id)) {
    InputMethodDescriptors input_methods(GetEnabledInputMethods());
    DCHECK(!input_methods.empty());
    input_method_id_to_switch = input_methods.at(0).id();
    if (!input_method_id.empty()) {
      DVLOG(1) << "Can't change the current input method to " << input_method_id
               << " since the engine is not enabled. " << "Switch to "
               << input_method_id_to_switch << " instead.";
    }
  }

  const InputMethodDescriptor* descriptor = nullptr;
  if (extension_ime_util::IsExtensionIME(input_method_id_to_switch) ||
      extension_ime_util::IsArcIME(input_method_id_to_switch)) {
    DCHECK(available_input_methods_.find(input_method_id_to_switch) !=
           available_input_methods_.end());
    descriptor = &(available_input_methods_[input_method_id_to_switch]);
  } else {
    descriptor = manager_->util_.GetInputMethodDescriptorFromId(
        input_method_id_to_switch);
    if (!descriptor) {
      LOG(ERROR) << "Unknown input method id: " << input_method_id_to_switch;
    }
  }
  DCHECK(descriptor);
  return descriptor;
}

Profile* InputMethodManagerImpl::StateImpl::GetProfile() const {
  return profile_;
}

void InputMethodManagerImpl::StateImpl::SetMenuActivated(bool activated) {
  menu_activated_ = activated;
}

bool InputMethodManagerImpl::StateImpl::IsMenuActivated() const {
  return menu_activated_;
}

// ------------------------ InputMethodManagerImpl
bool InputMethodManagerImpl::IsLoginKeyboard(const std::string& layout) const {
  return util_.IsLoginKeyboard(layout);
}

std::string InputMethodManagerImpl::GetMigratedInputMethodID(
    const std::string& input_method_id) {
  return util_.GetMigratedInputMethod(input_method_id);
}

bool InputMethodManagerImpl::GetMigratedInputMethodIDs(
    std::vector<std::string>* input_method_ids) {
  return util_.GetMigratedInputMethodIDs(input_method_ids);
}

// Starts or stops the system input method framework as needed.
void InputMethodManagerImpl::ReconfigureIMFramework(
    InputMethodManagerImpl::StateImpl* state) {
  DCHECK(state);
  state->LoadNecessaryComponentExtensions();

  // Initialize candidate window controller and widgets such as
  // candidate window, infolist and mode indicator.  Note, mode
  // indicator is used by only keyboard layout input methods.
  if (state_.get() == state) {
    MaybeInitializeCandidateWindowController();
    MaybeInitializeAssistiveWindowController();
  }
}

void InputMethodManagerImpl::SetState(
    scoped_refptr<InputMethodManager::State> state) {
  DCHECK(state.get());
  auto* new_impl_state =
      static_cast<InputMethodManagerImpl::StateImpl*>(state.get());

  state_ = new_impl_state;

  if (state_.get() && state_->GetNumEnabledInputMethods()) {
    // Initialize candidate window controller and widgets such as
    // candidate window, infolist and mode indicator.  Note, mode
    // indicator is used by only keyboard layout input methods.
    MaybeInitializeCandidateWindowController();
    MaybeInitializeAssistiveWindowController();

    // Always call ChangeInputMethodInternalFromActiveState even when the input
    // method id remain unchanged, because onActivate event needs to be sent to
    // IME extension to update the current screen type correctly.
    ChangeInputMethodInternalFromActiveState(false /* show_message */,
                                             true /* notify_menu */);
  }
}

scoped_refptr<InputMethodManager::State>
InputMethodManagerImpl::GetActiveIMEState() {
  return scoped_refptr<InputMethodManager::State>(state_.get());
}

InputMethodManagerImpl::InputMethodManagerImpl(
    std::unique_ptr<InputMethodDelegate> delegate,
    std::unique_ptr<ComponentExtensionIMEManagerDelegate>
        component_extension_ime_manager_delegate,
    bool enable_extension_loading,
    std::unique_ptr<ImeKeyboard> ime_keyboard)
    : delegate_(std::move(delegate)),
      util_(delegate_.get()),
      keyboard_(std::move(ime_keyboard)),
      enable_extension_loading_(enable_extension_loading),
      features_enabled_state_(InputMethodManager::FEATURE_ALL) {
  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    // Create a set of layouts that do not use positional shortcuts.
    non_positional_layouts_.reserve(kNonPositionalLayoutsLength);
    for (size_t i = 0; i < kNonPositionalLayoutsLength; i++) {
      non_positional_layouts_.emplace(kNonPositionalLayouts[i]);
    }
  }

  // Initializes the system IME list.
  component_extension_ime_manager_ =
      std::make_unique<ComponentExtensionIMEManager>(
          std::move(component_extension_ime_manager_delegate));
  const InputMethodDescriptors& descriptors =
      component_extension_ime_manager_->GetAllIMEAsInputMethodDescriptor();
  util_.ResetInputMethods(descriptors);

  // We should not use ALL_BROWSERS_CLOSING here since logout might be cancelled
  // by JavaScript after ALL_BROWSERS_CLOSING is sent (crosbug.com/11055).
  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(base::BindOnce(
          &InputMethodManagerImpl::OnAppTerminating, base::Unretained(this)));
}

InputMethodManagerImpl::~InputMethodManagerImpl() {
  if (candidate_window_controller_.get()) {
    candidate_window_controller_->RemoveObserver(this);
  }
}

void InputMethodManagerImpl::RecordInputMethodUsage(
    const std::string& input_method_id) {
  UMA_HISTOGRAM_ENUMERATION("InputMethod.Category",
                            GetInputMethodCategory(input_method_id),
                            INPUT_METHOD_CATEGORY_MAX);
  base::UmaHistogramSparse(
      "InputMethod.ID2",
      static_cast<int32_t>(base::PersistentHash(input_method_id)));
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

void InputMethodManagerImpl::ChangeInputMethodInternalFromActiveState(
    bool show_message,
    bool notify_menu) {
  // No need to switch input method when terminating.
  if (IsShuttingDown()) {
    VLOG(1) << "No need to switch input method when terminating.";
    return;
  }

  if (candidate_window_controller_.get()) {
    candidate_window_controller_->Hide();
  }

  if (notify_menu) {
    // Clear property list.  Property list would be updated by
    // extension IMEs via TextInputMethod::(Set|Update)MenuItems.
    // If the current input method is a keyboard layout, empty
    // properties are sufficient.
    const ui::ime::InputMethodMenuItemList empty_menu_item_list;
    ui::ime::InputMethodMenuManager* input_method_menu_manager =
        ui::ime::InputMethodMenuManager::GetInstance();
    input_method_menu_manager->SetCurrentInputMethodMenuItemList(
        empty_menu_item_list);
  }

  // Disable the current engine handler.
  TextInputMethod* engine = IMEBridge::Get()->GetCurrentEngineHandler();
  if (engine) {
    engine->Disable();
  }

  // Configure the next engine handler.
  // This must be after |current_input_method_| has been set to new input
  // method, because engine's Enable() method needs to access it.
  const std::string& extension_id =
      extension_ime_util::GetExtensionIDFromInputMethodID(
          state_->GetCurrentInputMethod().id());
  const std::string& component_id =
      extension_ime_util::GetComponentIDByInputMethodID(
          state_->GetCurrentInputMethod().id());
  if (!engine_map_.count(state_->GetProfile()) ||
      !engine_map_[state_->GetProfile()].count(extension_id)) {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "IMEEngine for \"" << extension_id << "\" is not registered";
  }
  engine = engine_map_[state_->GetProfile()][extension_id];

  IMEBridge::Get()->SetCurrentEngineHandler(engine);

  if (engine) {
    engine->Enable(component_id);
  } else {
    // If no engine to enable, cancel the virtual keyboard url override so that
    // it can use the fallback system virtual keyboard UI.
    state_->DisableInputView();
    ReloadKeyboard();
  }

  // Change the keyboard layout to a preferred layout for the input method.
  keyboard_->SetCurrentKeyboardLayoutByName(
      state_->GetCurrentInputMethod().keyboard_layout(),
      base::BindOnce(&InputMethodManagerImpl::NotifyInputMethodChanged,
                     base::Unretained(this), show_message));

  // Update the current input method in IME menu.
  NotifyImeMenuListChanged();
}

void InputMethodManagerImpl::NotifyInputMethodChanged(bool show_message,
                                                      bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to change keyboard layout to "
               << state_->GetCurrentInputMethod().keyboard_layout();
  }

  // Update input method indicators (e.g. "US", "DV") in Chrome windows.
  for (auto& observer : observers_) {
    observer.InputMethodChanged(this, state_->GetProfile(), show_message);
  }
}

void InputMethodManagerImpl::ActivateInputMethodMenuItem(
    const std::string& key) {
  DCHECK(!key.empty());

  if (ui::ime::InputMethodMenuManager::GetInstance()
          ->HasInputMethodMenuItemForKey(key)) {
    TextInputMethod* engine = IMEBridge::Get()->GetCurrentEngineHandler();
    if (engine) {
      engine->PropertyActivate(key);
    }
    return;
  }

  DVLOG(1) << "ActivateInputMethodMenuItem: unknown key: " << key;
}

void InputMethodManagerImpl::ConnectInputEngineManager(
    mojo::PendingReceiver<ime::mojom::InputEngineManager> receiver) {
  DCHECK(state_);
  ImeServiceConnectorMap::iterator iter =
      ime_service_connectors_.find(state_->GetProfile());
  if (iter == ime_service_connectors_.end()) {
    auto connector_ =
        std::make_unique<ImeServiceConnector>(state_->GetProfile());
    iter =
        ime_service_connectors_
            .insert(std::make_pair(state_->GetProfile(), std::move(connector_)))
            .first;
  }
  iter->second->SetupImeService(std::move(receiver));
}

void InputMethodManagerImpl::BindInputMethodUserDataService(
    mojo::PendingReceiver<ime::mojom::InputMethodUserDataService> receiver) {
  DCHECK(state_);
  ImeServiceConnectorMap::iterator iter =
      ime_service_connectors_.find(state_->GetProfile());
  if (iter == ime_service_connectors_.end()) {
    auto connector_ =
        std::make_unique<ImeServiceConnector>(state_->GetProfile());
    iter =
        ime_service_connectors_
            .insert(std::make_pair(state_->GetProfile(), std::move(connector_)))
            .first;
  }
  iter->second->BindInputMethodUserDataService(std::move(receiver));
}

bool InputMethodManagerImpl::IsISOLevel5ShiftUsedByCurrentInputMethod() const {
  return keyboard_->IsISOLevel5ShiftAvailable();
}

bool InputMethodManagerImpl::IsAltGrUsedByCurrentInputMethod() const {
  return keyboard_->IsAltGrAvailable();
}

bool InputMethodManagerImpl::ArePositionalShortcutsUsedByCurrentInputMethod()
    const {
  if (!state_ || !::features::IsImprovedKeyboardShortcutsEnabled()) {
    return false;
  }

  return !non_positional_layouts_.contains(
      state_.get()->GetCurrentInputMethod().keyboard_layout());
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
  // Enabled and current (active) IM should be set to owner/user's default.
  PrefService* prefs = g_browser_process->local_state();
  PrefService* user_prefs = profile ? profile->GetPrefs() : nullptr;
  std::string initial_input_method_id;
  if (user_prefs) {
    initial_input_method_id =
        user_prefs->GetString(prefs::kLanguageCurrentInputMethod);
  }
  if (initial_input_method_id.empty()) {
    initial_input_method_id =
        prefs->GetString(language_prefs::kPreferredKeyboardLayout);
  }

  const InputMethodDescriptor* descriptor =
      GetInputMethodUtil()->GetInputMethodDescriptorFromId(
          initial_input_method_id.empty()
              ? GetInputMethodUtil()->GetFallbackInputMethodDescriptor().id()
              : initial_input_method_id);

  auto* new_state = new StateImpl(this, profile, descriptor);
  return scoped_refptr<InputMethodManager::State>(new_state);
}

void InputMethodManagerImpl::SetCandidateWindowControllerForTesting(
    CandidateWindowController* candidate_window_controller) {
  candidate_window_controller_.reset(candidate_window_controller);
  candidate_window_controller_->AddObserver(this);
}

void InputMethodManagerImpl::OnAppTerminating() {
  if (candidate_window_controller_.get()) {
    candidate_window_controller_.reset();
  }

  if (assistive_window_controller_.get()) {
    assistive_window_controller_.reset();
    IMEBridge::Get()->SetAssistiveWindowHandler(nullptr);
  }
}

void InputMethodManagerImpl::CandidateClicked(int index) {
  TextInputMethod* engine = IMEBridge::Get()->GetCurrentEngineHandler();
  if (engine) {
    engine->CandidateClicked(index);
  }
}

void InputMethodManagerImpl::CandidateWindowOpened() {
  for (auto& observer : candidate_window_observers_) {
    observer.CandidateWindowOpened(this);
  }
}

void InputMethodManagerImpl::CandidateWindowClosed() {
  for (auto& observer : candidate_window_observers_) {
    observer.CandidateWindowClosed(this);
  }
}

void InputMethodManagerImpl::AssistiveWindowButtonClicked(
    const ui::ime::AssistiveWindowButton& button) const {
  TextInputMethod* engine = IMEBridge::Get()->GetCurrentEngineHandler();
  if (engine) {
    engine->AssistiveWindowButtonClicked(button);
  }
}

void InputMethodManagerImpl::AssistiveWindowChanged(
    const ash::ime::AssistiveWindow& window) const {
  TextInputMethod* engine = IMEBridge::Get()->GetCurrentEngineHandler();
  if (engine) {
    engine->AssistiveWindowChanged(window);
  }
}

void InputMethodManagerImpl::ImeMenuActivationChanged(bool is_active) {
  // Saves the state that whether the expanded IME menu has been activated by
  // users. This method is only called when the preference is changing.
  state_->SetMenuActivated(is_active);
  MaybeNotifyImeMenuActivationChanged();
}

void InputMethodManagerImpl::NotifyInputMethodExtensionAdded(
    const std::string& extension_id) {
  for (auto& observer : observers_) {
    observer.OnInputMethodExtensionAdded(extension_id);
  }
}

void InputMethodManagerImpl::NotifyInputMethodExtensionRemoved(
    const std::string& extension_id) {
  for (auto& observer : observers_) {
    observer.OnInputMethodExtensionRemoved(extension_id);
  }
}

void InputMethodManagerImpl::NotifyImeMenuListChanged() {
  for (auto& observer : ime_menu_observers_) {
    observer.ImeMenuListChanged();
  }
}

void InputMethodManagerImpl::MaybeInitializeCandidateWindowController() {
  if (candidate_window_controller_.get()) {
    return;
  }

  candidate_window_controller_.reset(
      CandidateWindowController::CreateCandidateWindowController());
  candidate_window_controller_->AddObserver(this);
}

void InputMethodManagerImpl::MaybeInitializeAssistiveWindowController() {
  if (assistive_window_controller_.get()) {
    return;
  }

  assistive_window_controller_ =
      std::make_unique<AssistiveWindowController>(this, state_->GetProfile());
  IMEBridge::Get()->SetAssistiveWindowHandler(
      assistive_window_controller_.get());
}

void InputMethodManagerImpl::NotifyImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<InputMethodManager::MenuItem>& items) {
  for (auto& observer : ime_menu_observers_) {
    observer.ImeMenuItemsChanged(engine_id, items);
  }
}

void InputMethodManagerImpl::MaybeNotifyImeMenuActivationChanged() {
  if (is_ime_menu_activated_ == state_->IsMenuActivated()) {
    return;
  }

  is_ime_menu_activated_ = state_->IsMenuActivated();
  for (auto& observer : ime_menu_observers_) {
    observer.ImeMenuActivationChanged(is_ime_menu_activated_);
  }
}

void InputMethodManagerImpl::OverrideKeyboardKeyset(ImeKeyset keyset) {
  GURL url = state_->GetInputViewUrl();

  // If fails to find ref or tag "id" in the ref, it means the current IME is
  // not system IME, and we don't support show emoji, handwriting or voice
  // input for such IME extension.
  if (!url.has_ref()) {
    return;
  }
  std::string overridden_ref = url.ref();

  auto id_start = overridden_ref.find("id=");
  if (id_start == std::string::npos) {
    return;
  }

  if (keyset == ImeKeyset::kNone) {
    // Resets the url as the input method default url and notify the hash
    // changed to VK.
    state_->ResetInputViewUrl();
    ReloadKeyboard();
    return;
  }

  // For IME component extension, the input view url is overridden as:
  // chrome-extension://${extension_id}/inputview.html#id=us.compact.qwerty
  // &language=en-US&passwordLayout=us.compact.qwerty&name=keyboard_us
  // For emoji, handwriting and voice input, we append the keyset to the end of
  // id like: id=${keyset}.emoji/hwt/voice.
  auto id_end = overridden_ref.find("&", id_start + 1);
  std::string id_string = overridden_ref.substr(id_start, id_end - id_start);
  // Remove existing keyset string.
  for (const ImeKeyset keyset_to_find : kKeysets) {
    std::string keyset_string = KeysetToString(keyset_to_find);
    auto keyset_start = id_string.find("." + keyset_string);
    if (keyset_start != std::string::npos) {
      id_string.replace(keyset_start, keyset_string.length() + 1, "");
    }
  }
  id_string += "." + KeysetToString(keyset);
  overridden_ref.replace(id_start, id_end - id_start, id_string);

  // Always add a timestamp tag to make sure the hash tags are changed, so that
  // the frontend will reload.
  auto ts_start = overridden_ref.find("&ts=");
  std::string ts_tag =
      base::StringPrintf("&ts=%" PRId64, base::Time::NowFromSystemTime()
                                             .ToDeltaSinceWindowsEpoch()
                                             .InMicroseconds());
  if (ts_start == std::string::npos) {
    overridden_ref += ts_tag;
  } else {
    auto ts_end = overridden_ref.find("&", ts_start + 1);
    if (ts_end == std::string::npos) {
      overridden_ref.replace(ts_start, overridden_ref.length() - ts_start,
                             ts_tag);
    } else {
      overridden_ref.replace(ts_start, ts_end - ts_start, ts_tag);
    }
  }

  GURL::Replacements replacements;
  replacements.SetRefStr(overridden_ref);
  state_->OverrideInputViewUrl(url.ReplaceComponents(replacements));
  ReloadKeyboard();
}

void InputMethodManagerImpl::SetImeMenuFeatureEnabled(ImeMenuFeature feature,
                                                      bool enabled) {
  const uint32_t original_state = features_enabled_state_;
  if (enabled) {
    features_enabled_state_ |= feature;
  } else {
    features_enabled_state_ &= ~feature;
  }
  if (original_state != features_enabled_state_) {
    NotifyObserversImeExtraInputStateChange();
  }
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

void InputMethodManagerImpl::ReloadKeyboard() {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_enabled()) {
    keyboard_client->ReloadKeyboardIfNeeded();
  }
}

}  // namespace input_method
}  // namespace ash
