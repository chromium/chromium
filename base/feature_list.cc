// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"

#include <string>
#include <tuple>

#include <stddef.h>

#include "base/base_switches.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/pickle.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequence_manager/work_queue.h"
#include "build/build_config.h"

namespace base {

namespace {

// Pointer to the FeatureList instance singleton that was set via
// FeatureList::SetInstance(). Does not use base/memory/singleton.h in order to
// have more control over initialization timing. Leaky.
FeatureList* g_feature_list_instance = nullptr;

// Tracks access to Feature state before FeatureList registration.
class EarlyFeatureAccessTracker {
 public:
  static EarlyFeatureAccessTracker* GetInstance() {
    static NoDestructor<EarlyFeatureAccessTracker> instance;
    return instance.get();
  }

  // Invoked when `feature` is accessed before FeatureList registration.
  void AccessedFeature(const Feature& feature) {
    AutoLock lock(lock_);
    if (fail_instantly_)
      Fail(&feature);
    else if (!feature_)
      feature_ = &feature;
  }

  // Asserts that no feature was accessed before FeatureList registration.
  void AssertNoAccess() {
    AutoLock lock(lock_);
    if (feature_)
      Fail(feature_);
  }

  // Makes calls to AccessedFeature() fail instantly.
  void FailOnFeatureAccessWithoutFeatureList() {
    AutoLock lock(lock_);
    if (feature_)
      Fail(feature_);
    fail_instantly_ = true;
  }

  // Resets the state of this tracker.
  void Reset() {
    AutoLock lock(lock_);
    feature_ = nullptr;
    fail_instantly_ = false;
  }

 private:
  void Fail(const Feature* feature) {
    // TODO(crbug.com/1358639): Enable this check on all platforms.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
#if !BUILDFLAG(IS_NACL)
    // Create a crash key with the name of the feature accessed too early, to
    // facilitate crash triage.
    SCOPED_CRASH_KEY_STRING256("FeatureList", "feature-accessed-too-early",
                               feature->name);
#endif  // !BUILDFLAG(IS_NACL)
    // Fail if DCHECKs are enabled.
    DCHECK(!feature) << "Accessed feature " << feature->name
                     << " before FeatureList registration.";
    // TODO(crbug.com/1383852): When we believe that all early accesses have
    // been fixed, remove this base::debug::DumpWithoutCrashing() and change the
    // above DCHECK to a CHECK.

    // The following line is commented to reduce the crash volume while a fix
    // for crbug.com/1392145 is prepared.
    // base::debug::DumpWithoutCrashing();
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_CHROMEOS)
  }

  friend class NoDestructor<EarlyFeatureAccessTracker>;

  EarlyFeatureAccessTracker() = default;
  ~EarlyFeatureAccessTracker() = default;

  Lock lock_;

  // First feature to be accessed before FeatureList registration.
  raw_ptr<const Feature> feature_ GUARDED_BY(lock_) = nullptr;

