// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_syncer.h"

#include <set>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace input_method {
namespace {

// Checks input method IDs, converting engine IDs to input method IDs and
// removing unsupported IDs from |values|.
void CheckAndResolveInputMethodIDs(
    const InputMethodDescriptors& supported_descriptors,
    std::vector<std::string>* values) {
  // Extract the supported input method IDs into a set.
  std::set<std::string> supported_input_method_ids;
  for (const auto& descriptor : supported_descriptors) {
    supported_input_method_ids.insert(descriptor.id());
  }

  // Convert engine IDs to input method extension IDs.
  base::ranges::transform(values->begin(), values->end(), values->begin(),
                          extension_ime_util::GetInputMethodIDByEngineID);

  // Remove values that aren't found in the set of supported input method IDs.
  auto it = values->begin();
  while (it != values->end()) {
    if (it->size() && supported_input_method_ids.find(*it) !=
                          supported_input_method_ids.end()) {
      ++it;
    } else {
      it = values->erase(it);
    }
  }
}

// Checks whether each language is supported, replacing locales with variants
// if they are available. Must be called on a thread that allows IO.
std::string CheckAndResolveLocales(const std::string& languages) {
  if (languages.empty()) {
    return languages;
  }
  std::vector<std::string> values = base::SplitString(
      languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  const std::string app_locale = g_browser_process->GetApplicationLocale();

  std::vector<std::string> accept_language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &accept_language_codes);
  std::sort(accept_language_codes.begin(), accept_language_codes.end());

  // Remove unsupported language values.
  auto value_iter = values.begin();
  while (value_iter != values.end()) {
    if (binary_search(accept_language_codes.begin(),
                      accept_language_codes.end(), *value_iter)) {
      ++value_iter;
      continue;
    }

    // If a language code resolves to a supported backup locale, replace it
    // with the resolved locale.
    std::string resolved_locale;
    if (l10n_util::CheckAndResolveLocale(*value_iter, &resolved_locale)) {
      if (binary_search(accept_language_codes.begin(),
                        accept_language_codes.end(), resolved_locale)) {
        *value_iter = resolved_locale;
        ++value_iter;
        continue;
      }
    }
    value_iter = values.erase(value_iter);
  }

  return base::JoinString(values, ",");
}

// Appends tokens from |src| that are not in |dest| to |dest|.
void MergeLists(std::vector<std::string_view>* dest,
                const std::vector<std::string_view>& src) {
  // Keep track of already-added tokens.
  std::set<std::string_view> unique_tokens(dest->begin(), dest->end());

  for (const auto& token : src) {
    // Skip token if it's already in |dest|.
    if (binary_search(unique_tokens.begin(), unique_tokens.end(), token)) {
      continue;
    }
    dest->push_back(token);
    unique_tokens.insert(token);
  }
}

}  // anonymous namespace

InputMethodSyncer::InputMethodSyncer(
    sync_preferences::PrefServiceSyncable* prefs,
    scoped_refptr<InputMethodManager::State> ime_state)
    : prefs_(prefs), ime_state_(ime_state), merging_(false) {}

InputMethodSyncer::~InputMethodSyncer() {
  prefs_->RemoveObserver(this);
}

// static
void InputMethodSyncer::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kLanguagePreloadEnginesSyncable, "",
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(
      prefs::kLanguageEnabledImesSyncable, "",
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  // Locally tracks whether we should do the first-sync merge, hence not a
  // syncable pref itself.
  registry->RegisterBooleanPref(prefs::kLanguageShouldMergeInputMethods, false);
}

void InputMethodSyncer::Initialize() {
  // This causes OnIsSyncingChanged to be called when the PrefService starts
  // syncing prefs.
  prefs_->AddObserver(this);

  preferred_languages_syncable_.Init(
      language::prefs::kPreferredLanguagesSyncable, prefs_);
  preload_engines_syncable_.Init(prefs::kLanguagePreloadEnginesSyncable,
                                 prefs_);
  enabled_imes_syncable_.Init(prefs::kLanguageEnabledImesSyncable, prefs_);

  BooleanPrefMember::NamedChangeCallback callback = base::BindRepeating(
      &InputMethodSyncer::OnPreferenceChanged, base::Unretained(this));
  preferred_languages_.Init(language::prefs::kPreferredLanguages, prefs_,
                            callback);
  preload_engines_.Init(prefs::kLanguagePreloadEngines, prefs_, callback);
  enabled_imes_.Init(prefs::kLanguageEnabledImes, prefs_, callback);

  // If we have already synced but haven't merged input methods yet, do so now.
  if (prefs_->GetBoolean(prefs::kLanguageShouldMergeInputMethods) &&
      !(preferred_languages_syncable_.GetValue().empty() &&
        preload_engines_syncable_.GetValue().empty() &&
        enabled_imes_syncable_.GetValue().empty())) {
    MergeSyncedPrefs();
  }
}

