// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_prefs.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
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
using extensions::mojom::ManifestLocation;

namespace extensions {

// A Clock which returns an incrementally later time each time Now() is called.
class TestExtensionPrefs::IncrementalClock : public base::Clock {
 public:
  IncrementalClock() : current_time_(base::Time::Now()) {}

  IncrementalClock(const IncrementalClock&) = delete;
  IncrementalClock& operator=(const IncrementalClock&) = delete;

  ~IncrementalClock() override {}

  base::Time Now() const override {
    current_time_ += base::Seconds(10);
    return current_time_;
  }

 private:
  mutable base::Time current_time_;
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

  extension_pref_value_map_ = std::make_unique<ExtensionPrefValueMap>();
  sync_preferences::PrefServiceMockFactory factory;
  factory.SetUserPrefsFile(preferences_file_, task_runner_.get());
  factory.set_extension_prefs(
      new ExtensionPrefStore(extension_pref_value_map_.get(), false));
  // Don't replace `pref_service_` until after re-assigning the `ExtensionPrefs`
  // testing instance to avoid a dangling pointer.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> new_pref_service =
      factory.CreateSyncable(pref_registry_.get());
  std::unique_ptr<ExtensionPrefs> prefs(ExtensionPrefs::Create(
      &profile_, new_pref_service.get(), temp_dir_.GetPath(),
      extension_pref_value_map_.get(), extensions_disabled_,
      std::vector<EarlyExtensionPrefsObserver*>(),
      // Guarantee that no two extensions get the same installation time
      // stamp and we can reliably assert the installation order in the tests.
      clock_.get()));
  ExtensionPrefsFactory::GetInstance()->SetInstanceForTesting(&profile_,
                                                              std::move(prefs));
  pref_service_ = std::move(new_pref_service);
  // Hack: After recreating ExtensionPrefs, the AppSorting also needs to be
  // recreated. (ExtensionPrefs is never recreated in non-test code.)
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(&profile_))
      ->RecreateAppSorting();
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtension(
    const std::string& name) {
  return AddExtensionWithLocation(name, ManifestLocation::kInternal);
}

scoped_refptr<Extension> TestExtensionPrefs::AddApp(const std::string& name) {
  base::Value::Dict dictionary;
  AddDefaultManifestKeys(name, dictionary);

  dictionary.SetByDottedPath(manifest_keys::kLaunchWebURL,
                             "http://example.com");

  return AddExtensionWithManifest(dictionary, ManifestLocation::kInternal);
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtensionWithLocation(
    const std::string& name,
    ManifestLocation location) {
  base::Value::Dict dictionary;
  AddDefaultManifestKeys(name, dictionary);
  return AddExtensionWithManifest(dictionary, location);
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtensionWithManifest(
    const base::Value::Dict& manifest,
    ManifestLocation location) {
  return AddExtensionWithManifestAndFlags(manifest, location,
                                          Extension::NO_FLAGS);
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtensionWithManifestAndFlags(
    const base::Value::Dict& manifest,
    ManifestLocation location,
    int extra_flags) {
  const std::string* name = manifest.FindString(manifest_keys::kName);
  EXPECT_TRUE(name);
  base::FilePath path = extensions_dir_.AppendASCII(*name);
  std::string errors;
  scoped_refptr<Extension> extension =
      Extension::Create(path, location, manifest, extra_flags, &errors);
  EXPECT_TRUE(extension.get()) << errors;
  if (!extension.get()) {
    return nullptr;
  }

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
      new ExtensionPrefStore(extension_pref_value_map_.get(), true));
}

void TestExtensionPrefs::set_extensions_disabled(bool extensions_disabled) {
  extensions_disabled_ = extensions_disabled;
}

ChromeAppSorting* TestExtensionPrefs::app_sorting() {
  return static_cast<ChromeAppSorting*>(
      ExtensionSystem::Get(&profile_)->app_sorting());
}

void TestExtensionPrefs::AddDefaultManifestKeys(const std::string& name,
                                                base::Value::Dict& dict) {
  dict.Set(manifest_keys::kName, name);
  dict.Set(manifest_keys::kVersion, "0.1");
  dict.Set(manifest_keys::kManifestVersion, 2);
}

}  // namespace extensions
