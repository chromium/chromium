// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_database_win.h"

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "base/feature_list.h"
#include "base/task/post_task.h"
#include "base/win/win_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/conflicts/incompatible_applications_updater_win.h"
#include "chrome/browser/conflicts/module_load_attempt_log_listener_win.h"
#include "chrome/browser/conflicts/third_party_conflicts_manager_win.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#endif

namespace {

// Document the assumptions made on the ProcessType enum in order to convert
// them to bits.
static_assert(content::PROCESS_TYPE_UNKNOWN == 1,
              "assumes unknown process type has value 1");
static_assert(content::PROCESS_TYPE_BROWSER == 2,
              "assumes browser process type has value 2");
constexpr uint32_t kFirstValidProcessType = content::PROCESS_TYPE_BROWSER;

ModuleDatabase* g_module_database_win_instance = nullptr;

#if defined(GOOGLE_CHROME_BUILD)
// Returns true if either the IncompatibleApplicationsWarning or
// ThirdPartyModulesBlocking features are enabled via the "enable-features"
// command-line switch.
bool AreThirdPartyFeaturesEnabledViaCommandLine() {
  base::FeatureList* feature_list_instance = base::FeatureList::GetInstance();

  return feature_list_instance->IsFeatureOverriddenFromCommandLine(
             features::kIncompatibleApplicationsWarning.name,
             base::FeatureList::OVERRIDE_ENABLE_FEATURE) ||
         feature_list_instance->IsFeatureOverriddenFromCommandLine(
             features::kThirdPartyModulesBlocking.name,
             base::FeatureList::OVERRIDE_ENABLE_FEATURE);
}
#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace

// static
constexpr base::TimeDelta ModuleDatabase::kIdleTimeout;

ModuleDatabase::ModuleDatabase(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner),
      idle_timer_(
          FROM_HERE,
          kIdleTimeout,
          base::Bind(&ModuleDatabase::OnDelayExpired, base::Unretained(this))),
      has_started_processing_(false),
      shell_extensions_enumerated_(false),
      ime_enumerated_(false),
      // ModuleDatabase owns |module_inspector_|, so it is safe to use
      // base::Unretained().
      module_inspector_(base::Bind(&ModuleDatabase::OnModuleInspected,
                                   base::Unretained(this))) {
  AddObserver(&third_party_metrics_);

#if defined(GOOGLE_CHROME_BUILD)
  MaybeInitializeThirdPartyConflictsManager();
#endif
}

ModuleDatabase::~ModuleDatabase() {
  if (this == g_module_database_win_instance)
    g_module_database_win_instance = nullptr;
}

// static
ModuleDatabase* ModuleDatabase::GetInstance() {
  return g_module_database_win_instance;
}

// static
void ModuleDatabase::SetInstance(
    std::unique_ptr<ModuleDatabase> module_database) {
  DCHECK_EQ(nullptr, g_module_database_win_instance);
  // This is deliberately leaked. It can be cleaned up by manually deleting the
  // ModuleDatabase.
  g_module_database_win_instance = module_database.release();
}

void ModuleDatabase::StartDrainingModuleLoadAttemptsLog() {
#if defined(GOOGLE_CHROME_BUILD)
  // ModuleDatabase owns |module_load_attempt_log_listener_|, so it is safe to
  // use base::Unretained().
  module_load_attempt_log_listener_ =
      std::make_unique<ModuleLoadAttemptLogListener>(base::BindRepeating(
          &ModuleDatabase::OnModuleBlocked, base::Unretained(this)));
#endif  // defined(GOOGLE_CHROME_BUILD)
}

bool ModuleDatabase::IsIdle() {
  return has_started_processing_ && RegisteredModulesEnumerated() &&
         !idle_timer_.IsRunning() && module_inspector_.IsIdle();
}

void ModuleDatabase::OnShellExtensionEnumerated(const base::FilePath& path,
                                                uint32_t size_of_image,
                                                uint32_t time_date_stamp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  idle_timer_.Reset();

  ModuleInfo* module_info = nullptr;
  FindOrCreateModuleInfo(path, size_of_image, time_date_stamp, &module_info);
  module_info->second.module_properties |=
      ModuleInfoData::kPropertyShellExtension;
}

void ModuleDatabase::OnShellExtensionEnumerationFinished() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!shell_extensions_enumerated_);

  shell_extensions_enumerated_ = true;

  if (RegisteredModulesEnumerated())
    OnRegisteredModulesEnumerated();
}

