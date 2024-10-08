// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/memory.h"
#include "base/process/process_handle.h"
#include "base/process/process_info.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_BLINK)
#include "base/memory/shared_memory_switch.h"
#include "base/process/launch.h"
#endif

#if BUILDFLAG(IS_APPLE) && BUILDFLAG(USE_BLINK)
#include "base/apple/mach_port_rendezvous.h"
#endif

#if BUILDFLAG(IS_POSIX) && BUILDFLAG(USE_BLINK)
#include <unistd.h>  // For getppid().
#include "base/threading/platform_thread.h"
// On POSIX, the fd is shared using the mapping in GlobalDescriptors.
#include "base/posix/global_descriptors.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/vmo.h>
#include <zircon/process.h>

#include "base/fuchsia/fuchsia_logging.h"
#endif

namespace base {

namespace {

#if BUILDFLAG(USE_BLINK)
using shared_memory::SharedMemoryError;
#endif

// Define a separator character to use when creating a persistent form of an
// instance.  This is intended for use as a command line argument, passed to a
// second process to mimic our state (i.e., provide the same group name).
const char kPersistentStringSeparator = '/';  // Currently a slash.

// Define a marker character to be used as a prefix to a trial name on the
// command line which forces its activation.
const char kActivationMarker = '*';

// Constants for the field trial allocator.
const char kAllocatorName[] = "FieldTrialAllocator";

// We allocate 256 KiB to hold all the field trial data. This should be enough,
// as most people use 3 - 25 KiB for field trials (as of 11/25/2016).
// This also doesn't allocate all 256 KiB at once -- the pages only get mapped
// to physical memory when they are touched. If the size of the allocated field
// trials does get larger than 256 KiB, then we will drop some field trials in
// child processes, leading to an inconsistent view between browser and child
// processes and possibly causing crashes (see crbug.com/661617).
const size_t kFieldTrialAllocationSize = 256 << 10;  // 256 KiB

#if BUILDFLAG(IS_APPLE) && BUILDFLAG(USE_BLINK)
constexpr MachPortsForRendezvous::key_type kFieldTrialRendezvousKey = 'fldt';
#endif

// Writes out string1 and then string2 to pickle.
void WriteStringPair(Pickle* pickle,
                     std::string_view string1,
                     std::string_view string2) {
  pickle->WriteString(string1);
  pickle->WriteString(string2);
}

// Writes out the field trial's contents (via trial_state) to the pickle. The
// format of the pickle looks like:
// TrialName, GroupName, is_overridden, ParamKey1, ParamValue1, ParamKey2,
// ParamValue2, ... If there are no parameters, then it just ends at
// is_overridden.
void PickleFieldTrial(const FieldTrial::PickleState& trial_state,
                      Pickle* pickle) {
  WriteStringPair(pickle, *trial_state.trial_name, *trial_state.group_name);
  pickle->WriteBool(trial_state.is_overridden);

  // Get field trial params.
  std::map<std::string, std::string> params;
  FieldTrialParamAssociator::GetInstance()->GetFieldTrialParamsWithoutFallback(
      *trial_state.trial_name, *trial_state.group_name, &params);

  // Write params to pickle.
  for (const auto& param : params)
    WriteStringPair(pickle, param.first, param.second);
}

// Returns the boundary value for comparing against the FieldTrial's added
// groups for a given |divisor| (total probability) and |entropy_value|.
FieldTrial::Probability GetGroupBoundaryValue(
    FieldTrial::Probability divisor,
    double entropy_value) {
  // Add a tiny epsilon value to get consistent results when converting floating
  // points to int. Without it, boundary values have inconsistent results, e.g.:
  //
  //   static_cast<FieldTrial::Probability>(100 * 0.56) == 56
  //   static_cast<FieldTrial::Probability>(100 * 0.57) == 56
  //   static_cast<FieldTrial::Probability>(100 * 0.58) == 57
  //   static_cast<FieldTrial::Probability>(100 * 0.59) == 59
  const double kEpsilon = 1e-8;
  const FieldTrial::Probability result =
      static_cast<FieldTrial::Probability>(divisor * entropy_value + kEpsilon);
  // Ensure that adding the epsilon still results in a value < |divisor|.
  return std::min(result, divisor - 1);
}

void OnOutOfMemory(size_t size) {
  TerminateBecauseOutOfMemory(size);
}

void AppendFieldTrialGroupToString(bool activated,
                                   std::string_view trial_name,
                                   std::string_view group_name,
                                   std::string& field_trials_string) {
  DCHECK_EQ(std::string::npos, trial_name.find(kPersistentStringSeparator))
      << " in name " << trial_name;
  DCHECK_EQ(std::string::npos, group_name.find(kPersistentStringSeparator))
      << " in name " << group_name;

  if (!field_trials_string.empty()) {
    // Add a '/' in-between field trial groups.
    field_trials_string.push_back(kPersistentStringSeparator);
  }
  if (activated) {
    field_trials_string.push_back(kActivationMarker);
  }

  base::StrAppend(&field_trials_string,
                  {trial_name, std::string_view(&kPersistentStringSeparator, 1),
                   group_name});
}

}  // namespace

// statics
const int FieldTrial::kNotFinalized = -1;
const int FieldTrial::kDefaultGroupNumber = 0;
bool FieldTrial::enable_benchmarking_ = false;

//------------------------------------------------------------------------------
// FieldTrial methods and members.

FieldTrial::EntropyProvider::~EntropyProvider() = default;

uint32_t FieldTrial::EntropyProvider::GetPseudorandomValue(
    uint32_t salt,
    uint32_t output_range) const {
  // Passing a different salt is sufficient to get a "different" result from
  // GetEntropyForTrial (ignoring collisions).
  double entropy_value = GetEntropyForTrial(/*trial_name=*/"", salt);

  // Convert the [0,1) double to an [0, output_range) integer.
  return static_cast<uint32_t>(GetGroupBoundaryValue(
      static_cast<FieldTrial::Probability>(output_range), entropy_value));
}

FieldTrial::PickleState::PickleState() = default;

FieldTrial::PickleState::PickleState(const PickleState& other) = default;

FieldTrial::PickleState::~PickleState() = default;

bool FieldTrial::FieldTrialEntry::GetState(std::string_view& trial_name,
                                           std::string_view& group_name,
                                           bool& overridden) const {
  PickleIterator iter = GetPickleIterator();
  return ReadHeader(iter, trial_name, group_name, overridden);
}

bool FieldTrial::FieldTrialEntry::GetParams(
    std::map<std::string, std::string>* params) const {
  PickleIterator iter = GetPickleIterator();
  std::string_view tmp_string;
  bool tmp_bool;
  // Skip reading trial and group name, and overridden bit.
  if (!ReadHeader(iter, tmp_string, tmp_string, tmp_bool)) {
    return false;
  }

  while (true) {
    std::string_view key;
    std::string_view value;
    if (!ReadStringPair(&iter, &key, &value))
      return key.empty();  // Non-empty is bad: got one of a pair.
    (*params)[std::string(key)] = std::string(value);
  }
}

PickleIterator FieldTrial::FieldTrialEntry::GetPickleIterator() const {
  Pickle pickle = Pickle::WithUnownedBuffer(
      // TODO(crbug.com/40284755): FieldTrialEntry should be constructed with a
      // span over the pickle memory.
      UNSAFE_TODO(
          span(GetPickledDataPtr(), checked_cast<size_t>(pickle_size))));
  return PickleIterator(pickle);
}

bool FieldTrial::FieldTrialEntry::ReadHeader(PickleIterator& iter,
                                             std::string_view& trial_name,
                                             std::string_view& group_name,
                                             bool& overridden) const {
  return ReadStringPair(&iter, &trial_name, &group_name) &&
         iter.ReadBool(&overridden);
}

bool FieldTrial::FieldTrialEntry::ReadStringPair(
    PickleIterator* iter,
    std::string_view* trial_name,
    std::string_view* group_name) const {
  if (!iter->ReadStringPiece(trial_name))
    return false;
  if (!iter->ReadStringPiece(group_name))
    return false;
  return true;
}

void FieldTrial::AppendGroup(const std::string& name,
                             Probability group_probability) {
  // When the group choice was previously forced, we only need to return the
  // the id of the chosen group, and anything can be returned for the others.
  if (forced_) {
    DCHECK(!group_name_.empty());
    if (name == group_name_) {
      // Note that while |group_| may be equal to |kDefaultGroupNumber| on the
      // forced trial, it will not have the same value as the default group
      // number returned from the non-forced |FactoryGetFieldTrial()| call,
      // which takes care to ensure that this does not happen.
      return;
    }
    DCHECK_NE(next_group_number_, group_);
    // We still return different numbers each time, in case some caller need
    // them to be different.
    next_group_number_++;
    return;
  }

  DCHECK_LE(group_probability, divisor_);
  DCHECK_GE(group_probability, 0);

  if (enable_benchmarking_)
    group_probability = 0;

  accumulated_group_probability_ += group_probability;

  DCHECK_LE(accumulated_group_probability_, divisor_);
  if (group_ == kNotFinalized && accumulated_group_probability_ > random_) {
    // This is the group that crossed the random line, so we do the assignment.
    SetGroupChoice(name, next_group_number_);
  }
  next_group_number_++;
  return;
}

void FieldTrial::Activate() {
  FinalizeGroupChoice();
  if (trial_registered_)
    FieldTrialList::NotifyFieldTrialGroupSelection(this);
}

const std::string& FieldTrial::group_name() {
  // Call |Activate()| to ensure group gets assigned and observers are notified.
  Activate();
  DCHECK(!group_name_.empty());
  return group_name_;
}

const std::string& FieldTrial::GetGroupNameWithoutActivation() {
  FinalizeGroupChoice();
  return group_name_;
}

void FieldTrial::SetForced() {
  // We might have been forced before (e.g., by CreateFieldTrial) and it's
  // first come first served, e.g., command line switch has precedence.
  if (forced_)
    return;

  // And we must finalize the group choice before we mark ourselves as forced.
  FinalizeGroupChoice();
  forced_ = true;
}

bool FieldTrial::IsOverridden() const {
  return is_overridden_;
}

// static
void FieldTrial::EnableBenchmarking() {
  // We don't need to see field trials created via CreateFieldTrial() for
  // benchmarking, because such field trials have only a single group and are
  // not affected by randomization that |enable_benchmarking_| would disable.
  DCHECK_EQ(0u, FieldTrialList::GetRandomizedFieldTrialCount());
  enable_benchmarking_ = true;
}

// static
FieldTrial* FieldTrial::CreateSimulatedFieldTrial(
    std::string_view trial_name,
    Probability total_probability,
    std::string_view default_group_name,
    double entropy_value) {
  // `is_low_anonymity` is only used for differentiating which observers of the
  // global `FieldTrialList` should be notified. As this field trial is assumed
  // to never be registered with the global `FieldTrialList`, `is_low_anonymity`
  // can be set to an arbitrary value here.
  return new FieldTrial(trial_name, total_probability, default_group_name,
                        entropy_value, /*is_low_anonymity=*/false,
                        /*is_overridden=*/false);
}

// static
bool FieldTrial::ParseFieldTrialsString(std::string_view trials_string,
                                        bool override_trials,
                                        std::vector<State>& entries) {
  size_t next_item = 0;
  while (next_item < trials_string.length()) {
    // Parse one entry. Entries have the format
    // TrialName1/GroupName1/TrialName2/GroupName2. Each loop parses one trial
    // and group name.

    // Find the first delimiter starting at next_item, or quit.
    size_t trial_name_end =
        trials_string.find(kPersistentStringSeparator, next_item);
    if (trial_name_end == trials_string.npos || next_item == trial_name_end) {
      return false;
    }
    // Find the second delimiter, or end of string.
    size_t group_name_end =
        trials_string.find(kPersistentStringSeparator, trial_name_end + 1);
    if (group_name_end == trials_string.npos) {
      group_name_end = trials_string.length();
    }
    // Group names should not be empty, so quit if it is.
    if (trial_name_end + 1 == group_name_end) {
      return false;
    }

    FieldTrial::State entry;
    // Verify if the trial should be activated or not.
    if (trials_string[next_item] == kActivationMarker) {
      // Name cannot be only the indicator.
      if (trial_name_end - next_item == 1) {
        return false;
      }
      next_item++;
      entry.activated = true;
    }
    entry.trial_name =
        trials_string.substr(next_item, trial_name_end - next_item);
    entry.group_name = trials_string.substr(
        trial_name_end + 1, group_name_end - trial_name_end - 1);
    entry.is_overridden = override_trials;
    // The next item starts after the delimiter, if it exists.
    next_item = group_name_end + 1;

    entries.push_back(std::move(entry));
  }
  return true;
}

// static
std::string FieldTrial::BuildFieldTrialStateString(
    const std::vector<State>& states) {
  std::string result;
  for (const State& state : states) {
    AppendFieldTrialGroupToString(state.activated, state.trial_name,
                                  state.group_name, result);
  }
  return result;
}

FieldTrial::FieldTrial(std::string_view trial_name,
                       const Probability total_probability,
                       std::string_view default_group_name,
                       double entropy_value,
                       bool is_low_anonymity,
                       bool is_overridden)
    : trial_name_(trial_name),
      divisor_(total_probability),
      default_group_name_(default_group_name),
      random_(GetGroupBoundaryValue(total_probability, entropy_value)),
      accumulated_group_probability_(0),
      next_group_number_(kDefaultGroupNumber + 1),
      group_(kNotFinalized),
      forced_(false),
      is_overridden_(is_overridden),
      group_reported_(false),
      trial_registered_(false),
      ref_(FieldTrialList::FieldTrialAllocator::kReferenceNull),
      is_low_anonymity_(is_low_anonymity) {
  DCHECK_GT(total_probability, 0);
  DCHECK(!trial_name_.empty());
  DCHECK(!default_group_name_.empty())
      << "Trial " << trial_name << " is missing a default group name.";
}

FieldTrial::~FieldTrial() = default;

void FieldTrial::SetTrialRegistered() {
  DCHECK_EQ(kNotFinalized, group_);
  DCHECK(!trial_registered_);
  trial_registered_ = true;
}

void FieldTrial::SetGroupChoice(const std::string& group_name, int number) {
  group_ = number;
  if (group_name.empty())
    StringAppendF(&group_name_, "%d", group_);
  else
    group_name_ = group_name;
  DVLOG(1) << "Field trial: " << trial_name_ << " Group choice:" << group_name_;
}

void FieldTrial::FinalizeGroupChoice() {
  if (group_ != kNotFinalized)
    return;
  accumulated_group_probability_ = divisor_;
  // Here it's OK to use |kDefaultGroupNumber| since we can't be forced and not
  // finalized.
  DCHECK(!forced_);
  SetGroupChoice(default_group_name_, kDefaultGroupNumber);
}

bool FieldTrial::GetActiveGroup(ActiveGroup* active_group) const {
  if (!group_reported_)
    return false;
  DCHECK_NE(group_, kNotFinalized);
  active_group->trial_name = trial_name_;
  active_group->group_name = group_name_;
  active_group->is_overridden = is_overridden_;
  return true;
}

void FieldTrial::GetStateWhileLocked(PickleState* field_trial_state) {
  FinalizeGroupChoice();
  field_trial_state->trial_name = &trial_name_;
  field_trial_state->group_name = &group_name_;
  field_trial_state->activated = group_reported_;
  field_trial_state->is_overridden = is_overridden_;
}

//------------------------------------------------------------------------------
// FieldTrialList methods and members.

// static
FieldTrialList* FieldTrialList::global_ = nullptr;

FieldTrialList::Observer::~Observer() = default;

FieldTrialList::FieldTrialList() {
  DCHECK(!global_);
  global_ = this;
}

FieldTrialList::~FieldTrialList() {
  AutoLock auto_lock(lock_);
  while (!registered_.empty()) {
    auto it = registered_.begin();
    it->second->Release();
    registered_.erase(it->first);
  }
  // Note: If this DCHECK fires in a test that uses ScopedFeatureList, it is
  // likely caused by nested ScopedFeatureLists being destroyed in a different
  // order than they are initialized.
  if (!was_reset_) {
    DCHECK_EQ(this, global_);
    global_ = nullptr;
  }
}

// static
FieldTrial* FieldTrialList::FactoryGetFieldTrial(
    std::string_view trial_name,
    FieldTrial::Probability total_probability,
    std::string_view default_group_name,
    const FieldTrial::EntropyProvider& entropy_provider,
    uint32_t randomization_seed,
    bool is_low_anonymity,
    bool is_overridden) {
  // Check if the field trial has already been created in some other way.
  FieldTrial* existing_trial = Find(trial_name);
  if (existing_trial) {
    CHECK(existing_trial->forced_);
    return existing_trial;
  }

  double entropy_value =
      entropy_provider.GetEntropyForTrial(trial_name, randomization_seed);

  FieldTrial* field_trial =
      new FieldTrial(trial_name, total_probability, default_group_name,
                     entropy_value, is_low_anonymity, is_overridden);
  FieldTrialList::Register(field_trial, /*is_randomized_trial=*/true);
  return field_trial;
}

// static
FieldTrial* FieldTrialList::Find(std::string_view trial_name) {
  if (!global_)
    return nullptr;
  AutoLock auto_lock(global_->lock_);
  return global_->PreLockedFind(trial_name);
}

// static
std::string FieldTrialList::FindFullName(std::string_view trial_name) {
  FieldTrial* field_trial = Find(trial_name);
  if (field_trial)
    return field_trial->group_name();
  return std::string();
}

// static
bool FieldTrialList::TrialExists(std::string_view trial_name) {
  return Find(trial_name) != nullptr;
}

// static
bool FieldTrialList::IsTrialActive(std::string_view trial_name) {
  FieldTrial* field_trial = Find(trial_name);
  return field_trial && field_trial->group_reported_;
}

// static
std::vector<FieldTrial::State> FieldTrialList::GetAllFieldTrialStates(
    PassKey<test::ScopedFeatureList>) {
  std::vector<FieldTrial::State> states;

  if (!global_)
    return states;

  AutoLock auto_lock(global_->lock_);
  for (const auto& registered : global_->registered_) {
    FieldTrial::PickleState trial;
    registered.second->GetStateWhileLocked(&trial);
    DCHECK_EQ(std::string::npos,
              trial.trial_name->find(kPersistentStringSeparator));
    DCHECK_EQ(std::string::npos,
              trial.group_name->find(kPersistentStringSeparator));
    FieldTrial::State entry;
    entry.activated = trial.activated;
    entry.trial_name = *trial.trial_name;
    entry.group_name = *trial.group_name;
    states.push_back(std::move(entry));
  }
  return states;
}

// static
void FieldTrialList::AllStatesToString(std::string* output) {
  if (!global_)
    return;
  AutoLock auto_lock(global_->lock_);

  for (const auto& registered : global_->registered_) {
    FieldTrial::PickleState trial;
    registered.second->GetStateWhileLocked(&trial);
    AppendFieldTrialGroupToString(trial.activated, *trial.trial_name,
                                  *trial.group_name, *output);
  }
}

// static
std::string FieldTrialList::AllParamsToString(EscapeDataFunc encode_data_func) {
  FieldTrialParamAssociator* params_associator =
      FieldTrialParamAssociator::GetInstance();
  std::string output;
  for (const auto& registered : GetRegisteredTrials()) {
    FieldTrial::PickleState trial;
    registered.second->GetStateWhileLocked(&trial);
    DCHECK_EQ(std::string::npos,
              trial.trial_name->find(kPersistentStringSeparator));
    DCHECK_EQ(std::string::npos,
              trial.group_name->find(kPersistentStringSeparator));
    std::map<std::string, std::string> params;
    if (params_associator->GetFieldTrialParamsWithoutFallback(
            *trial.trial_name, *trial.group_name, &params)) {
      if (params.size() > 0) {
        // Add comma to seprate from previous entry if it exists.
        if (!output.empty())
          output.append(1, ',');

        output.append(encode_data_func(*trial.trial_name));
        output.append(1, '.');
        output.append(encode_data_func(*trial.group_name));
        output.append(1, ':');

        std::string param_str;
        for (const auto& param : params) {
          // Add separator from previous param information if it exists.
          if (!param_str.empty()) {
            param_str.append(1, kPersistentStringSeparator);
          }
          param_str.append(encode_data_func(param.first));
          param_str.append(1, kPersistentStringSeparator);
          param_str.append(encode_data_func(param.second));
        }

        output.append(param_str);
      }
    }
  }
  return output;
}

// static
void FieldTrialList::GetActiveFieldTrialGroups(
    FieldTrial::ActiveGroups* active_groups) {
  GetActiveFieldTrialGroupsInternal(active_groups,
                                    /*include_low_anonymity=*/false);
}

// static
std::set<std::string> FieldTrialList::GetActiveTrialsOfParentProcess() {
  CHECK(global_);
  CHECK(global_->create_trials_in_child_process_called_);

  std::set<std::string> result;
  // CreateTrialsInChildProcess() may not have created the allocator if
  // kFieldTrialHandle was not passed on the command line.
  if (!global_->field_trial_allocator_) {
    return result;
  }

  FieldTrialAllocator* allocator = global_->field_trial_allocator_.get();
  FieldTrialAllocator::Iterator mem_iter(allocator);
  const FieldTrial::FieldTrialEntry* entry;
  while ((entry = mem_iter.GetNextOfObject<FieldTrial::FieldTrialEntry>()) !=
         nullptr) {
    std::string_view trial_name;
    std::string_view group_name;
    bool is_overridden;
    if (subtle::NoBarrier_Load(&entry->activated) &&
        entry->GetState(trial_name, group_name, is_overridden)) {
      result.emplace(trial_name);
    }
  }
  return result;
}

// static
bool FieldTrialList::CreateTrialsFromString(const std::string& trials_string,
                                            bool override_trials) {
  DCHECK(global_);
  if (trials_string.empty() || !global_)
    return true;

  std::vector<FieldTrial::State> entries;
  if (!FieldTrial::ParseFieldTrialsString(trials_string, override_trials,
                                          entries)) {
    return false;
  }

  return CreateTrialsFromFieldTrialStatesInternal(entries);
}

// static
bool FieldTrialList::CreateTrialsFromFieldTrialStates(
    PassKey<test::ScopedFeatureList>,
    const std::vector<FieldTrial::State>& entries) {
  return CreateTrialsFromFieldTrialStatesInternal(entries);
}

// static
void FieldTrialList::CreateTrialsInChildProcess(const CommandLine& cmd_line) {
  CHECK(!global_->create_trials_in_child_process_called_);
  global_->create_trials_in_child_process_called_ = true;

#if BUILDFLAG(USE_BLINK)
  // TODO(crbug.com/41403903): Change to a CHECK.
  if (cmd_line.HasSwitch(switches::kFieldTrialHandle)) {
    std::string switch_value =
        cmd_line.GetSwitchValueASCII(switches::kFieldTrialHandle);
    SharedMemoryError result = CreateTrialsFromSwitchValue(switch_value);
    SCOPED_CRASH_KEY_NUMBER("FieldTrialList", "SharedMemoryError",
                            static_cast<int>(result));
    CHECK_EQ(result, SharedMemoryError::kNoError);
  }
#endif  // BUILDFLAG(USE_BLINK)
}

// static
void FieldTrialList::ApplyFeatureOverridesInChildProcess(
    FeatureList* feature_list) {
  CHECK(global_->create_trials_in_child_process_called_);
  // TODO(crbug.com/41403903): Change to a CHECK.
  if (global_->field_trial_allocator_) {
    feature_list->InitFromSharedMemory(global_->field_trial_allocator_.get());
  }
}

#if BUILDFLAG(USE_BLINK)
// static
void FieldTrialList::PopulateLaunchOptionsWithFieldTrialState(
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    GlobalDescriptors::Key descriptor_key,
    ScopedFD& descriptor_to_share,
#endif
    CommandLine* command_line,
    LaunchOptions* launch_options) {
  CHECK(command_line);

  // Use shared memory to communicate field trial state to child processes.
  // The browser is the only process that has write access to the shared memory.
  InstantiateFieldTrialAllocatorIfNeeded();
  CHECK(global_);
  CHECK(global_->readonly_allocator_region_.IsValid());

  global_->field_trial_allocator_->UpdateTrackingHistograms();
  shared_memory::AddToLaunchParameters(
      switches::kFieldTrialHandle,
      global_->readonly_allocator_region_.Duplicate(),
#if BUILDFLAG(IS_APPLE)
      kFieldTrialRendezvousKey,
#elif BUILDFLAG(IS_POSIX)
      descriptor_key, descriptor_to_share,
#endif
      command_line, launch_options);

  // Append --enable-features and --disable-features switches corresponding
  // to the features enabled on the command-line, so that child and browser
  // process command lines match and clearly show what has been specified
  // explicitly by the user.
  std::string enabled_features;
  std::string disabled_features;
  FeatureList::GetInstance()->GetCommandLineFeatureOverrides(
      &enabled_features, &disabled_features);

  if (!enabled_features.empty()) {
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    enabled_features);
  }
  if (!disabled_features.empty()) {
    command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                    disabled_features);
  }
}
#endif  // BUILDFLAG(USE_BLINK)

