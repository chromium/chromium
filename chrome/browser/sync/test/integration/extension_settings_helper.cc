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
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "components/value_store/value_store.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

using extensions::ExtensionRegistry;
using sync_datatype_helper::test;

namespace extension_settings_helper {

namespace {

std::string ToJson(base::ValueView value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

void GetAllSettingsOnBackendSequence(base::Value::Dict* out,
                                     base::WaitableEvent* signal,
                                     value_store::ValueStore* storage) {
  EXPECT_TRUE(extensions::GetBackendTaskRunner()->RunsTasksInCurrentSequence());
  std::swap(*out, storage->Get().settings());
  signal->Signal();
}

base::Value::Dict GetAllSettings(Profile* profile, const std::string& id) {
  base::WaitableEvent signal(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::Value::Dict settings;
  extensions::StorageFrontend::Get(profile)->RunWithStorage(
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(id),
      extensions::settings_namespace::SYNC,
      base::BindOnce(&GetAllSettingsOnBackendSequence, &settings, &signal));
  signal.Wait();
  return settings;
}

bool AreSettingsSame(Profile* expected_profile, Profile* actual_profile) {
  const extensions::ExtensionSet& extensions =
      ExtensionRegistry::Get(expected_profile)->enabled_extensions();
  if (extensions.size() !=
      ExtensionRegistry::Get(actual_profile)->enabled_extensions().size()) {
    ADD_FAILURE();
    return false;
  }

  bool same = true;
  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    const std::string& id = (*it)->id();
    base::Value::Dict expected(GetAllSettings(expected_profile, id));
    base::Value::Dict actual(GetAllSettings(actual_profile, id));
    if (expected != actual) {
      ADD_FAILURE() << "Expected " << ToJson(expected) << " got "
                    << ToJson(actual);
      same = false;
    }
  }
  return same;
}

void SetSettingsOnBackendSequence(const base::Value::Dict* settings,
                                  base::WaitableEvent* signal,
                                  value_store::ValueStore* storage) {
  EXPECT_TRUE(extensions::GetBackendTaskRunner()->RunsTasksInCurrentSequence());
  storage->Set(value_store::ValueStore::DEFAULTS, *settings);
  signal->Signal();
}

}  // namespace

void SetExtensionSettings(Profile* profile,
                          const std::string& id,
                          const base::Value::Dict& settings) {
  base::WaitableEvent signal(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  extensions::StorageFrontend::Get(profile)->RunWithStorage(
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(id),
      extensions::settings_namespace::SYNC,
      base::BindOnce(&SetSettingsOnBackendSequence, &settings, &signal));
  signal.Wait();
}

void SetExtensionSettingsForAllProfiles(const std::string& id,
                                        const base::Value::Dict& settings) {
  for (int i = 0; i < test()->num_clients(); ++i) {
    SetExtensionSettings(test()->GetProfile(i), id, settings);
  }
  SetExtensionSettings(test()->verifier(), id, settings);
}

bool AllExtensionSettingsSameAsVerifier() {
  bool all_profiles_same = true;
  for (int i = 0; i < test()->num_clients(); ++i) {
    // &= so that all profiles are tested; analogous to EXPECT over ASSERT.
    all_profiles_same &=
        AreSettingsSame(test()->verifier(), test()->GetProfile(i));
  }
  return all_profiles_same;
}

}  // namespace extension_settings_helper