void ModuleDatabase::OnImeEnumerated(const base::FilePath& path,
                                     uint32_t size_of_image,
                                     uint32_t time_date_stamp) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  idle_timer_.Reset();

  ModuleInfo* module_info = nullptr;
  FindOrCreateModuleInfo(path, size_of_image, time_date_stamp, &module_info);
  module_info->second.module_properties |= ModuleInfoData::kPropertyIme;
}

void ModuleDatabase::OnImeEnumerationFinished() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!ime_enumerated_);

  ime_enumerated_ = true;

  if (RegisteredModulesEnumerated())
    OnRegisteredModulesEnumerated();
}

void ModuleDatabase::OnModuleLoad(content::ProcessType process_type,
                                  const base::FilePath& module_path,
                                  uint32_t module_size,
                                  uint32_t module_time_date_stamp) {
  // Messages can arrive from any thread (UI thread for calls over IPC, and
  // anywhere at all for calls from ModuleWatcher), so bounce if necessary.
  // It is safe to use base::Unretained() because this class is a singleton that
  // is never freed.
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&ModuleDatabase::OnModuleLoad,
                              base::Unretained(this), process_type, module_path,
                              module_size, module_time_date_stamp));
    return;
  }

  ModuleInfo* module_info = nullptr;
  bool new_module = FindOrCreateModuleInfo(
      module_path, module_size, module_time_date_stamp, &module_info);

  uint32_t old_module_properties = module_info->second.module_properties;

  // Mark the module as loaded.
  module_info->second.module_properties |=
      ModuleInfoData::kPropertyLoadedModule;

  // Update the list of process types that this module has been seen in.
  module_info->second.process_types |= ProcessTypeToBit(process_type);

  // Some observers care about a known module that is just now loading. Also
  // making sure that the module is ready to be sent to observers.
  bool is_known_module_loading =
      !new_module &&
      old_module_properties != module_info->second.module_properties;
  bool ready_for_notification =
      module_info->second.inspection_result && RegisteredModulesEnumerated();
  if (is_known_module_loading && ready_for_notification) {
    for (auto& observer : observer_list_) {
      observer.OnKnownModuleLoaded(module_info->first, module_info->second);
    }
  }
}

void ModuleDatabase::OnModuleBlocked(const base::FilePath& module_path,
                                     uint32_t module_size,
                                     uint32_t module_time_date_stamp) {
  ModuleInfo* module_info = nullptr;
  FindOrCreateModuleInfo(module_path, module_size, module_time_date_stamp,
                         &module_info);

  module_info->second.module_properties |= ModuleInfoData::kPropertyBlocked;
}

void ModuleDatabase::OnModuleAddedToBlacklist(const base::FilePath& module_path,
                                              uint32_t module_size,
                                              uint32_t module_time_date_stamp) {
  auto iter = modules_.find(
      ModuleInfoKey(module_path, module_size, module_time_date_stamp, 0));

  // Only known modules should be added to the blacklist.
  DCHECK(iter != modules_.end());

  iter->second.module_properties |= ModuleInfoData::kPropertyAddedToBlacklist;
}

void ModuleDatabase::AddObserver(ModuleDatabaseObserver* observer) {
  observer_list_.AddObserver(observer);

  // If the registered modules enumeration is not finished yet, the |observer|
  // will be notified later in OnRegisteredModulesEnumerated().
  if (!RegisteredModulesEnumerated())
    return;

  NotifyLoadedModules(observer);

  if (IsIdle())
    observer->OnModuleDatabaseIdle();
}

void ModuleDatabase::RemoveObserver(ModuleDatabaseObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ModuleDatabase::IncreaseInspectionPriority() {
  module_inspector_.IncreaseInspectionPriority();
}

#if defined(GOOGLE_CHROME_BUILD)
// static
void ModuleDatabase::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // Register the pref used to disable the Incompatible Applications warning and
  // the blocking of third-party modules using group policy. Enabled by default.
  registry->RegisterBooleanPref(prefs::kThirdPartyBlockingEnabled, true);
}

// static
bool ModuleDatabase::IsThirdPartyBlockingPolicyEnabled() {
  const PrefService::Preference* third_party_blocking_enabled_pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kThirdPartyBlockingEnabled);
  return !third_party_blocking_enabled_pref->IsManaged() ||
         third_party_blocking_enabled_pref->GetValue()->GetBool();
}
#endif  // defined(GOOGLE_CHROME_BUILD)