// static
ReadOnlySharedMemoryRegion
FieldTrialList::DuplicateFieldTrialSharedMemoryForTesting() {
  if (!global_)
    return ReadOnlySharedMemoryRegion();

  return global_->readonly_allocator_region_.Duplicate();
}

// static
FieldTrial* FieldTrialList::CreateFieldTrial(std::string_view name,
                                             std::string_view group_name,
                                             bool is_low_anonymity,
                                             bool is_overridden) {
  DCHECK(global_);
  DCHECK_GE(name.size(), 0u);
  DCHECK_GE(group_name.size(), 0u);
  if (name.empty() || group_name.empty() || !global_)
    return nullptr;

  FieldTrial* field_trial = FieldTrialList::Find(name);
  if (field_trial) {
    // In single process mode, or when we force them from the command line,
    // we may have already created the field trial.
    if (field_trial->group_name_internal() != group_name)
      return nullptr;
    return field_trial;
  }
  const int kTotalProbability = 100;
  field_trial = new FieldTrial(name, kTotalProbability, group_name, 0,
                               is_low_anonymity, is_overridden);
  // The group choice will be finalized in this method. So
  // |is_randomized_trial| should be false.
  FieldTrialList::Register(field_trial, /*is_randomized_trial=*/false);
  // Force the trial, which will also finalize the group choice.
  field_trial->SetForced();
  return field_trial;
}