  // Whether AccessedFeature() should fail instantly.
  bool fail_instantly_ GUARDED_BY(lock_) = false;
};

// Controls whether a feature's override state will be cached in
// `base::Feature::cached_value`. This field and the associated `base::Feature`
// only exist to measure the impact of the caching on different performance
// metrics.
// TODO(crbug.com/1341292): Remove this global and this feature once the gains
// are measured.
bool g_cache_override_state = false;
BASE_FEATURE(kCacheFeatureOverrideState,
             "CacheFeatureOverrideState",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if DCHECK_IS_ON()
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
  static constexpr uint32_t kPersistentTypeId = 0x06567CA6 + 2;

  // Expected size for 32/64-bit check.
  static constexpr size_t kExpectedInstanceSize = 16;

  // Specifies whether a feature override enables or disables the feature. Same
  // values as the OverrideState enum in feature_list.h
  uint32_t override_state;

  // On e.g. x86, alignof(uint64_t) is 4.  Ensure consistent size and alignment
  // of `pickle_size` across platforms.
  uint32_t padding;

  // Size of the pickled structure, NOT the total size of this entry.
  uint64_t pickle_size;

  // Reads the feature and trial name from the pickle. Calling this is only
  // valid on an initialized entry that's in shared memory.
  bool GetFeatureAndTrialName(StringPiece* feature_name,
                              StringPiece* trial_name) const {
    const char* src =
        reinterpret_cast<const char*>(this) + sizeof(FeatureEntry);

    Pickle pickle(src, checked_cast<size_t>(pickle_size));
    PickleIterator pickle_iter(pickle);

    if (!pickle_iter.ReadStringPiece(feature_name))
      return false;

    // Return true because we are not guaranteed to have a trial name anyways.
    std::ignore = pickle_iter.ReadStringPiece(trial_name);
    return true;
  }
};

// Splits |text| into two parts by the |separator| where the first part will be
// returned updated in |first| and the second part will be returned as |second|.
// This function returns false if there is more than one |separator| in |first|.
// If there is no |separator| presented in |first|, this function will not
// modify |first| and |second|. It's used for splitting the |enable_features|
// flag into feature name, field trial name and feature parameters.
bool SplitIntoTwo(StringPiece text,
                  StringPiece separator,
                  StringPiece* first,
                  std::string* second) {
  std::vector<StringPiece> parts =
      SplitStringPiece(text, separator, TRIM_WHITESPACE, SPLIT_WANT_ALL);
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
  for (const auto& enable_feature :
       FeatureList::SplitFeatureListString(enable_features)) {
    std::string feature_name;
    std::string study;
    std::string group;
    std::string feature_params;
    if (!FeatureList::ParseEnableFeatureString(
            enable_feature, &feature_name, &study, &group, &feature_params)) {
      return false;
    }

    // If feature params were set but group and study weren't, associate the
    // feature and its feature params to a synthetic field trial as the
    // feature params only make sense when it's combined with a field trial.
    if (!feature_params.empty()) {
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

std::pair<FeatureList::OverrideState, uint16_t> UnpackFeatureCache(
    uint32_t packed_cache_value) {
  return std::make_pair(
      static_cast<FeatureList::OverrideState>(packed_cache_value >> 24),
      packed_cache_value & 0xFFFF);
}

uint32_t PackFeatureCache(FeatureList::OverrideState override_state,
                          uint32_t caching_context) {
  return (static_cast<uint32_t>(override_state) << 24) |
         (caching_context & 0xFFFF);
}

}  // namespace

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
BASE_FEATURE(kDCheckIsFatalFeature,
             "DcheckIsFatal",
             FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

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
  DCHECK(!HasAssociatedFieldTrialByFeatureName(feature_name))
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
                                      std::string* disable_overrides,
                                      bool include_group_name) const {
  GetFeatureOverridesImpl(enable_overrides, disable_overrides, false,
                          include_group_name);
}

void FeatureList::GetCommandLineFeatureOverrides(
    std::string* enable_overrides,
    std::string* disable_overrides) const {
  GetFeatureOverridesImpl(enable_overrides, disable_overrides, true);
}

// static
bool FeatureList::IsEnabled(const Feature& feature) {
  if (!g_feature_list_instance) {
    EarlyFeatureAccessTracker::GetInstance()->AccessedFeature(feature);
    return feature.default_state == FEATURE_ENABLED_BY_DEFAULT;
  }
  return g_feature_list_instance->IsFeatureEnabled(feature);
}

// static
bool FeatureList::IsValidFeatureOrFieldTrialName(StringPiece name) {
  return IsStringASCII(name) && name.find_first_of(",<*") == std::string::npos;
}

// static
absl::optional<bool> FeatureList::GetStateIfOverridden(const Feature& feature) {
  if (!g_feature_list_instance) {
    EarlyFeatureAccessTracker::GetInstance()->AccessedFeature(feature);
    // If there is no feature list, there can be no overrides.
    return absl::nullopt;
  }
  return g_feature_list_instance->IsFeatureEnabledIfOverridden(feature);
}

// static
FieldTrial* FeatureList::GetFieldTrial(const Feature& feature) {
  if (!g_feature_list_instance) {
    EarlyFeatureAccessTracker::GetInstance()->AccessedFeature(feature);
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
bool FeatureList::ParseEnableFeatureString(StringPiece enable_feature,
                                           std::string* feature_name,
                                           std::string* study_name,
                                           std::string* group_name,
                                           std::string* params) {
  StringPiece first;
  // First, check whether ":" is present. If true, feature parameters were
  // set for this feature.
  std::string feature_params;
  if (!SplitIntoTwo(enable_feature, ":", &first, &feature_params))
    return false;
  // Then, check whether "." is present. If true, a group was specified for
  // this feature.
  std::string group;
  if (!SplitIntoTwo(first, ".", &first, &group))
    return false;
  // Finally, check whether "<" is present. If true, a study was specified for
  // this feature.
  std::string study;
  if (!SplitIntoTwo(first, "<", &first, &study))
    return false;

  std::string enable_feature_name(first);
  // If feature params were set but group and study weren't, associate the
  // feature and its feature params to a synthetic field trial as the
  // feature params only make sense when it's combined with a field trial.
  if (!feature_params.empty()) {
    study = study.empty() ? "Study" + enable_feature_name : study;
    group = group.empty() ? "Group" + enable_feature_name : group;
  }

  feature_name->swap(enable_feature_name);
  study_name->swap(study);
  group_name->swap(group);
  params->swap(feature_params);
  return true;
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
  EarlyFeatureAccessTracker::GetInstance()->AssertNoAccess();
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

  EarlyFeatureAccessTracker::GetInstance()->AssertNoAccess();

#if !BUILDFLAG(IS_NACL)
  // Configured first because it takes precedence over the getrandom() trial.
  internal::ConfigureBoringSSLBackedRandBytesFieldTrial();
#endif

#if BUILDFLAG(IS_ANDROID)
  internal::ConfigureRandBytesFieldTrial();
#endif

  g_cache_override_state =
      base::FeatureList::IsEnabled(kCacheFeatureOverrideState);

  base::sequence_manager::internal::WorkQueue::ConfigureCapacityFieldTrial();

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
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
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)
}

// static
std::unique_ptr<FeatureList> FeatureList::ClearInstanceForTesting() {
  FeatureList* old_instance = g_feature_list_instance;
  g_feature_list_instance = nullptr;
  EarlyFeatureAccessTracker::GetInstance()->Reset();
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
void FeatureList::FailOnFeatureAccessWithoutFeatureList() {
  EarlyFeatureAccessTracker::GetInstance()
      ->FailOnFeatureAccessWithoutFeatureList();
}

void FeatureList::SetCachingContextForTesting(uint16_t caching_context) {
  caching_context_ = caching_context;
}

void FeatureList::FinalizeInitialization() {
  DCHECK(!initialized_);
  // Store the field trial list pointer for DCHECKing.
  field_trial_list_ = FieldTrialList::GetInstance();
  initialized_ = true;
}

bool FeatureList::IsFeatureEnabled(const Feature& feature) const {
  OverrideState overridden_state = GetOverrideState(feature);

  // If marked as OVERRIDE_USE_DEFAULT, simply return the default state below.
  if (overridden_state != OVERRIDE_USE_DEFAULT)
    return overridden_state == OVERRIDE_ENABLE_FEATURE;

  return feature.default_state == FEATURE_ENABLED_BY_DEFAULT;
}

absl::optional<bool> FeatureList::IsFeatureEnabledIfOverridden(
    const Feature& feature) const {
  OverrideState overridden_state = GetOverrideState(feature);

  // If marked as OVERRIDE_USE_DEFAULT, fall through to returning empty.
  if (overridden_state != OVERRIDE_USE_DEFAULT)
    return overridden_state == OVERRIDE_ENABLE_FEATURE;

  return absl::nullopt;
}

FeatureList::OverrideState FeatureList::GetOverrideState(
    const Feature& feature) const {
  DCHECK(initialized_);
  DCHECK(IsValidFeatureOrFieldTrialName(feature.name)) << feature.name;
  DCHECK(CheckFeatureIdentity(feature)) << feature.name;

  // If caching is disabled, always perform the full lookup.
  if (!g_cache_override_state)
    return GetOverrideStateByFeatureName(feature.name);

  uint32_t current_cache_value =
      feature.cached_value.load(std::memory_order_relaxed);

  auto unpacked = UnpackFeatureCache(current_cache_value);

  if (unpacked.second == caching_context_)
    return unpacked.first;

  OverrideState state = GetOverrideStateByFeatureName(feature.name);
  uint32_t new_cache_value = PackFeatureCache(state, caching_context_);

  // Update the cache with the new value.
  // In non-test code, this value can be in one of 2 states: either it's unset,
  // or another thread has updated it to the same value we're about to write.
  // Because of this, a plain `store` yields the correct result in all cases.
  // In test code, it's possible for a different thread to have installed a new
  // `ScopedFeatureList` and written a value that's different than the one we're
  // about to write, although that would be a thread safety violation already
  // and such tests should be fixed.
  feature.cached_value.store(new_cache_value, std::memory_order_relaxed);

  return state;
}

FeatureList::OverrideState FeatureList::GetOverrideStateByFeatureName(
    StringPiece feature_name) const {
  DCHECK(initialized_);
  DCHECK(IsValidFeatureOrFieldTrialName(feature_name)) << feature_name;

  auto it = overrides_.find(feature_name);
  if (it != overrides_.end()) {
    const OverrideEntry& entry = it->second;

    // Activate the corresponding field trial, if necessary.
    if (entry.field_trial)
      entry.field_trial->Activate();

    // TODO(asvitkine) Expand this section as more support is added.

    return entry.overridden_state;
  }
  // Otherwise, report that we want to use the default state.
  return OVERRIDE_USE_DEFAULT;
}

FieldTrial* FeatureList::GetAssociatedFieldTrial(const Feature& feature) const {
  DCHECK(initialized_);
  DCHECK(CheckFeatureIdentity(feature)) << feature.name;

  return GetAssociatedFieldTrialByFeatureName(feature.name);
}

const base::FeatureList::OverrideEntry*
FeatureList::GetOverrideEntryByFeatureName(StringPiece name) const {
  DCHECK(initialized_);
  DCHECK(IsValidFeatureOrFieldTrialName(name)) << name;

  auto it = overrides_.find(name);
  if (it != overrides_.end()) {
    const OverrideEntry& entry = it->second;
    return &entry;
  }
  return nullptr;
}

FieldTrial* FeatureList::GetAssociatedFieldTrialByFeatureName(
    StringPiece name) const {
  DCHECK(initialized_);

  const base::FeatureList::OverrideEntry* entry =
      GetOverrideEntryByFeatureName(name);
  if (entry) {
    return entry->field_trial;
  }
  return nullptr;
}

bool FeatureList::HasAssociatedFieldTrialByFeatureName(StringPiece name) const {
  DCHECK(!initialized_);
  auto entry = overrides_.find(name);
  return entry != overrides_.end() && entry->second.field_trial != nullptr;
}

FieldTrial* FeatureList::GetEnabledFieldTrialByFeatureName(
    StringPiece name) const {
  DCHECK(initialized_);

  const base::FeatureList::OverrideEntry* entry =
      GetOverrideEntryByFeatureName(name);
  if (entry &&
      entry->overridden_state == base::FeatureList::OVERRIDE_ENABLE_FEATURE) {
    return entry->field_trial;
  }
  return nullptr;
}

std::unique_ptr<FeatureList::Accessor> FeatureList::ConstructAccessor() {
  if (initialized_) {
    // This function shouldn't be called after initialization.
    NOTREACHED();
    return nullptr;
  }
  // Use new and WrapUnique because we want to restrict access to the Accessor's
  // constructor.
  return base::WrapUnique(new Accessor(this));
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
#if !BUILDFLAG(IS_NACL)
      // If the below DCHECK fires, it means a non-existent trial name was
      // specified via the "Feature<Trial" command-line syntax.
      DCHECK(trial) << "trial='" << value.substr(pos + 1) << "' does not exist";
#endif  // !BUILDFLAG(IS_NACL)
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
                                          bool command_line_only,
                                          bool include_group_name) const {
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
      auto* const field_trial = entry.second.field_trial.get();
      target_list->push_back('<');
      target_list->append(field_trial->trial_name());
      if (include_group_name) {
        target_list->push_back('.');
        target_list->append(field_trial->GetGroupNameWithoutActivation());
      }
    }
  }
}

bool FeatureList::CheckFeatureIdentity(const Feature& feature) const {
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

FeatureList::Accessor::Accessor(FeatureList* feature_list)
    : feature_list_(feature_list) {}

FeatureList::OverrideState FeatureList::Accessor::GetOverrideStateByFeatureName(
    StringPiece feature_name) {
  return feature_list_->GetOverrideStateByFeatureName(feature_name);
}

bool FeatureList::Accessor::GetParamsByFeatureName(
    StringPiece feature_name,
    std::map<std::string, std::string>* params) {
  base::FieldTrial* trial =
      feature_list_->GetAssociatedFieldTrialByFeatureName(feature_name);
  return FieldTrialParamAssociator::GetInstance()->GetFieldTrialParams(trial,
                                                                       params);
}

}  // namespace base
