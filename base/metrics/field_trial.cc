// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_BLINK)
#include "base/process/launch.h"
#endif

#if BUILDFLAG(IS_APPLE) && BUILDFLAG(USE_BLINK)
#include "base/mac/mach_port_rendezvous.h"
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
                     const StringPiece& string1,
                     const StringPiece& string2) {
  pickle->WriteString(string1);
  pickle->WriteString(string2);
}

// Writes out the field trial's contents (via trial_state) to the pickle. The
// format of the pickle looks like:
// TrialName, GroupName, ParamKey1, ParamValue1, ParamKey2, ParamValue2, ...
// If there are no parameters, then it just ends at GroupName.
void PickleFieldTrial(const FieldTrial::PickleState& trial_state,
                      Pickle* pickle) {
  WriteStringPair(pickle, *trial_state.trial_name, *trial_state.group_name);

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

// Parses the --force-fieldtrials string |trials_string| into |entries|.
// Returns true if the string was parsed correctly. On failure, the |entries|
// array may end up being partially filled.
bool ParseFieldTrialsString(const std::string& trials_string,
                            std::vector<FieldTrial::State>* entries) {
  const StringPiece trials_string_piece(trials_string);

  size_t next_item = 0;
  while (next_item < trials_string.length()) {
    size_t name_end = trials_string.find(kPersistentStringSeparator, next_item);
    if (name_end == trials_string.npos || next_item == name_end)
      return false;
    size_t group_name_end =
        trials_string.find(kPersistentStringSeparator, name_end + 1);
    if (name_end + 1 == group_name_end)
      return false;
    if (group_name_end == trials_string.npos)
      group_name_end = trials_string.length();

    FieldTrial::State entry;
    // Verify if the trial should be activated or not.
    if (trials_string[next_item] == kActivationMarker) {
      // Name cannot be only the indicator.
      if (name_end - next_item == 1)
        return false;
      next_item++;
      entry.activated = true;
    }
    entry.trial_name =
        trials_string_piece.substr(next_item, name_end - next_item);
    entry.group_name =
        trials_string_piece.substr(name_end + 1, group_name_end - name_end - 1);
    next_item = group_name_end + 1;

    entries->push_back(std::move(entry));
  }
  return true;
}

void OnOutOfMemory(size_t size) {
  TerminateBecauseOutOfMemory(size);
}

#if BUILDFLAG(USE_BLINK)
#if BUILDFLAG(IS_POSIX)
// Exits the process gracefully if the parent process is dead. We've seen cases
// where the child will still be executing after its parent process has died.
// In those cases, if we hit an error that would otherwise result in a CHECK,
// this function can be used to exit gracefully instead of producing a crash
// report. Note: This function calls Sleep() so should not be called in a code
// path that wouldn't otherwise result in a CHECK().
void ExitGracefullyIfParentProcessIsDead() {
  // The parent process crash may not be visible immediately so loop for 100ms.
  for (int i = 0; i < 100; i++) {
    // If the parent process has died, getppid() will return 1, meaning we were
    // orphaned and parented to init.
    if (getppid() == 1) {
      base::Process::TerminateCurrentProcessImmediately(0);
    }
    PlatformThread::Sleep(base::Milliseconds(1));
  }
}
#endif  // BUILDFLAG(IS_POSIX)

// Returns whether the operation succeeded.
bool DeserializeGUIDFromStringPieces(StringPiece first,
                                     StringPiece second,
                                     UnguessableToken* guid) {
  uint64_t high = 0;
  uint64_t low = 0;
  if (!StringToUint64(first, &high) || !StringToUint64(second, &low))
    return false;

  absl::optional<UnguessableToken> token =
      UnguessableToken::Deserialize(high, low);
  if (!token.has_value()) {
    return false;
  }

  *guid = token.value();
  return true;
}
#endif  // BUILDFLAG(USE_BLINK)

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

bool FieldTrial::FieldTrialEntry::GetTrialAndGroupName(
    StringPiece* trial_name,
    StringPiece* group_name) const {
  PickleIterator iter = GetPickleIterator();
  return ReadStringPair(&iter, trial_name, group_name);
}

bool FieldTrial::FieldTrialEntry::GetParams(
    std::map<std::string, std::string>* params) const {
  PickleIterator iter = GetPickleIterator();
  StringPiece tmp;
  // Skip reading trial and group name.
  if (!ReadStringPair(&iter, &tmp, &tmp))
    return false;

  while (true) {
    StringPiece key;
    StringPiece value;
    if (!ReadStringPair(&iter, &key, &value))
      return key.empty();  // Non-empty is bad: got one of a pair.
    (*params)[std::string(key)] = std::string(value);
  }
}

PickleIterator FieldTrial::FieldTrialEntry::GetPickleIterator() const {
  const char* src =
      reinterpret_cast<const char*>(this) + sizeof(FieldTrialEntry);

  Pickle pickle(src, checked_cast<size_t>(pickle_size));
  return PickleIterator(pickle);
}

bool FieldTrial::FieldTrialEntry::ReadStringPair(
    PickleIterator* iter,
    StringPiece* trial_name,
    StringPiece* group_name) const {
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
    StringPiece trial_name,
    Probability total_probability,
    StringPiece default_group_name,
    double entropy_value) {
  return new FieldTrial(trial_name, total_probability, default_group_name,
                        entropy_value, /*is_low_anonymity=*/false);
}

FieldTrial::FieldTrial(StringPiece trial_name,
                       const Probability total_probability,
                       StringPiece default_group_name,
                       double entropy_value,
                       bool is_low_anonymity)
    : trial_name_(trial_name),
      divisor_(total_probability),
      default_group_name_(default_group_name),
      random_(GetGroupBoundaryValue(total_probability, entropy_value)),
      accumulated_group_probability_(0),
      next_group_number_(kDefaultGroupNumber + 1),
      group_(kNotFinalized),
      forced_(false),
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
  return true;
}