void InputMethodSyncer::MergeSyncedPrefs() {
  // This should only be done after the first ever sync.
  DCHECK(prefs_->GetBoolean(prefs::kLanguageShouldMergeInputMethods));
  prefs_->SetBoolean(prefs::kLanguageShouldMergeInputMethods, false);
  merging_ = true;

  std::vector<std::string_view> synced_tokens;
  std::vector<std::string_view> new_tokens;

  // First, set the syncable prefs to the union of the local and synced prefs.
  std::string preferred_languages_syncable =
      preferred_languages_syncable_.GetValue();
  synced_tokens =
      base::SplitStringPiece(preferred_languages_syncable, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string preferred_languages = preferred_languages_.GetValue();
  new_tokens = base::SplitStringPiece(
      preferred_languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Append the synced values to the current values.
  MergeLists(&new_tokens, synced_tokens);
  preferred_languages_syncable_.SetValue(base::JoinString(new_tokens, ","));

  std::string enabled_imes_syncable = enabled_imes_syncable_.GetValue();
  synced_tokens = base::SplitStringPiece(
      enabled_imes_syncable, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string enabled_imes = enabled_imes_.GetValue();
  new_tokens = base::SplitStringPiece(enabled_imes, ",", base::TRIM_WHITESPACE,
                                      base::SPLIT_WANT_ALL);

  MergeLists(&new_tokens, synced_tokens);
  enabled_imes_syncable_.SetValue(base::JoinString(new_tokens, ","));

  // Revert preload engines to legacy component IDs.
  std::string preload_engines = preload_engines_.GetValue();
  std::vector<std::string> new_token_values;
  new_token_values = base::SplitString(
      preload_engines, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  base::ranges::transform(new_token_values, new_token_values.begin(),
                          extension_ime_util::GetComponentIDByInputMethodID);
  std::string preload_engines_syncable = preload_engines_syncable_.GetValue();
  synced_tokens =
      base::SplitStringPiece(preload_engines_syncable, ",",
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  new_tokens = std::vector<std::string_view>(new_token_values.begin(),
                                             new_token_values.end());
  MergeLists(&new_tokens, synced_tokens);
  preload_engines_syncable_.SetValue(base::JoinString(new_tokens, ","));

  // Second, set the local prefs, incorporating new values from the sync server.
  preload_engines_.SetValue(AddSupportedInputMethodValues(
      preload_engines_.GetValue(), preload_engines_syncable,
      prefs::kLanguagePreloadEngines));
  enabled_imes_.SetValue(AddSupportedInputMethodValues(
      enabled_imes_.GetValue(), enabled_imes_syncable,
      prefs::kLanguageEnabledImes));

  // Remove unsupported locales before updating the local languages preference.
  std::string languages(AddSupportedInputMethodValues(
      preferred_languages_.GetValue(), preferred_languages_syncable,
      language::prefs::kPreferredLanguages));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CheckAndResolveLocales, languages),
      base::BindOnce(&InputMethodSyncer::FinishMerge,
                     weak_factory_.GetWeakPtr()));
}

std::string InputMethodSyncer::AddSupportedInputMethodValues(
    const std::string& pref,
    const std::string& synced_pref,
    const char* pref_name) {
  std::vector<std::string_view> old_tokens = base::SplitStringPiece(
      pref, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<std::string> new_token_values = base::SplitString(
      synced_pref, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Check and convert the new tokens.
  if (pref_name == prefs::kLanguagePreloadEngines ||
      pref_name == prefs::kLanguageEnabledImes) {
    InputMethodManager* manager = InputMethodManager::Get();
    InputMethodDescriptors supported_descriptors;

    if (pref_name == prefs::kLanguagePreloadEngines) {
      // Add the available component extension IMEs.
      ComponentExtensionIMEManager* component_extension_manager =
          manager->GetComponentExtensionIMEManager();
      supported_descriptors =
          component_extension_manager->GetAllIMEAsInputMethodDescriptor();
    } else {
      ime_state_->GetInputMethodExtensions(&supported_descriptors);
    }
    CheckAndResolveInputMethodIDs(supported_descriptors, &new_token_values);
  } else if (pref_name != language::prefs::kPreferredLanguages) {
    NOTREACHED_IN_MIGRATION() << "Attempting to merge an invalid preference.";
    // kPreferredLanguages is checked in CheckAndResolveLocales().
  }

  // Do the actual merging.
  std::vector<std::string_view> new_tokens(new_token_values.begin(),
                                           new_token_values.end());
  MergeLists(&old_tokens, new_tokens);
  return base::JoinString(old_tokens, ",");
}

void InputMethodSyncer::FinishMerge(const std::string& languages) {
  // Since the merge only removed locales that are unsupported on this system,
  // we don't need to update the syncable prefs. If the local preference changes
  // later, the sync server will lose the values we dropped. That's okay since
  // the values from this device should then become the new defaults anyway.
  preferred_languages_.SetValue(languages);

  // We've finished merging.
  merging_ = false;
}

void InputMethodSyncer::OnPreferenceChanged(const std::string& pref_name) {
  DCHECK(pref_name == language::prefs::kPreferredLanguages ||
         pref_name == prefs::kLanguagePreloadEngines ||
         pref_name == prefs::kLanguageEnabledImes);

  if (merging_ || prefs_->GetBoolean(prefs::kLanguageShouldMergeInputMethods)) {
    return;
  }

  // Set the language and input prefs at the same time. Otherwise we may,
  // e.g., use a stale languages setting but push a new preload engines setting.
  preferred_languages_syncable_.SetValue(preferred_languages_.GetValue());
  enabled_imes_syncable_.SetValue(enabled_imes_.GetValue());

  // For preload engines, use legacy xkb IDs so the preference can sync
  // across Chrome OS and Chromium OS.
  std::vector<std::string> engines =
      base::SplitString(preload_engines_.GetValue(), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  base::ranges::transform(engines, engines.begin(),
                          extension_ime_util::GetComponentIDByInputMethodID);
  preload_engines_syncable_.SetValue(base::JoinString(engines, ","));
}

void InputMethodSyncer::OnIsSyncingChanged() {
  // Only merge once.
  if (!prefs_->GetBoolean(prefs::kLanguageShouldMergeInputMethods)) {
    return;
  }
  // Wait for the correct type of prefs to sync before merging.
  bool is_syncing = prefs_->AreOsPrefsSyncing();
  if (is_syncing) {
    MergeSyncedPrefs();
  }
}

}  // namespace input_method
}  // namespace ash