// static
bool FieldTrialList::AddObserver(Observer* observer) {
  return FieldTrialList::AddObserverInternal(observer,
                                             /*include_low_anonymity=*/false);
}

// static
void FieldTrialList::RemoveObserver(Observer* observer) {
  FieldTrialList::RemoveObserverInternal(observer,
                                         /*include_low_anonymity=*/false);
}

// static
void FieldTrialList::NotifyFieldTrialGroupSelection(FieldTrial* field_trial) {
  if (!global_)
    return;

  std::vector<raw_ptr<Observer, VectorExperimental>> local_observers;
  std::vector<raw_ptr<Observer, VectorExperimental>>
      local_observers_including_low_anonymity;

  {
    AutoLock auto_lock(global_->lock_);
    if (field_trial->group_reported_)
      return;
    field_trial->group_reported_ = true;

    ++global_->num_ongoing_notify_field_trial_group_selection_calls_;

    ActivateFieldTrialEntryWhileLocked(field_trial);

    // Copy observers to a local variable to access outside the scope of the
    // lock. Since removing observers concurrently with this method is
    // disallowed, pointers should remain valid while observers are notified.
    local_observers = global_->observers_;
    local_observers_including_low_anonymity =
        global_->observers_including_low_anonymity_;
  }

  if (!field_trial->is_low_anonymity_) {
    for (Observer* observer : local_observers) {
      observer->OnFieldTrialGroupFinalized(*field_trial,
                                           field_trial->group_name_internal());
    }
  }

  for (Observer* observer : local_observers_including_low_anonymity) {
    observer->OnFieldTrialGroupFinalized(*field_trial,
                                         field_trial->group_name_internal());
  }

  int previous_num_ongoing_notify_field_trial_group_selection_calls =
      global_->num_ongoing_notify_field_trial_group_selection_calls_--;
  DCHECK_GT(previous_num_ongoing_notify_field_trial_group_selection_calls, 0);
}