void FieldTrial::GetStateWhileLocked(PickleState* field_trial_state) {
  FinalizeGroupChoice();
  field_trial_state->trial_name = &trial_name_;
  field_trial_state->group_name = &group_name_;
  field_trial_state->activated = group_reported_;
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
  DCHECK_EQ(this, global_);
  global_ = nullptr;
}

// static
FieldTrial* FieldTrialList::FactoryGetFieldTrial(
    StringPiece trial_name,
    FieldTrial::Probability total_probability,
    StringPiece default_group_name,
    const FieldTrial::EntropyProvider& entropy_provider,
    uint32_t randomization_seed,
    bool is_low_anonymity) {
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
                     entropy_value, is_low_anonymity);
  FieldTrialList::Register(field_trial, /*is_randomized_trial=*/true);
  return field_trial;
}

// static
FieldTrial* FieldTrialList::Find(StringPiece trial_name) {
  if (!global_)
    return nullptr;
  AutoLock auto_lock(global_->lock_);
  return global_->PreLockedFind(trial_name);
}

// static
std::string FieldTrialList::FindFullName(StringPiece trial_name) {
  FieldTrial* field_trial = Find(trial_name);
  if (field_trial)
    return field_trial->group_name();
  return std::string();
}

// static
bool FieldTrialList::TrialExists(StringPiece trial_name) {
  return Find(trial_name) != nullptr;
}

// static
bool FieldTrialList::IsTrialActive(StringPiece trial_name) {
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
    DCHECK_EQ(std::string::npos,
              trial.trial_name->find(kPersistentStringSeparator));
    DCHECK_EQ(std::string::npos,
              trial.group_name->find(kPersistentStringSeparator));
    if (trial.activated)
      output->append(1, kActivationMarker);
    output->append(*trial.trial_name);
    output->append(1, kPersistentStringSeparator);
    output->append(*trial.group_name);
    output->append(1, kPersistentStringSeparator);
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
          if (!param_str.empty())
            param_str.append(1, kPersistentStringSeparator);
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
    StringPiece trial_name;
    StringPiece group_name;
    if (subtle::NoBarrier_Load(&entry->activated) &&
        entry->GetTrialAndGroupName(&trial_name, &group_name)) {
      result.insert(std::string(trial_name));
    }
  }
  return result;
}

// static
bool FieldTrialList::CreateTrialsFromString(const std::string& trials_string) {
  DCHECK(global_);
  if (trials_string.empty() || !global_)
    return true;

  std::vector<FieldTrial::State> entries;
  if (!ParseFieldTrialsString(trials_string, &entries))
    return false;

  return CreateTrialsFromFieldTrialStatesInternal(entries);
}

