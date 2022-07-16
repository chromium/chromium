// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include <string>

// feature_list.h is a widely included header and its size impacts build
// time. Try not to raise this limit unless necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#ifndef NACL_TC_REV
#pragma clang max_tokens_here 530000
#endif

#include <stddef.h>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

namespace base {

namespace {

// Pointer to the FeatureList instance singleton that was set via
// FeatureList::SetInstance(). Does not use base/memory/singleton.h in order to
// have more control over initialization timing. Leaky.
FeatureList* g_feature_list_instance = nullptr;

// Tracks whether the FeatureList instance was initialized via an accessor, and
// which Feature that accessor was for, if so.
const Feature* g_initialized_from_accessor = nullptr;

#if DCHECK_IS_ON()
// Tracks whether the use of base::Feature is allowed for this module.
// See ForbidUseForCurrentModule().
bool g_use_allowed = true;

const char* g_reason_overrides_disallowed = nullptr;

void DCheckOverridesAllowed() {
  const bool feature_overrides_allowed = !g_reason_overrides_disallowed;
  DCHECK(feature_overrides_allowed) << g_reason_overrides_disallowed;
}
#else
void DCheckOverridesAllowed() {}
#endif

// An allocator entry for a feature in shared memory. The FeatureEntry is
// followed by a base::Pickle object that contains the feature and trial name.
struct FeatureEntry {
  // SHA1(FeatureEntry): Increment this if structure changes!
  static constexpr uint32_t kPersistentTypeId = 0x06567CA6 + 1;

  // Expected size for 32/64-bit check.
  static constexpr size_t kExpectedInstanceSize = 8;

  // Specifies whether a feature override enables or disables the feature. Same
  // values as the OverrideState enum in feature_list.h
  uint32_t override_state;

  // Size of the pickled structure, NOT the total size of this entry.
  uint32_t pickle_size;

