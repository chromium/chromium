// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/extension_settings_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "components/value_store/value_store.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

using extensions::ExtensionRegistry;

namespace extension_settings_helper {

namespace {

std::string ToJson(base::ValueView value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

void GetAllSettingsOnBackendSequence(base::DictValue* out,
                                     base::WaitableEvent* signal,
                                     value_store::ValueStore* storage) {
  EXPECT_TRUE(extensions::GetBackendTaskRunner()->RunsTasksInCurrentSequence());
  std::swap(*out, storage->Get().settings());
  signal->Signal();
}

base::DictValue GetAllSettings(Profile* profile, const std::string& id) {
  base::WaitableEvent signal(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::DictValue settings;
  extensions::StorageFrontend::Get(profile)->RunWithStorage(
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(id),
      extensions::settings_namespace::SYNC,
      base::BindOnce(&GetAllSettingsOnBackendSequence, &settings, &signal));
  signal.Wait();
  return settings;
}

bool AreSettingsSame(Profile* expected_profile,
                     Profile* actual_profile,
                     std::ostream* os) {
  const extensions::ExtensionSet& extensions =
      ExtensionRegistry::Get(expected_profile)->enabled_extensions();
  if (extensions.size() !=
      ExtensionRegistry::Get(actual_profile)->enabled_extensions().size()) {
    return false;
  }

  bool same = true;
  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const std::string& id = (*it)->id();
    base::DictValue expected(GetAllSettings(expected_profile, id));
    base::DictValue actual(GetAllSettings(actual_profile, id));
    if (expected != actual) {
      *os << "Expected " << ToJson(expected) << " got " << ToJson(actual);
      same = false;
    }
  }
  return same;
}

void SetSettingsOnBackendSequence(const base::DictValue* settings,
                                  base::WaitableEvent* signal,
                                  value_store::ValueStore* storage) {
  EXPECT_TRUE(extensions::GetBackendTaskRunner()->RunsTasksInCurrentSequence());
  storage->Set(value_store::ValueStore::DEFAULTS, *settings);
  signal->Signal();
}

bool AllExtensionSettingsSame(
    const std::vector<raw_ptr<Profile, VectorExperimental>>& profiles,
    std::ostream* os) {
  CHECK_GT(profiles.size(), 1u) << "At least two profiles are required.";

  bool all_profiles_same = true;
  for (size_t i = 1; i < profiles.size(); ++i) {
    // &= so that all profiles are tested; analogous to EXPECT over ASSERT.
    all_profiles_same &= AreSettingsSame(profiles[0], profiles[i], os);
  }
  return all_profiles_same;
}

}  // namespace

void SetExtensionSettings(Profile* profile,
                          const std::string& id,
                          const base::DictValue& settings) {
  base::WaitableEvent signal(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  extensions::StorageFrontend::Get(profile)->RunWithStorage(
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(id),
      extensions::settings_namespace::SYNC,
      base::BindOnce(&SetSettingsOnBackendSequence, &settings, &signal));
  signal.Wait();
}

void SetExtensionSettings(
    const std::vector<raw_ptr<Profile, VectorExperimental>>& profiles,
    const std::string& id,
    const base::DictValue& settings) {
  for (Profile* profile : profiles) {
    SetExtensionSettings(profile, id, settings);
  }
}

AllExtensionSettingsSameChecker::AllExtensionSettingsSameChecker(
    const std::vector<raw_ptr<syncer::SyncServiceImpl, VectorExperimental>>&
        services,
    const std::vector<raw_ptr<Profile, VectorExperimental>>& profiles)
    : MultiClientStatusChangeChecker(services), profiles_(profiles) {}

AllExtensionSettingsSameChecker::~AllExtensionSettingsSameChecker() = default;

bool AllExtensionSettingsSameChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  return AllExtensionSettingsSame(profiles_, os);
}

}  // namespace extension_settings_helper