// static
size_t FieldTrialList::GetFieldTrialCount() {
  if (!global_)
    return 0;
  AutoLock auto_lock(global_->lock_);
  return global_->registered_.size();
}

// static
size_t FieldTrialList::GetRandomizedFieldTrialCount() {
  if (!global_)
    return 0;
  AutoLock auto_lock(global_->lock_);
  return global_->num_registered_randomized_trials_;
}

// static
bool FieldTrialList::GetParamsFromSharedMemory(
    FieldTrial* field_trial,
    std::map<std::string, std::string>* params) {
  DCHECK(global_);
  // If the field trial allocator is not set up yet, then there are several
  // cases:
  //   - We are in the browser process and the allocator has not been set up
  //   yet. If we got here, then we couldn't find the params in
  //   FieldTrialParamAssociator, so it's definitely not here. Return false.
  //   - Using shared memory for field trials is not enabled. If we got here,
  //   then there's nothing in shared memory. Return false.
  //   - We are in the child process and the allocator has not been set up yet.
  //   If this is the case, then you are calling this too early. The field trial
  //   allocator should get set up very early in the lifecycle. Try to see if
  //   you can call it after it's been set up.
  AutoLock auto_lock(global_->lock_);
  if (!global_->field_trial_allocator_)
    return false;

  // If ref_ isn't set, then the field trial data can't be in shared memory.
  if (!field_trial->ref_)
    return false;

  const FieldTrial::FieldTrialEntry* entry =
      global_->field_trial_allocator_->GetAsObject<FieldTrial::FieldTrialEntry>(
          field_trial->ref_);

  size_t allocated_size =
      global_->field_trial_allocator_->GetAllocSize(field_trial->ref_);
  uint64_t actual_size =
      sizeof(FieldTrial::FieldTrialEntry) + entry->pickle_size;
  if (allocated_size < actual_size)
    return false;

  return entry->GetParams(params);
}

