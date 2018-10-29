// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_prefs.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_app_sorting.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/common/chrome_constants.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace extensions {

// A Clock which returns an incrementally later time each time Now() is called.
class TestExtensionPrefs::IncrementalClock : public base::Clock {
 public:
  IncrementalClock() : current_time_(base::Time::Now()) {}

  ~IncrementalClock() override {}

  base::Time Now() const override {
    current_time_ += base::TimeDelta::FromSeconds(10);
    return current_time_;
  }

 private:
  mutable base::Time current_time_;

  DISALLOW_COPY_AND_ASSIGN(IncrementalClock);
};

TestExtensionPrefs::TestExtensionPrefs(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner),
      clock_(std::make_unique<IncrementalClock>()),
      extensions_disabled_(false) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  preferences_file_ = temp_dir_.GetPath().Append(chrome::kPreferencesFilename);
  extensions_dir_ = temp_dir_.GetPath().AppendASCII("Extensions");
  EXPECT_TRUE(base::CreateDirectory(extensions_dir_));

  ResetPrefRegistry();
  RecreateExtensionPrefs();
}

TestExtensionPrefs::~TestExtensionPrefs() {
}

ExtensionPrefs* TestExtensionPrefs::prefs() {
  return ExtensionPrefs::Get(&profile_);
}

TestingProfile* TestExtensionPrefs::profile() {
  return &profile_;
}

PrefService* TestExtensionPrefs::pref_service() {
  return pref_service_.get();
}

const scoped_refptr<user_prefs::PrefRegistrySyncable>&
TestExtensionPrefs::pref_registry() {
  return pref_registry_;
}

void TestExtensionPrefs::ResetPrefRegistry() {
  pref_registry_ = new user_prefs::PrefRegistrySyncable;
  RegisterUserProfilePrefs(pref_registry_.get());
}

void TestExtensionPrefs::RecreateExtensionPrefs() {
  // We persist and reload the PrefService's PrefStores because this process
  // deletes all empty dictionaries. The ExtensionPrefs implementation
  // needs to be able to handle this situation.
  if (pref_service_) {
    // Commit a pending write (which posts a task to task_runner_) and wait for
    // it to finish.
    pref_service_->CommitPendingWrite();
    base::RunLoop run_loop;
    ASSERT_TRUE(task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                               run_loop.QuitClosure()));
    run_loop.Run();
  }

  extension_pref_value_map_.reset(new ExtensionPrefValueMap);
  sync_preferences::PrefServiceMockFactory factory;
  factory.SetUserPrefsFile(preferences_file_, task_runner_.get());
  factory.set_extension_prefs(
      new ExtensionPrefStore(extension_pref_value_map_.get(), false));
  pref_service_ = factory.CreateSyncable(pref_registry_.get());
  std::unique_ptr<ExtensionPrefs> prefs(ExtensionPrefs::Create(
      &profile_, pref_service_.get(), temp_dir_.GetPath(),
      extension_pref_value_map_.get(), extensions_disabled_,
      std::vector<ExtensionPrefsObserver*>(),
      // Guarantee that no two extensions get the same installation time
      // stamp and we can reliably assert the installation order in the tests.
      clock_.get()));
  ExtensionPrefsFactory::GetInstance()->SetInstanceForTesting(&profile_,
                                                              std::move(prefs));
  // Hack: After recreating ExtensionPrefs, the AppSorting also needs to be
  // recreated. (ExtensionPrefs is never recreated in non-test code.)
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(&profile_))
      ->RecreateAppSorting();
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtension(
    const std::string& name) {
  base::DictionaryValue dictionary;
  dictionary.SetString(manifest_keys::kName, name);
  dictionary.SetString(manifest_keys::kVersion, "0.1");
  dictionary.SetInteger(manifest_keys::kManifestVersion, 2);
  return AddExtensionWithManifest(dictionary, Manifest::INTERNAL);
}

scoped_refptr<Extension> TestExtensionPrefs::AddApp(const std::string& name) {
  base::DictionaryValue dictionary;
  dictionary.SetString(manifest_keys::kName, name);
  dictionary.SetString(manifest_keys::kVersion, "0.1");
  dictionary.SetString(manifest_keys::kApp, "true");
  dictionary.SetString(manifest_keys::kLaunchWebURL, "http://example.com");
  return AddExtensionWithManifest(dictionary, Manifest::INTERNAL);

}

scoped_refptr<Extension> TestExtensionPrefs::AddExtensionWithManifest(
    const base::DictionaryValue& manifest, Manifest::Location location) {
  return AddExtensionWithManifestAndFlags(manifest, location,
                                          Extension::NO_FLAGS);
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtensionWithManifestAndFlags(
    const base::DictionaryValue& manifest,
    Manifest::Location location,
    int extra_flags) {
  std::string name;
  EXPECT_TRUE(manifest.GetString(manifest_keys::kName, &name));
  base::FilePath path =  extensions_dir_.AppendASCII(name);
  std::string errors;
  scoped_refptr<Extension> extension = Extension::Create(
      path, location, manifest, extra_flags, &errors);
  EXPECT_TRUE(extension.get()) << errors;
  if (!extension.get())
    return NULL;

  EXPECT_TRUE(crx_file::id_util::IdIsValid(extension->id()));
  prefs()->OnExtensionInstalled(extension.get(),
                                Extension::ENABLED,
                                syncer::StringOrdinal::CreateInitialOrdinal(),
                                std::string());
  return extension;
}

std::string TestExtensionPrefs::AddExtensionAndReturnId(
    const std::string& name) {
  scoped_refptr<Extension> extension(AddExtension(name));
  return extension->id();
}

void TestExtensionPrefs::AddExtension(const Extension* extension) {
  prefs()->OnExtensionInstalled(extension,
                                Extension::ENABLED,
                                syncer::StringOrdinal::CreateInitialOrdinal(),
                                std::string());
}

std::unique_ptr<PrefService> TestExtensionPrefs::CreateIncognitoPrefService()
    const {
  return CreateIncognitoPrefServiceSyncable(
      pref_service_.get(),
      new ExtensionPrefStore(extension_pref_value_map_.get(), true), nullptr);
}

void TestExtensionPrefs::set_extensions_disabled(bool extensions_disabled) {
  extensions_disabled_ = extensions_disabled;
}

ChromeAppSorting* TestExtensionPrefs::app_sorting() {
  return static_cast<ChromeAppSorting*>(
      ExtensionSystem::Get(&profile_)->app_sorting());
}

}  // namespace extensions