// static
bool FieldTrialList::CreateTrialsFromFieldTrialStates(
    PassKey<test::ScopedFeatureList>,
    const std::vector<FieldTrial::State>& entries) {
  return CreateTrialsFromFieldTrialStatesInternal(entries);
}

// static
void FieldTrialList::CreateTrialsInChildProcess(const CommandLine& cmd_line,
                                                uint32_t fd_key) {
  global_->create_trials_in_child_process_called_ = true;

#if BUILDFLAG(USE_BLINK)
  // TODO(crbug.com/867558): Change to a CHECK.
  if (cmd_line.HasSwitch(switches::kFieldTrialHandle)) {
    std::string switch_value =
        cmd_line.GetSwitchValueASCII(switches::kFieldTrialHandle);
    bool result = CreateTrialsFromSwitchValue(switch_value, fd_key);
#if BUILDFLAG(IS_POSIX)
    if (!result) {
      // This may be an error mapping the shared memory segment if the parent
      // process just died. Exit gracefully in this case.
      ExitGracefullyIfParentProcessIsDead();
    }
#endif  // BUILDFLAG(IS_POSIX)
    CHECK(result);
  }
#endif  // BUILDFLAG(USE_BLINK)
}

// static
void FieldTrialList::ApplyFeatureOverridesInChildProcess(
    FeatureList* feature_list) {
  CHECK(global_->create_trials_in_child_process_called_);
  // TODO(crbug.com/867558): Change to a CHECK.
  if (global_->field_trial_allocator_) {
    feature_list->InitializeFromSharedMemory(
        global_->field_trial_allocator_.get());
  }
}

