// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/plugin_metrics_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

// Delay for RecordCurrentState execution.
constexpr base::TimeDelta kRecordStateDelay = base::TimeDelta::FromSeconds(15);

// Returns the plugin preferences corresponding for this user, if available.
// If multiple user profiles are loaded, returns the preferences corresponding
// to an arbitrary one of the profiles.
PluginPrefs* GetPluginPrefs() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  if (!profile_manager) {
    // The profile manager can be NULL when testing.
    return NULL;
  }

  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  if (profiles.empty())
    return NULL;

  return PluginPrefs::GetForProfile(profiles.front()).get();
}

// Fills |plugin| with the info contained in |plugin_info| and |plugin_prefs|.
void SetPluginInfo(const content::WebPluginInfo& plugin_info,
                   const PluginPrefs* plugin_prefs,
                   metrics::SystemProfileProto::Plugin* plugin) {
  plugin->set_name(base::UTF16ToUTF8(plugin_info.name));
  plugin->set_filename(plugin_info.path.BaseName().AsUTF8Unsafe());
  plugin->set_version(base::UTF16ToUTF8(plugin_info.version));
  plugin->set_is_pepper(plugin_info.is_pepper_plugin());
  if (plugin_prefs)
    plugin->set_is_disabled(!plugin_prefs->IsPluginEnabled(plugin_info));
}

}  // namespace

// This is used to quickly log stats from child process related notifications in
// PluginMetricsProvider::child_stats_buffer_.  The buffer's contents are
// transferred out when Local State is periodically saved.  The information is
// then reported to the UMA server on next launch.
struct PluginMetricsProvider::ChildProcessStats {
 public:
  explicit ChildProcessStats(int process_type)
      : process_launches(0),
        process_crashes(0),
        instances(0),
        loading_errors(0),
        process_type(process_type) {}

  // This constructor is only used by the map to return some default value for
  // an index for which no value has been assigned.
  ChildProcessStats()
      : process_launches(0),
        process_crashes(0),
        instances(0),
        loading_errors(0),
        process_type(content::PROCESS_TYPE_UNKNOWN) {}

  // The number of times that the given child process has been launched
  int process_launches;

  // The number of times that the given child process has crashed
  int process_crashes;

  // The number of instances of this child process that have been created.
  // An instance is a DOM object rendered by this child process during a page
  // load.
  int instances;

  // The number of times there was an error loading an instance of this child
  // process.
  int loading_errors;

  int process_type;
};

PluginMetricsProvider::PluginMetricsProvider(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);

  BrowserChildProcessObserver::Add(this);
}

PluginMetricsProvider::~PluginMetricsProvider() {
  BrowserChildProcessObserver::Remove(this);
}

void PluginMetricsProvider::AsyncInit(const base::Closure& done_callback) {
  content::PluginService::GetInstance()->GetPlugins(
      base::Bind(&PluginMetricsProvider::OnGotPlugins,
                 weak_ptr_factory_.GetWeakPtr(),
                 done_callback));
}

void PluginMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  PluginPrefs* plugin_prefs = GetPluginPrefs();
  for (size_t i = 0; i < plugins_.size(); ++i) {
    SetPluginInfo(plugins_[i], plugin_prefs,
                  system_profile_proto->add_plugin());
  }
}

void PluginMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  RecordCurrentStateIfPending();
  const base::ListValue* plugin_stats_list = local_state_->GetList(
      prefs::kStabilityPluginStats);
  if (!plugin_stats_list)
    return;

  metrics::SystemProfileProto::Stability* stability =
      system_profile_proto->mutable_stability();
  for (const auto& value : *plugin_stats_list) {
    const base::DictionaryValue* plugin_dict;
    if (!value.GetAsDictionary(&plugin_dict)) {
      NOTREACHED();
      continue;
    }

    // Note that this search is potentially a quadratic operation, but given the
    // low number of plugins installed on a "reasonable" setup, this should be
    // fine.
    // TODO(isherman): Verify that this does not show up as a hotspot in
    // profiler runs.
    const metrics::SystemProfileProto::Plugin* system_profile_plugin = NULL;
    std::string plugin_name;
    plugin_dict->GetString(prefs::kStabilityPluginName, &plugin_name);
    for (int i = 0; i < system_profile_proto->plugin_size(); ++i) {
      if (system_profile_proto->plugin(i).name() == plugin_name) {
        system_profile_plugin = &system_profile_proto->plugin(i);
        break;
      }
    }

    if (!system_profile_plugin) {
      NOTREACHED();
      continue;
    }

    metrics::SystemProfileProto::Stability::PluginStability* plugin_stability =
        stability->add_plugin_stability();
    *plugin_stability->mutable_plugin() = *system_profile_plugin;

    int launches = 0;
    plugin_dict->GetInteger(prefs::kStabilityPluginLaunches, &launches);
    if (launches > 0)
      plugin_stability->set_launch_count(launches);

    int instances = 0;
    plugin_dict->GetInteger(prefs::kStabilityPluginInstances, &instances);
    if (instances > 0)
      plugin_stability->set_instance_count(instances);

    int crashes = 0;
    plugin_dict->GetInteger(prefs::kStabilityPluginCrashes, &crashes);
    if (crashes > 0)
      plugin_stability->set_crash_count(crashes);

    int loading_errors = 0;
    plugin_dict->GetInteger(prefs::kStabilityPluginLoadingErrors,
                            &loading_errors);
    if (loading_errors > 0)
      plugin_stability->set_loading_error_count(loading_errors);
  }

  local_state_->ClearPref(prefs::kStabilityPluginStats);
}

void PluginMetricsProvider::ClearSavedStabilityMetrics() {
  local_state_->ClearPref(prefs::kStabilityPluginStats);
}

// Saves plugin-related updates from the in-object buffer to Local State
// for retrieval next time we send a Profile log (generally next launch).
void PluginMetricsProvider::RecordCurrentState() {
  ListPrefUpdate update(local_state_, prefs::kStabilityPluginStats);
  base::ListValue* plugins = update.Get();
  DCHECK(plugins);

  for (auto& value : *plugins) {
    base::DictionaryValue* plugin_dict;
    if (!value.GetAsDictionary(&plugin_dict)) {
      NOTREACHED();
      continue;
    }

    base::string16 plugin_name;
    plugin_dict->GetString(prefs::kStabilityPluginName, &plugin_name);
    if (plugin_name.empty()) {
      NOTREACHED();
      continue;
    }

    if (child_process_stats_buffer_.find(plugin_name) ==
        child_process_stats_buffer_.end()) {
      continue;
    }

    ChildProcessStats stats = child_process_stats_buffer_[plugin_name];
    if (stats.process_launches) {
      int launches = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginLaunches, &launches);
      launches += stats.process_launches;
      plugin_dict->SetInteger(prefs::kStabilityPluginLaunches, launches);
    }
    if (stats.process_crashes) {
      int crashes = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginCrashes, &crashes);
      crashes += stats.process_crashes;
      plugin_dict->SetInteger(prefs::kStabilityPluginCrashes, crashes);
    }
    if (stats.instances) {
      int instances = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginInstances, &instances);
      instances += stats.instances;
      plugin_dict->SetInteger(prefs::kStabilityPluginInstances, instances);
    }
    if (stats.loading_errors) {
      int loading_errors = 0;
      plugin_dict->GetInteger(prefs::kStabilityPluginLoadingErrors,
                              &loading_errors);
      loading_errors += stats.loading_errors;
      plugin_dict->SetInteger(prefs::kStabilityPluginLoadingErrors,
                              loading_errors);
    }

    child_process_stats_buffer_.erase(plugin_name);
  }

  // Now go through and add dictionaries for plugins that didn't already have
  // reports in Local State.
  for (auto cache_iter = child_process_stats_buffer_.begin();
       cache_iter != child_process_stats_buffer_.end(); ++cache_iter) {
    ChildProcessStats stats = cache_iter->second;

    // Insert only plugins information into the plugins list.
    if (!IsPluginProcess(stats.process_type))
      continue;

    std::unique_ptr<base::DictionaryValue> plugin_dict(
        new base::DictionaryValue);

    plugin_dict->SetString(prefs::kStabilityPluginName, cache_iter->first);
    plugin_dict->SetInteger(prefs::kStabilityPluginLaunches,
                            stats.process_launches);
    plugin_dict->SetInteger(prefs::kStabilityPluginCrashes,
                            stats.process_crashes);
    plugin_dict->SetInteger(prefs::kStabilityPluginInstances,
                            stats.instances);
    plugin_dict->SetInteger(prefs::kStabilityPluginLoadingErrors,
                            stats.loading_errors);
    plugins->Append(std::move(plugin_dict));
  }
  child_process_stats_buffer_.clear();
}