// static
void FieldTrialList::ClearParamsFromSharedMemoryForTesting() {
  if (!global_)
    return;

  AutoLock auto_lock(global_->lock_);
  if (!global_->field_trial_allocator_)
    return;

  // To clear the params, we iterate through every item in the allocator, copy
  // just the trial and group name into a newly-allocated segment and then clear
  // the existing item.
  FieldTrialAllocator* allocator = global_->field_trial_allocator_.get();
  FieldTrialAllocator::Iterator mem_iter(allocator);

  // List of refs to eventually be made iterable. We can't make it in the loop,
  // since it would go on forever.
  std::vector<FieldTrial::FieldTrialRef> new_refs;

  FieldTrial::FieldTrialRef prev_ref;
  while ((prev_ref = mem_iter.GetNextOfType<FieldTrial::FieldTrialEntry>()) !=
         FieldTrialAllocator::kReferenceNull) {
    // Get the existing field trial entry in shared memory.
    const FieldTrial::FieldTrialEntry* prev_entry =
        allocator->GetAsObject<FieldTrial::FieldTrialEntry>(prev_ref);
    std::string_view trial_name;
    std::string_view group_name;
    bool is_overridden;
    if (!prev_entry->GetState(trial_name, group_name, is_overridden)) {
      continue;
    }

    // Write a new entry, minus the params.
    Pickle pickle;
    pickle.WriteString(trial_name);
    pickle.WriteString(group_name);
    pickle.WriteBool(is_overridden);

    if (prev_entry->pickle_size == pickle.size() &&
        memcmp(prev_entry->GetPickledDataPtr(), pickle.data(), pickle.size()) ==
            0) {
      // If the new entry is going to be the exact same as the existing one,
      // then simply keep the existing one to avoid taking extra space in the
      // allocator. This should mean that this trial has no params.
      std::map<std::string, std::string> params;
      CHECK(prev_entry->GetParams(&params));
      CHECK(params.empty());
      continue;
    }

    size_t total_size = sizeof(FieldTrial::FieldTrialEntry) + pickle.size();
    FieldTrial::FieldTrialEntry* new_entry =
        allocator->New<FieldTrial::FieldTrialEntry>(total_size);
    DCHECK(new_entry)
        << "Failed to allocate a new entry, likely because the allocator is "
           "full. Consider increasing kFieldTrialAllocationSize.";
    subtle::NoBarrier_Store(&new_entry->activated,
                            subtle::NoBarrier_Load(&prev_entry->activated));
    new_entry->pickle_size = pickle.size();

    // TODO(lawrencewu): Modify base::Pickle to be able to write over a section
    // in memory, so we can avoid this memcpy.
    memcpy(new_entry->GetPickledDataPtr(), pickle.data(), pickle.size());

    // Update the ref on the field trial and add it to the list to be made
    // iterable.
    FieldTrial::FieldTrialRef new_ref = allocator->GetAsReference(new_entry);
    FieldTrial* trial = global_->PreLockedFind(trial_name);
    trial->ref_ = new_ref;
    new_refs.push_back(new_ref);

    // Mark the existing entry as unused.
    allocator->ChangeType(prev_ref, 0,
                          FieldTrial::FieldTrialEntry::kPersistentTypeId,
                          /*clear=*/false);
  }

  for (const auto& ref : new_refs) {
    allocator->MakeIterable(ref);
  }
}