// static
uint32_t ModuleDatabase::ProcessTypeToBit(content::ProcessType process_type) {
  uint32_t bit_index =
      static_cast<uint32_t>(process_type) - kFirstValidProcessType;
  DCHECK_GE(31u, bit_index);
  uint32_t bit = (1 << bit_index);
  return bit;
}

// static
content::ProcessType ModuleDatabase::BitIndexToProcessType(uint32_t bit_index) {
  DCHECK_GE(31u, bit_index);
  return static_cast<content::ProcessType>(bit_index + kFirstValidProcessType);
}

bool ModuleDatabase::FindOrCreateModuleInfo(
    const base::FilePath& module_path,
    uint32_t module_size,
    uint32_t module_time_date_stamp,
    ModuleDatabase::ModuleInfo** module_info) {
  auto result = modules_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(module_path, module_size, module_time_date_stamp,
                            modules_.size()),
      std::forward_as_tuple());

  // New modules must be inspected.
  bool new_module = result.second;
  if (new_module) {
    has_started_processing_ = true;
    idle_timer_.Reset();

    module_inspector_.AddModule(result.first->first);
  }

  *module_info = &(*result.first);
  return new_module;
}

bool ModuleDatabase::RegisteredModulesEnumerated() {
  return shell_extensions_enumerated_ && ime_enumerated_;
}

void ModuleDatabase::OnRegisteredModulesEnumerated() {
  for (auto& observer : observer_list_)
    NotifyLoadedModules(&observer);

  if (IsIdle())
    EnterIdleState();
}

void ModuleDatabase::OnModuleInspected(
    const ModuleInfoKey& module_key,
    std::unique_ptr<ModuleInspectionResult> inspection_result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto it = modules_.find(module_key);
  if (it == modules_.end())
    return;

  it->second.inspection_result = std::move(inspection_result);

  if (RegisteredModulesEnumerated())
    for (auto& observer : observer_list_)
      observer.OnNewModuleFound(it->first, it->second);

  // Notify the observers if this was the last outstanding module inspection and
  // the delay has already expired.
  if (IsIdle())
    EnterIdleState();
}

void ModuleDatabase::OnDelayExpired() {
  // Notify the observers if there are no outstanding module inspections.
  if (IsIdle())
    EnterIdleState();
}

void ModuleDatabase::EnterIdleState() {
  for (auto& observer : observer_list_)
    observer.OnModuleDatabaseIdle();
}

void ModuleDatabase::NotifyLoadedModules(ModuleDatabaseObserver* observer) {
  for (const auto& module : modules_) {
    if (module.second.inspection_result)
      observer->OnNewModuleFound(module.first, module.second);
  }
}

#if defined(GOOGLE_CHROME_BUILD)
void ModuleDatabase::MaybeInitializeThirdPartyConflictsManager() {
  // Temporarily disable this class on domain-joined machines because enterprise
  // clients depend on IAttachmentExecute::Save() to be invoked for downloaded
  // files, but that API call has a known issue (https://crbug.com/870998) with
  // third-party modules blocking. Can be Overridden by enabling the feature via
  // the command-line.
  // TODO(pmonette): Move IAttachmentExecute::Save() to a utility process and
  //                 remove this.
  if (base::win::IsEnterpriseManaged() &&
      !AreThirdPartyFeaturesEnabledViaCommandLine()) {
    return;
  }

  if (!IsThirdPartyBlockingPolicyEnabled())
    return;

  if (IncompatibleApplicationsUpdater::IsWarningEnabled() ||
      base::FeatureList::IsEnabled(features::kThirdPartyModulesBlocking)) {
    third_party_conflicts_manager_ =
        std::make_unique<ThirdPartyConflictsManager>(this);

    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(g_browser_process->local_state());
    pref_change_registrar_->Add(
        prefs::kThirdPartyBlockingEnabled,
        base::Bind(&ModuleDatabase::OnThirdPartyBlockingPolicyChanged,
                   base::Unretained(this)));
  }
}

void ModuleDatabase::OnThirdPartyBlockingPolicyChanged() {
  if (!IsThirdPartyBlockingPolicyEnabled()) {
    DCHECK(third_party_conflicts_manager_);
    ThirdPartyConflictsManager::ShutdownAndDestroy(
        std::move(third_party_conflicts_manager_));
    pref_change_registrar_ = nullptr;
  }
}
#endif