void PluginMetricsProvider::LogPluginLoadingError(
    const base::FilePath& plugin_path) {
  content::WebPluginInfo plugin;
  bool success =
      content::PluginService::GetInstance()->GetPluginInfoByPath(plugin_path,
                                                                 &plugin);
  DCHECK(success);
  ChildProcessStats& stats = child_process_stats_buffer_[plugin.name];
  // Initialize the type if this entry is new.
  if (stats.process_type == content::PROCESS_TYPE_UNKNOWN) {
    // The plugin process might not actually be of type PPAPI_PLUGIN, but we
    // only care that it is *a* plugin process.
    stats.process_type = content::PROCESS_TYPE_PPAPI_PLUGIN;
  } else {
    DCHECK(IsPluginProcess(stats.process_type));
  }
  stats.loading_errors++;
  RecordCurrentStateWithDelay();
}

void PluginMetricsProvider::SetPluginsForTesting(
    const std::vector<content::WebPluginInfo>& plugins) {
  plugins_ = plugins;
}

// static
bool PluginMetricsProvider::IsPluginProcess(int process_type) {
  return (process_type == content::PROCESS_TYPE_PPAPI_PLUGIN ||
          process_type == content::PROCESS_TYPE_PPAPI_BROKER);
}

// static
void PluginMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kStabilityPluginStats);
}

void PluginMetricsProvider::OnGotPlugins(
    const base::Closure& done_callback,
    const std::vector<content::WebPluginInfo>& plugins) {
  plugins_ = plugins;
  done_callback.Run();
}

PluginMetricsProvider::ChildProcessStats&
PluginMetricsProvider::GetChildProcessStats(
    const content::ChildProcessData& data) {
  const base::string16& child_name = data.name;
  if (!base::Contains(child_process_stats_buffer_, child_name)) {
    child_process_stats_buffer_[child_name] =
        ChildProcessStats(data.process_type);
  }
  return child_process_stats_buffer_[child_name];
}

void PluginMetricsProvider::BrowserChildProcessHostConnected(
    const content::ChildProcessData& data) {
  GetChildProcessStats(data).process_launches++;
  RecordCurrentStateWithDelay();
}

void PluginMetricsProvider::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  GetChildProcessStats(data).process_crashes++;
  RecordCurrentStateWithDelay();
}

void PluginMetricsProvider::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  // Treat a kill as a crash, since Flash returns STATUS_DEBUGGER_INACTIVE for
  // actual crashes, which is treated as a kill rather than a crash by
  // base::GetTerminationStatus
  GetChildProcessStats(data).process_crashes++;
  RecordCurrentStateWithDelay();
}

bool PluginMetricsProvider::RecordCurrentStateWithDelay() {
  if (weak_ptr_factory_.HasWeakPtrs())
    return false;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PluginMetricsProvider::RecordCurrentState,
                     weak_ptr_factory_.GetWeakPtr()),
      kRecordStateDelay);
  return true;
}

bool PluginMetricsProvider::RecordCurrentStateIfPending() {
  if (!weak_ptr_factory_.HasWeakPtrs())
    return false;

  weak_ptr_factory_.InvalidateWeakPtrs();
  RecordCurrentState();
  return true;
}

// static
base::TimeDelta PluginMetricsProvider::GetRecordStateDelay() {
  return kRecordStateDelay;
}