// static
void FieldTrialList::DumpAllFieldTrialsToPersistentAllocator(
    PersistentMemoryAllocator* allocator) {
  if (!global_)
    return;
  AutoLock auto_lock(global_->lock_);
  for (const auto& registered : global_->registered_) {
    AddToAllocatorWhileLocked(allocator, registered.second);
  }
}

// static
std::vector<const FieldTrial::FieldTrialEntry*>
FieldTrialList::GetAllFieldTrialsFromPersistentAllocator(
    PersistentMemoryAllocator const& allocator) {
  std::vector<const FieldTrial::FieldTrialEntry*> entries;
  FieldTrialAllocator::Iterator iter(&allocator);
  const FieldTrial::FieldTrialEntry* entry;
  while ((entry = iter.GetNextOfObject<FieldTrial::FieldTrialEntry>()) !=
         nullptr) {
    entries.push_back(entry);
  }
  return entries;
}

// static
FieldTrialList* FieldTrialList::GetInstance() {
  return global_;
}

// static
FieldTrialList* FieldTrialList::ResetInstance() {
  FieldTrialList* instance = global_;
  instance->was_reset_ = true;
  global_ = nullptr;
  return instance;
}

// static
FieldTrialList* FieldTrialList::BackupInstanceForTesting() {
  FieldTrialList* instance = global_;
  global_ = nullptr;
  return instance;
}