#if BUILDFLAG(USE_BLINK)
// static
void FieldTrialList::PopulateLaunchOptionsWithFieldTrialState(
    CommandLine* command_line,
    LaunchOptions* launch_options) {
  CHECK(command_line);

  // Use shared memory to communicate field trial state to child processes.
  // The browser is the only process that has write access to the shared memory.
  InstantiateFieldTrialAllocatorIfNeeded();
  CHECK(global_);
  CHECK(global_->readonly_allocator_region_.IsValid());

  global_->field_trial_allocator_->UpdateTrackingHistograms();
  std::string switch_value = SerializeSharedMemoryRegionMetadata(
      global_->readonly_allocator_region_, launch_options);
  command_line->AppendSwitchASCII(switches::kFieldTrialHandle, switch_value);

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

#if BUILDFLAG(USE_BLINK) && BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
// static
int FieldTrialList::GetFieldTrialDescriptor() {
  InstantiateFieldTrialAllocatorIfNeeded();
  if (!global_ || !global_->readonly_allocator_region_.IsValid())
    return -1;

#if BUILDFLAG(IS_ANDROID)
  return global_->readonly_allocator_region_.GetPlatformHandle();
#else
  return global_->readonly_allocator_region_.GetPlatformHandle().fd;
#endif
}
#endif  // BUILDFLAG(USE_BLINK) && BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)

// static
ReadOnlySharedMemoryRegion
FieldTrialList::DuplicateFieldTrialSharedMemoryForTesting() {
  if (!global_)
    return ReadOnlySharedMemoryRegion();

  return global_->readonly_allocator_region_.Duplicate();
}

// static
FieldTrial* FieldTrialList::CreateFieldTrial(StringPiece name,
                                             StringPiece group_name,
                                             bool is_low_anonymity) {
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
  field_trial =
      new FieldTrial(name, kTotalProbability, group_name, 0, is_low_anonymity);
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

  std::vector<Observer*> local_observers;
  std::vector<Observer*> local_observers_including_low_anonymity;

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
      observer->OnFieldTrialGroupFinalized(field_trial->trial_name(),
                                           field_trial->group_name_internal());
    }
  }

  for (Observer* observer : local_observers_including_low_anonymity) {
    observer->OnFieldTrialGroupFinalized(field_trial->trial_name(),
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
    StringPiece trial_name;
    StringPiece group_name;
    if (!prev_entry->GetTrialAndGroupName(&trial_name, &group_name))
      continue;

    // Write a new entry, minus the params.
    Pickle pickle;
    pickle.WriteString(trial_name);
    pickle.WriteString(group_name);
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
    char* dst = reinterpret_cast<char*>(new_entry) +
                sizeof(FieldTrial::FieldTrialEntry);
    memcpy(dst, pickle.data(), pickle.size());

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
std::string FieldTrialList::SerializeSharedMemoryRegionMetadata(
    const ReadOnlySharedMemoryRegion& shm,
    LaunchOptions* launch_options) {
  std::stringstream ss;
#if BUILDFLAG(IS_WIN)
  // Elevated process might not need this, although it is harmless.
  launch_options->handles_to_inherit.push_back(shm.GetPlatformHandle());

  // Tell the child process the name of the inherited HANDLE.
  uintptr_t uintptr_handle =
      reinterpret_cast<uintptr_t>(shm.GetPlatformHandle());
  ss << uintptr_handle << ",";
  if (launch_options->elevated) {
    // Tell the child that it must open its parent and grab the handle.
    ss << "p,";
  } else {
    // Tell the child that it inherited the handle.
    ss << "i,";
  }
#elif BUILDFLAG(IS_APPLE)
  launch_options->mach_ports_for_rendezvous.emplace(
      kFieldTrialRendezvousKey,
      MachRendezvousPort(shm.GetPlatformHandle(), MACH_MSG_TYPE_COPY_SEND));

  // The handle on Mac is looked up directly by the child, rather than being
  // transferred to the child over the command line.
  ss << kFieldTrialRendezvousKey << ",";
  // Tell the child that the handle is looked up.
  ss << "r,";
#elif BUILDFLAG(IS_FUCHSIA)
  zx::vmo transfer_vmo;
  zx_status_t status = shm.GetPlatformHandle()->duplicate(
      ZX_RIGHT_READ | ZX_RIGHT_MAP | ZX_RIGHT_TRANSFER | ZX_RIGHT_GET_PROPERTY |
          ZX_RIGHT_DUPLICATE,
      &transfer_vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_handle_duplicate";

  // The handle on Fuchsia is passed as part of the launch handles to transfer.
  uint32_t handle_id = LaunchOptions::AddHandleToTransfer(
      &launch_options->handles_to_transfer, transfer_vmo.release());
  ss << handle_id << ",";
  // Tell the child that the handle is inherited.
  ss << "i,";
#elif BUILDFLAG(IS_POSIX)
  // This is actually unused in the child process, but allows non-Mac Posix
  // platforms to have the same format as the others.
  ss << "0,i,";
#else
#error Unsupported OS
#endif

  UnguessableToken guid = shm.GetGUID();
  ss << guid.GetHighForSerialization() << "," << guid.GetLowForSerialization();
  ss << "," << shm.GetSize();
  return ss.str();
}

// static
ReadOnlySharedMemoryRegion
FieldTrialList::DeserializeSharedMemoryRegionMetadata(
    const std::string& switch_value,
    int fd) {
  // Format: "handle,[irp],guid-high,guid-low,size".
  std::vector<StringPiece> tokens =
      SplitStringPiece(switch_value, ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);

  if (tokens.size() != 5)
    return ReadOnlySharedMemoryRegion();

  int field_trial_handle = 0;
  if (!StringToInt(tokens[0], &field_trial_handle))
    return ReadOnlySharedMemoryRegion();

    // token[1] has a fixed value but is ignored on all platforms except
    // Windows, where it can be 'i' or 'p' to indicate that the handle is
    // inherited or must be obtained from the parent.
#if BUILDFLAG(IS_WIN)
  HANDLE handle = reinterpret_cast<HANDLE>(field_trial_handle);
  if (tokens[1] == "p") {
    DCHECK(IsCurrentProcessElevated());
    // LaunchProcess doesn't have a way to duplicate the handle, but this
    // process can since by definition it's not sandboxed.
    ProcessId parent_pid = GetParentProcessId(GetCurrentProcess());
    HANDLE parent_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, parent_pid);
    // TODO(https://crbug.com/916461): Duplicating the handle is known to fail
    // with ERROR_ACCESS_DENIED when the parent process is being torn down. This
    // should be handled elegantly somehow.
    DuplicateHandle(parent_handle, handle, GetCurrentProcess(), &handle, 0,
                    FALSE, DUPLICATE_SAME_ACCESS);
    CloseHandle(parent_handle);
  } else if (tokens[1] != "i") {
    return ReadOnlySharedMemoryRegion();
  }
  win::ScopedHandle scoped_handle(handle);
#elif BUILDFLAG(IS_APPLE) && BUILDFLAG(USE_BLINK)
  auto* rendezvous = MachPortRendezvousClient::GetInstance();
  if (!rendezvous)
    return ReadOnlySharedMemoryRegion();
  apple::ScopedMachSendRight scoped_handle = rendezvous->TakeSendRight(
      static_cast<MachPortsForRendezvous::key_type>(field_trial_handle));
  if (!scoped_handle.is_valid())
    return ReadOnlySharedMemoryRegion();
#elif BUILDFLAG(IS_FUCHSIA)
  static bool startup_handle_taken = false;
  DCHECK(!startup_handle_taken) << "Shared memory region initialized twice";
  zx::vmo scoped_handle(
      zx_take_startup_handle(checked_cast<uint32_t>(field_trial_handle)));
  startup_handle_taken = true;
  if (!scoped_handle.is_valid())
    return ReadOnlySharedMemoryRegion();
#elif BUILDFLAG(IS_POSIX)
  if (fd == -1)
    return ReadOnlySharedMemoryRegion();
  ScopedFD scoped_handle(fd);
#else
#error Unsupported OS
#endif

  UnguessableToken guid;
  if (!DeserializeGUIDFromStringPieces(tokens[2], tokens[3], &guid))
    return ReadOnlySharedMemoryRegion();

  int size;
  if (!StringToInt(tokens[4], &size))
    return ReadOnlySharedMemoryRegion();

  auto platform_handle = subtle::PlatformSharedMemoryRegion::Take(
      std::move(scoped_handle),
      subtle::PlatformSharedMemoryRegion::Mode::kReadOnly,
      static_cast<size_t>(size), guid);
  return ReadOnlySharedMemoryRegion::Deserialize(std::move(platform_handle));
}

// static
bool FieldTrialList::CreateTrialsFromSwitchValue(
    const std::string& switch_value,
    uint32_t fd_key) {
  int fd = -1;
#if BUILDFLAG(IS_POSIX)
  fd = GlobalDescriptors::GetInstance()->MaybeGet(fd_key);
  if (fd == -1)
    return false;
#endif  // BUILDFLAG(IS_POSIX)
  ReadOnlySharedMemoryRegion shm =
      DeserializeSharedMemoryRegionMetadata(switch_value, fd);
  if (!shm.IsValid()) {
    return false;
  }
  return FieldTrialList::CreateTrialsFromSharedMemoryRegion(shm);
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
    StringPiece trial_name;
    StringPiece group_name;
    if (!entry->GetTrialAndGroupName(&trial_name, &group_name))
      return false;

    FieldTrial* trial = CreateFieldTrial(trial_name, group_name);
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
    return;
  }

  FieldTrial::FieldTrialEntry* entry =
      allocator->GetAsObject<FieldTrial::FieldTrialEntry>(ref);
  subtle::NoBarrier_Store(&entry->activated, trial_state.activated);
  entry->pickle_size = pickle.size();

  // TODO(lawrencewu): Modify base::Pickle to be able to write over a section in
  // memory, so we can avoid this memcpy.
  char* dst =
      reinterpret_cast<char*>(entry) + sizeof(FieldTrial::FieldTrialEntry);
  memcpy(dst, pickle.data(), pickle.size());

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

FieldTrial* FieldTrialList::PreLockedFind(StringPiece name) {
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
    FieldTrial* trial = CreateFieldTrial(entry.trial_name, entry.group_name);
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
  if (include_low_anonymity) {
    Erase(global_->observers_including_low_anonymity_, observer);
  } else {
    Erase(global_->observers_, observer);
  }
  DCHECK_EQ(global_->num_ongoing_notify_field_trial_group_selection_calls_, 0)
      << "Cannot call RemoveObserver while accessing FieldTrial::group_name().";
}

}  // namespace base