  // Reads the feature and trial name from the pickle. Calling this is only
  // valid on an initialized entry that's in shared memory.
  bool GetFeatureAndTrialName(StringPiece* feature_name,
                              StringPiece* trial_name) const {
    const char* src =
        reinterpret_cast<const char*>(this) + sizeof(FeatureEntry);

    Pickle pickle(src, pickle_size);
    PickleIterator pickle_iter(pickle);

    if (!pickle_iter.ReadStringPiece(feature_name))
      return false;

    // Return true because we are not guaranteed to have a trial name anyways.
    auto sink = pickle_iter.ReadStringPiece(trial_name);
    ALLOW_UNUSED_LOCAL(sink);
    return true;
  }
};

// Some characters are not allowed to appear in feature names or the associated
// field trial names, as they are used as special characters for command-line
// serialization. This function checks that the strings are ASCII (since they
// are used in command-line API functions that require ASCII) and whether there
// are any reserved characters present, returning true if the string is valid.
// Only called in DCHECKs.
bool IsValidFeatureOrFieldTrialName(const std::string& name) {
  return IsStringASCII(name) && name.find_first_of(",<*") == std::string::npos;
}

// Splits |first| into two parts by the |separator| where the first part will be
// returned updated in |first| and the second part will be returned as |second|.
// This function returns false if there is more than one |separator| in |first|.
// If there is no |separator| presented in |first|, this function will not
// modify |first| and |second|. It's used for splitting the |enable_features|
// flag into feature name, field trial name and feature parameters.
bool SplitIntoTwo(const std::string& separator,
                  StringPiece* first,
                  std::string* second) {
  std::vector<StringPiece> parts =
      SplitStringPiece(*first, separator, TRIM_WHITESPACE, SPLIT_WANT_ALL);
  if (parts.size() == 2) {
    *second = std::string(parts[1]);
  } else if (parts.size() > 2) {
    DLOG(ERROR) << "Only one '" << separator
                << "' is allowed but got: " << *first;
    return false;
  }
  *first = parts[0];
  return true;
}

// Checks and parses the |enable_features| flag and sets
// |parsed_enable_features| to be a comma-separated list of features,
// |force_fieldtrials| to be a comma-separated list of field trials that each
// feature want to associate with and |force_fieldtrial_params| to be the field
// trial parameters for each field trial.
// Returns true if |enable_features| is parsable, otherwise false.
bool ParseEnableFeatures(const std::string& enable_features,
                         std::string* parsed_enable_features,
                         std::string* force_fieldtrials,
                         std::string* force_fieldtrial_params) {
  std::vector<std::string> enable_features_list;
  std::vector<std::string> force_fieldtrials_list;
  std::vector<std::string> force_fieldtrial_params_list;
  for (auto& enable_feature :
       FeatureList::SplitFeatureListString(enable_features)) {
    // First, check whether ":" is present. If true, feature parameters were
    // set for this feature.
    std::string feature_params;
    if (!SplitIntoTwo(":", &enable_feature, &feature_params))
      return false;
    // Then, check whether "." is present. If true, a group was specified for
    // this feature.
    std::string group;
    if (!SplitIntoTwo(".", &enable_feature, &group))
      return false;
    // Finally, check whether "<" is present. If true, a study was specified for
    // this feature.
    std::string study;
    if (!SplitIntoTwo("<", &enable_feature, &study))
      return false;

    const std::string feature_name(enable_feature);
    // If feature params were set but group and study weren't, associate the
    // feature and its feature params to a synthetic field trial as the
    // feature params only make sense when it's combined with a field trial.
    if (!feature_params.empty()) {
      study = study.empty() ? "Study" + feature_name : study;
      group = group.empty() ? "Group" + feature_name : group;
      force_fieldtrials_list.push_back(study + "/" + group);
      force_fieldtrial_params_list.push_back(study + "." + group + ":" +
                                             feature_params);
    }
    enable_features_list.push_back(
        study.empty() ? feature_name : (feature_name + "<" + study));
  }

  *parsed_enable_features = JoinString(enable_features_list, ",");
  // Field trial separator is currently a slash. See
  // |kPersistentStringSeparator| in base/metrics/field_trial.cc.
  *force_fieldtrials = JoinString(force_fieldtrials_list, "/");
  *force_fieldtrial_params = JoinString(force_fieldtrial_params_list, ",");
  return true;
}

}  // namespace

#if defined(DCHECK_IS_CONFIGURABLE)
const Feature kDCheckIsFatalFeature{"DcheckIsFatal",
                                    FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(DCHECK_IS_CONFIGURABLE)

FeatureList::FeatureList() = default;

FeatureList::~FeatureList() = default;

FeatureList::ScopedDisallowOverrides::ScopedDisallowOverrides(
    const char* reason)
#if DCHECK_IS_ON()
    : previous_reason_(g_reason_overrides_disallowed) {
  g_reason_overrides_disallowed = reason;
}
#else
{
}
#endif

FeatureList::ScopedDisallowOverrides::~ScopedDisallowOverrides() {
#if DCHECK_IS_ON()
  g_reason_overrides_disallowed = previous_reason_;
#endif
}

void FeatureList::InitializeFromCommandLine(
    const std::string& enable_features,
    const std::string& disable_features) {
  DCHECK(!initialized_);

  std::string parsed_enable_features;
  std::string force_fieldtrials;
  std::string force_fieldtrial_params;
  bool parse_enable_features_result =
      ParseEnableFeatures(enable_features, &parsed_enable_features,
                          &force_fieldtrials, &force_fieldtrial_params);
  DCHECK(parse_enable_features_result) << StringPrintf(
      "The --%s list is unparsable or invalid, please check the format.",
      ::switches::kEnableFeatures);

  // Only create field trials when field_trial_list is available. Some tests
  // don't have field trial list available.
  if (FieldTrialList::GetInstance()) {
    bool associate_params_result = AssociateFieldTrialParamsFromString(
        force_fieldtrial_params, &UnescapeValue);
    DCHECK(associate_params_result) << StringPrintf(
        "The field trial parameters part of the --%s list is invalid. Make "
        "sure "
        "you %%-encode the following characters in param values: %%:/.,",
        ::switches::kEnableFeatures);

    bool create_trials_result =
        FieldTrialList::CreateTrialsFromString(force_fieldtrials);
    DCHECK(create_trials_result)
        << StringPrintf("Invalid field trials are specified in --%s.",
                        ::switches::kEnableFeatures);
  }

  // Process disabled features first, so that disabled ones take precedence over
  // enabled ones (since RegisterOverride() uses insert()).
  RegisterOverridesFromCommandLine(disable_features, OVERRIDE_DISABLE_FEATURE);
  RegisterOverridesFromCommandLine(parsed_enable_features,
                                   OVERRIDE_ENABLE_FEATURE);

  initialized_from_command_line_ = true;
}

void FeatureList::InitializeFromSharedMemory(
    PersistentMemoryAllocator* allocator) {
  DCHECK(!initialized_);

  PersistentMemoryAllocator::Iterator iter(allocator);
  const FeatureEntry* entry;
  while ((entry = iter.GetNextOfObject<FeatureEntry>()) != nullptr) {
    OverrideState override_state =
        static_cast<OverrideState>(entry->override_state);

    StringPiece feature_name;
    StringPiece trial_name;
    if (!entry->GetFeatureAndTrialName(&feature_name, &trial_name))
      continue;

    FieldTrial* trial = FieldTrialList::Find(trial_name);
    RegisterOverride(feature_name, override_state, trial);
  }
}

bool FeatureList::IsFeatureOverridden(const std::string& feature_name) const {
  return overrides_.count(feature_name);
}

bool FeatureList::IsFeatureOverriddenFromCommandLine(
    const std::string& feature_name) const {
  auto it = overrides_.find(feature_name);
  return it != overrides_.end() && !it->second.overridden_by_field_trial;
}

bool FeatureList::IsFeatureOverriddenFromCommandLine(
    const std::string& feature_name,
    OverrideState state) const {
  auto it = overrides_.find(feature_name);
  return it != overrides_.end() && !it->second.overridden_by_field_trial &&
         it->second.overridden_state == state;
}

void FeatureList::AssociateReportingFieldTrial(
    const std::string& feature_name,
    OverrideState for_overridden_state,
    FieldTrial* field_trial) {
  DCHECK(
      IsFeatureOverriddenFromCommandLine(feature_name, for_overridden_state));

  // Only one associated field trial is supported per feature. This is generally
  // enforced server-side.
  OverrideEntry* entry = &overrides_.find(feature_name)->second;
  if (entry->field_trial) {
    NOTREACHED() << "Feature " << feature_name
                 << " already has trial: " << entry->field_trial->trial_name()
                 << ", associating trial: " << field_trial->trial_name();
    return;
  }

  entry->field_trial = field_trial;
}

void FeatureList::RegisterFieldTrialOverride(const std::string& feature_name,
                                             OverrideState override_state,
                                             FieldTrial* field_trial) {
  DCHECK(field_trial);
  DCHECK(!Contains(overrides_, feature_name) ||
         !overrides_.find(feature_name)->second.field_trial)
      << "Feature " << feature_name << " is overriden multiple times in these "
      << "trials: "
      << overrides_.find(feature_name)->second.field_trial->trial_name()
      << " and " << field_trial->trial_name() << ". "
      << "Check the trial (study) in (1) the server config, "
      << "(2) fieldtrial_testing_config.json, (3) about_flags.cc, and "
      << "(4) client-side field trials.";

  RegisterOverride(feature_name, override_state, field_trial);
}

void FeatureList::RegisterExtraFeatureOverrides(
    const std::vector<FeatureOverrideInfo>& extra_overrides) {
  for (const FeatureOverrideInfo& override_info : extra_overrides) {
    RegisterOverride(override_info.first.get().name, override_info.second,
                     /* field_trial = */ nullptr);
  }
}

void FeatureList::AddFeaturesToAllocator(PersistentMemoryAllocator* allocator) {
  DCHECK(initialized_);

  for (const auto& override : overrides_) {
    Pickle pickle;
    pickle.WriteString(override.first);
    if (override.second.field_trial)
      pickle.WriteString(override.second.field_trial->trial_name());

    size_t total_size = sizeof(FeatureEntry) + pickle.size();
    FeatureEntry* entry = allocator->New<FeatureEntry>(total_size);
    if (!entry)
      return;

    entry->override_state = override.second.overridden_state;
    entry->pickle_size = pickle.size();

    char* dst = reinterpret_cast<char*>(entry) + sizeof(FeatureEntry);
    memcpy(dst, pickle.data(), pickle.size());

    allocator->MakeIterable(entry);
  }
}

void FeatureList::GetFeatureOverrides(std::string* enable_overrides,
                                      std::string* disable_overrides) {
  GetFeatureOverridesImpl(enable_overrides, disable_overrides, false);
}

void FeatureList::GetCommandLineFeatureOverrides(
    std::string* enable_overrides,
    std::string* disable_overrides) {
  GetFeatureOverridesImpl(enable_overrides, disable_overrides, true);
}

// static
bool FeatureList::IsEnabled(const Feature& feature) {
#if DCHECK_IS_ON()
  CHECK(g_use_allowed) << "base::Feature not permitted for this module.";
#endif
  if (!g_feature_list_instance) {
    g_initialized_from_accessor = &feature;
    return feature.default_state == FEATURE_ENABLED_BY_DEFAULT;
  }
  return g_feature_list_instance->IsFeatureEnabled(feature);
}

// static
absl::optional<bool> FeatureList::GetStateIfOverridden(const Feature& feature) {
#if DCHECK_IS_ON()
  CHECK(g_use_allowed) << "base::Feature not permitted for this module.";
#endif
  if (!g_feature_list_instance) {
    g_initialized_from_accessor = &feature;
    // If there is no feature list, there can be no overrides.
    return absl::nullopt;
  }
  return g_feature_list_instance->IsFeatureEnabledIfOverridden(feature);
}

// static
FieldTrial* FeatureList::GetFieldTrial(const Feature& feature) {
#if DCHECK_IS_ON()
  // See documentation for ForbidUseForCurrentModule.
  CHECK(g_use_allowed) << "base::Feature not permitted for this module.";
#endif
  if (!g_feature_list_instance) {
    g_initialized_from_accessor = &feature;
    return nullptr;
  }
  return g_feature_list_instance->GetAssociatedFieldTrial(feature);
}

// static
std::vector<StringPiece> FeatureList::SplitFeatureListString(
    StringPiece input) {
  return SplitStringPiece(input, ",", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
}

// static
bool FeatureList::InitializeInstance(const std::string& enable_features,
                                     const std::string& disable_features) {
  return InitializeInstance(enable_features, disable_features,
                            std::vector<FeatureOverrideInfo>());
}

// static
bool FeatureList::InitializeInstance(
    const std::string& enable_features,
    const std::string& disable_features,
    const std::vector<FeatureOverrideInfo>& extra_overrides) {
  // We want to initialize a new instance here to support command-line features
  // in testing better. For example, we initialize a dummy instance in
  // base/test/test_suite.cc, and override it in content/browser/
  // browser_main_loop.cc.
  // On the other hand, we want to avoid re-initialization from command line.
  // For example, we initialize an instance in chrome/browser/
  // chrome_browser_main.cc and do not override it in content/browser/
  // browser_main_loop.cc.
  // If the singleton was previously initialized from within an accessor, we
  // want to prevent callers from reinitializing the singleton and masking the
  // accessor call(s) which likely returned incorrect information.
  if (g_initialized_from_accessor) {
    DEBUG_ALIAS_FOR_CSTR(accessor_name, g_initialized_from_accessor->name, 128);
    CHECK(!g_initialized_from_accessor);
  }
  bool instance_existed_before = false;
  if (g_feature_list_instance) {
    if (g_feature_list_instance->initialized_from_command_line_)
      return false;

    delete g_feature_list_instance;
    g_feature_list_instance = nullptr;
    instance_existed_before = true;
  }

  std::unique_ptr<FeatureList> feature_list(new FeatureList);
  feature_list->InitializeFromCommandLine(enable_features, disable_features);
  feature_list->RegisterExtraFeatureOverrides(extra_overrides);
  FeatureList::SetInstance(std::move(feature_list));
  return !instance_existed_before;
}

// static
FeatureList* FeatureList::GetInstance() {
  return g_feature_list_instance;
}

// static
void FeatureList::SetInstance(std::unique_ptr<FeatureList> instance) {
  DCHECK(!g_feature_list_instance);
  instance->FinalizeInitialization();

  // Note: Intentional leak of global singleton.
  g_feature_list_instance = instance.release();

#if defined(DCHECK_IS_CONFIGURABLE)
  // Update the behaviour of LOGGING_DCHECK to match the Feature configuration.
  // DCHECK is also forced to be FATAL if we are running a death-test.
  // TODO(crbug.com/1057995#c11): --gtest_internal_run_death_test doesn't
  // currently run through this codepath, mitigated in
  // base::TestSuite::Initialize() for now.
  // TODO(asvitkine): If we find other use-cases that need integrating here
  // then define a proper API/hook for the purpose.
  if (FeatureList::IsEnabled(kDCheckIsFatalFeature) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          "gtest_internal_run_death_test")) {
    logging::LOGGING_DCHECK = logging::LOG_FATAL;
  } else {
    logging::LOGGING_DCHECK = logging::LOG_INFO;
  }
#endif  // defined(DCHECK_IS_CONFIGURABLE)
}

// static
std::unique_ptr<FeatureList> FeatureList::ClearInstanceForTesting() {
  FeatureList* old_instance = g_feature_list_instance;
  g_feature_list_instance = nullptr;
  g_initialized_from_accessor = nullptr;
  return WrapUnique(old_instance);
}

// static
void FeatureList::RestoreInstanceForTesting(
    std::unique_ptr<FeatureList> instance) {
  DCHECK(!g_feature_list_instance);
  // Note: Intentional leak of global singleton.
  g_feature_list_instance = instance.release();
}

// static
void FeatureList::ForbidUseForCurrentModule() {
#if DCHECK_IS_ON()
  // Verify there hasn't been any use prior to being called.
  DCHECK(!g_initialized_from_accessor);
  g_use_allowed = false;
#endif  // DCHECK_IS_ON()
}

void FeatureList::FinalizeInitialization() {
  DCHECK(!initialized_);
  // Store the field trial list pointer for DCHECKing.
  field_trial_list_ = FieldTrialList::GetInstance();
  initialized_ = true;
}

bool FeatureList::IsFeatureEnabled(const Feature& feature) {
  OverrideState overridden_state = GetOverrideState(feature);

  // If marked as OVERRIDE_USE_DEFAULT, simply return the default state below.
  if (overridden_state != OVERRIDE_USE_DEFAULT)
    return overridden_state == OVERRIDE_ENABLE_FEATURE;

  return feature.default_state == FEATURE_ENABLED_BY_DEFAULT;
}

absl::optional<bool> FeatureList::IsFeatureEnabledIfOverridden(
    const Feature& feature) {
  OverrideState overridden_state = GetOverrideState(feature);

  // If marked as OVERRIDE_USE_DEFAULT, fall through to returning empty.
  if (overridden_state != OVERRIDE_USE_DEFAULT)
    return overridden_state == OVERRIDE_ENABLE_FEATURE;

  return absl::nullopt;
}

FeatureList::OverrideState FeatureList::GetOverrideState(
    const Feature& feature) {
  DCHECK(initialized_);
  DCHECK(IsValidFeatureOrFieldTrialName(feature.name)) << feature.name;
  DCHECK(CheckFeatureIdentity(feature)) << feature.name;

  auto it = overrides_.find(feature.name);
  if (it != overrides_.end()) {
    const OverrideEntry& entry = it->second;

    // Activate the corresponding field trial, if necessary.
    if (entry.field_trial)
      entry.field_trial->group();

    // TODO(asvitkine) Expand this section as more support is added.

    return entry.overridden_state;
  }
  // Otherwise, report that we want to use the default state.
  return OVERRIDE_USE_DEFAULT;
}

FieldTrial* FeatureList::GetAssociatedFieldTrial(const Feature& feature) {
  DCHECK(initialized_);
  DCHECK(CheckFeatureIdentity(feature)) << feature.name;

  return GetAssociatedFieldTrialByFeatureName(feature.name);
}

const base::FeatureList::OverrideEntry*
FeatureList::GetOverrideEntryByFeatureName(StringPiece name) {
  DCHECK(initialized_);
  DCHECK(IsValidFeatureOrFieldTrialName(std::string(name))) << name;

  auto it = overrides_.find(name);
  if (it != overrides_.end()) {
    const OverrideEntry& entry = it->second;
    return &entry;
  }
  return nullptr;
}

FieldTrial* FeatureList::GetAssociatedFieldTrialByFeatureName(
    StringPiece name) {
  DCHECK(initialized_);

  const base::FeatureList::OverrideEntry* entry =
      GetOverrideEntryByFeatureName(name);
  if (entry) {
    return entry->field_trial;
  }
  return nullptr;
}

FieldTrial* FeatureList::GetEnabledFieldTrialByFeatureName(StringPiece name) {
  DCHECK(initialized_);

  const base::FeatureList::OverrideEntry* entry =
      GetOverrideEntryByFeatureName(name);
  if (entry &&
      entry->overridden_state == base::FeatureList::OVERRIDE_ENABLE_FEATURE) {
    return entry->field_trial;
  }
  return nullptr;
}

void FeatureList::RegisterOverridesFromCommandLine(
    const std::string& feature_list,
    OverrideState overridden_state) {
  for (const auto& value : SplitFeatureListString(feature_list)) {
    StringPiece feature_name = value;
    FieldTrial* trial = nullptr;

    // The entry may be of the form FeatureName<FieldTrialName - in which case,
    // this splits off the field trial name and associates it with the override.
    std::string::size_type pos = feature_name.find('<');
    if (pos != std::string::npos) {
      feature_name = StringPiece(value.data(), pos);
      trial = FieldTrialList::Find(value.substr(pos + 1));
#if !defined(OS_NACL)
      // If the below DCHECK fires, it means a non-existent trial name was
      // specified via the "Feature<Trial" command-line syntax.
      DCHECK(trial) << "trial='" << value.substr(pos + 1) << "' does not exist";
#endif  // !defined(OS_NACL)
    }

    RegisterOverride(feature_name, overridden_state, trial);
  }
}

void FeatureList::RegisterOverride(StringPiece feature_name,
                                   OverrideState overridden_state,
                                   FieldTrial* field_trial) {
  DCHECK(!initialized_);
  DCheckOverridesAllowed();
  if (field_trial) {
    DCHECK(IsValidFeatureOrFieldTrialName(field_trial->trial_name()))
        << field_trial->trial_name();
  }
  if (StartsWith(feature_name, "*")) {
    feature_name = feature_name.substr(1);
    overridden_state = OVERRIDE_USE_DEFAULT;
  }

  // Note: The semantics of emplace() is that it does not overwrite the entry if
  // one already exists for the key. Thus, only the first override for a given
  // feature name takes effect.
  overrides_.emplace(std::string(feature_name),
                     OverrideEntry(overridden_state, field_trial));
}

void FeatureList::GetFeatureOverridesImpl(std::string* enable_overrides,
                                          std::string* disable_overrides,
                                          bool command_line_only) {
  DCHECK(initialized_);

  // Check that the FieldTrialList this is associated with, if any, is the
  // active one. If not, it likely indicates that this FeatureList has override
  // entries from a freed FieldTrial, which may be caused by an incorrect test
  // set up.
  if (field_trial_list_)
    DCHECK_EQ(field_trial_list_, FieldTrialList::GetInstance());

  enable_overrides->clear();
  disable_overrides->clear();

  // Note: Since |overrides_| is a std::map, iteration will be in alphabetical
  // order. This is not guaranteed to users of this function, but is useful for
  // tests to assume the order.
  for (const auto& entry : overrides_) {
    if (command_line_only &&
        (entry.second.field_trial != nullptr ||
         entry.second.overridden_state == OVERRIDE_USE_DEFAULT)) {
      continue;
    }

    std::string* target_list = nullptr;
    switch (entry.second.overridden_state) {
      case OVERRIDE_USE_DEFAULT:
      case OVERRIDE_ENABLE_FEATURE:
        target_list = enable_overrides;
        break;
      case OVERRIDE_DISABLE_FEATURE:
        target_list = disable_overrides;
        break;
    }

    if (!target_list->empty())
      target_list->push_back(',');
    if (entry.second.overridden_state == OVERRIDE_USE_DEFAULT)
      target_list->push_back('*');
    target_list->append(entry.first);
    if (entry.second.field_trial) {
      target_list->push_back('<');
      target_list->append(entry.second.field_trial->trial_name());
    }
  }
}

bool FeatureList::CheckFeatureIdentity(const Feature& feature) {
  AutoLock auto_lock(feature_identity_tracker_lock_);

  auto it = feature_identity_tracker_.find(feature.name);
  if (it == feature_identity_tracker_.end()) {
    // If it's not tracked yet, register it.
    feature_identity_tracker_[feature.name] = &feature;
    return true;
  }
  // Compare address of |feature| to the existing tracked entry.
  return it->second == &feature;
}

FeatureList::OverrideEntry::OverrideEntry(OverrideState overridden_state,
                                          FieldTrial* field_trial)
    : overridden_state(overridden_state),
      field_trial(field_trial),
      overridden_by_field_trial(field_trial != nullptr) {}

}  // namespace base