// static
void FieldTrialList::RestoreInstanceForTesting(FieldTrialList* instance) {
  global_ = instance;
}

#if BUILDFLAG(USE_BLINK)

// static
SharedMemoryError FieldTrialList::CreateTrialsFromSwitchValue(
    const std::string& switch_value) {
  auto shm = shared_memory::ReadOnlySharedMemoryRegionFrom(switch_value);
  if (!shm.has_value()) {
    return shm.error();
  }
  if (!FieldTrialList::CreateTrialsFromSharedMemoryRegion(shm.value())) {
    return SharedMemoryError::kCreateTrialsFailed;
  }
  return SharedMemoryError::kNoError;
}

#endif  // BUILDFLAG(USE_BLINK)

// static
bool FieldTrialList::CreateTrialsFromSharedMemoryRegion(
    const ReadOnlySharedMemoryRegion& shm_region) {
  ReadOnlySharedMemoryMapping shm_mapping =
      shm_region.MapAt(0, kFieldTrialAllocationSize);
  if (!shm_mapping.IsValid())
    OnOutOfMemory(kFieldTrialAllocationSize);

  return FieldTrialList::CreateTrialsFromSharedMemoryMapping(
      std::move(shm_mapping));
}

// static
bool FieldTrialList::CreateTrialsFromSharedMemoryMapping(
    ReadOnlySharedMemoryMapping shm_mapping) {
  global_->field_trial_allocator_ =
      std::make_unique<ReadOnlySharedPersistentMemoryAllocator>(
          std::move(shm_mapping), 0, kAllocatorName);
  FieldTrialAllocator* shalloc = global_->field_trial_allocator_.get();
  FieldTrialAllocator::Iterator mem_iter(shalloc);

  const FieldTrial::FieldTrialEntry* entry;
  while ((entry = mem_iter.GetNextOfObject<FieldTrial::FieldTrialEntry>()) !=
         nullptr) {
    std::string_view trial_name;
    std::string_view group_name;
    bool is_overridden;
    if (!entry->GetState(trial_name, group_name, is_overridden)) {
      return false;
    }
    // TODO(crbug.com/40263398): Don't set is_low_anonymity=false, but instead
    // propagate the is_low_anonymity state to the child process.
    FieldTrial* trial = CreateFieldTrial(
        trial_name, group_name, /*is_low_anonymity=*/false, is_overridden);
    trial->ref_ = mem_iter.GetAsReference(entry);
    if (subtle::NoBarrier_Load(&entry->activated)) {
      // Mark the trial as "used" and notify observers, if any.
      // This is useful to ensure that field trials created in child
      // processes are properly reported in crash reports.
      trial->Activate();
    }
  }
  return true;
}

// static
void FieldTrialList::InstantiateFieldTrialAllocatorIfNeeded() {
  if (!global_)
    return;

  AutoLock auto_lock(global_->lock_);
  // Create the allocator if not already created and add all existing trials.
  if (global_->field_trial_allocator_ != nullptr)
    return;

  MappedReadOnlyRegion shm =
      ReadOnlySharedMemoryRegion::Create(kFieldTrialAllocationSize);

  if (!shm.IsValid())
    OnOutOfMemory(kFieldTrialAllocationSize);

  global_->field_trial_allocator_ =
      std::make_unique<WritableSharedPersistentMemoryAllocator>(
          std::move(shm.mapping), 0, kAllocatorName);
  global_->field_trial_allocator_->CreateTrackingHistograms(kAllocatorName);

  // Add all existing field trials.
  for (const auto& registered : global_->registered_) {
    AddToAllocatorWhileLocked(global_->field_trial_allocator_.get(),
                              registered.second);
  }

  // Add all existing features.
  FeatureList::GetInstance()->AddFeaturesToAllocator(
      global_->field_trial_allocator_.get());

  global_->readonly_allocator_region_ = std::move(shm.region);
}

// static
void FieldTrialList::AddToAllocatorWhileLocked(
    PersistentMemoryAllocator* allocator,
    FieldTrial* field_trial) {
  // Don't do anything if the allocator hasn't been instantiated yet.
  if (allocator == nullptr)
    return;

  // Or if the allocator is read only, which means we are in a child process and
  // shouldn't be writing to it.
  if (allocator->IsReadonly())
    return;

  FieldTrial::PickleState trial_state;
  field_trial->GetStateWhileLocked(&trial_state);

  // Or if we've already added it. We must check after GetState since it can
  // also add to the allocator.
  if (field_trial->ref_)
    return;

  Pickle pickle;
  PickleFieldTrial(trial_state, &pickle);

  size_t total_size = sizeof(FieldTrial::FieldTrialEntry) + pickle.size();
  FieldTrial::FieldTrialRef ref = allocator->Allocate(
      total_size, FieldTrial::FieldTrialEntry::kPersistentTypeId);
  if (ref == FieldTrialAllocator::kReferenceNull) {
    NOTREACHED();
  }

  FieldTrial::FieldTrialEntry* entry =
      allocator->GetAsObject<FieldTrial::FieldTrialEntry>(ref);
  subtle::NoBarrier_Store(&entry->activated, trial_state.activated);
  entry->pickle_size = pickle.size();

  // TODO(lawrencewu): Modify base::Pickle to be able to write over a section in
  // memory, so we can avoid this memcpy.
  memcpy(entry->GetPickledDataPtr(), pickle.data(), pickle.size());

  allocator->MakeIterable(ref);
  field_trial->ref_ = ref;
}

// static
void FieldTrialList::ActivateFieldTrialEntryWhileLocked(
    FieldTrial* field_trial) {
  FieldTrialAllocator* allocator = global_->field_trial_allocator_.get();

  // Check if we're in the child process and return early if so.
  if (!allocator || allocator->IsReadonly())
    return;

  FieldTrial::FieldTrialRef ref = field_trial->ref_;
  if (ref == FieldTrialAllocator::kReferenceNull) {
    // It's fine to do this even if the allocator hasn't been instantiated
    // yet -- it'll just return early.
    AddToAllocatorWhileLocked(allocator, field_trial);
  } else {
    // It's also okay to do this even though the callee doesn't have a lock --
    // the only thing that happens on a stale read here is a slight performance
    // hit from the child re-synchronizing activation state.
    FieldTrial::FieldTrialEntry* entry =
        allocator->GetAsObject<FieldTrial::FieldTrialEntry>(ref);
    subtle::NoBarrier_Store(&entry->activated, 1);
  }
}

FieldTrial* FieldTrialList::PreLockedFind(std::string_view name) {
  auto it = registered_.find(name);
  if (registered_.end() == it)
    return nullptr;
  return it->second;
}

// static
void FieldTrialList::Register(FieldTrial* trial, bool is_randomized_trial) {
  DCHECK(global_);

  AutoLock auto_lock(global_->lock_);
  CHECK(!global_->PreLockedFind(trial->trial_name())) << trial->trial_name();
  trial->AddRef();
  trial->SetTrialRegistered();
  global_->registered_[trial->trial_name()] = trial;

  if (is_randomized_trial)
    ++global_->num_registered_randomized_trials_;
}

// static
FieldTrialList::RegistrationMap FieldTrialList::GetRegisteredTrials() {
  RegistrationMap output;
  if (global_) {
    AutoLock auto_lock(global_->lock_);
    output = global_->registered_;
  }
  return output;
}

// static
bool FieldTrialList::CreateTrialsFromFieldTrialStatesInternal(
    const std::vector<FieldTrial::State>& entries) {
  DCHECK(global_);

  for (const auto& entry : entries) {
    FieldTrial* trial =
        CreateFieldTrial(entry.trial_name, entry.group_name,
                         /*is_low_anonymity=*/false, entry.is_overridden);
    if (!trial)
      return false;
    if (entry.activated) {
      // Mark the trial as "used" and notify observers, if any.
      // This is useful to ensure that field trials created in child
      // processes are properly reported in crash reports.
      trial->Activate();
    }
  }
  return true;
}

// static
void FieldTrialList::GetActiveFieldTrialGroupsInternal(
    FieldTrial::ActiveGroups* active_groups,
    bool include_low_anonymity) {
  DCHECK(active_groups->empty());
  if (!global_) {
    return;
  }
  AutoLock auto_lock(global_->lock_);

  for (const auto& registered : global_->registered_) {
    const FieldTrial& trial = *registered.second;
    FieldTrial::ActiveGroup active_group;
    if ((include_low_anonymity || !trial.is_low_anonymity_) &&
        trial.GetActiveGroup(&active_group)) {
      active_groups->push_back(active_group);
    }
  }
}

// static
bool FieldTrialList::AddObserverInternal(Observer* observer,
                                         bool include_low_anonymity) {
  if (!global_) {
    return false;
  }
  AutoLock auto_lock(global_->lock_);
  if (include_low_anonymity) {
    global_->observers_including_low_anonymity_.push_back(observer);
  } else {
    global_->observers_.push_back(observer);
  }
  return true;
}

// static
void FieldTrialList::RemoveObserverInternal(Observer* observer,
                                            bool include_low_anonymity) {
  if (!global_) {
    return;
  }
  AutoLock auto_lock(global_->lock_);
  std::erase(include_low_anonymity ? global_->observers_including_low_anonymity_
                                   : global_->observers_,
             observer);
  DCHECK_EQ(global_->num_ongoing_notify_field_trial_group_selection_calls_, 0)
      << "Cannot call RemoveObserver while accessing FieldTrial::group_name().";
}

}  // namespace base
