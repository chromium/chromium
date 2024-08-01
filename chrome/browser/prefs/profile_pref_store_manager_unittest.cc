// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/prefs/profile_pref_store_manager.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/preferences/public/cpp/tracked/configuration.h"
#include "services/preferences/public/cpp/tracked/mock_validation_delegate.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

using EnforcementLevel =
    prefs::mojom::TrackedPreferenceMetadata::EnforcementLevel;
using PrefTrackingStrategy =
    prefs::mojom::TrackedPreferenceMetadata::PrefTrackingStrategy;
using ValueType = prefs::mojom::TrackedPreferenceMetadata::ValueType;

// Observes changes to the PrefStore and verifies that only registered prefs are
// written.
class RegistryVerifier : public PrefStore::Observer {
 public:
  explicit RegistryVerifier(PrefRegistry* pref_registry)
      : pref_registry_(pref_registry) {}

  // PrefStore::Observer implementation
  void OnPrefValueChanged(std::string_view key) override {
    EXPECT_TRUE(base::Contains(*pref_registry_, key,
                               &PrefValueMap::Map::value_type::first))
        << "Unregistered key " << key << " was changed.";
  }

  void OnInitializationCompleted(bool succeeded) override {}

 private:
  scoped_refptr<PrefRegistry> pref_registry_;
};

class PrefStoreReadObserver : public PrefStore::Observer {
 public:
  explicit PrefStoreReadObserver(scoped_refptr<PersistentPrefStore> pref_store)
      : pref_store_(std::move(pref_store)) {
    pref_store_->AddObserver(this);
  }

  PrefStoreReadObserver(const PrefStoreReadObserver&) = delete;
  PrefStoreReadObserver& operator=(const PrefStoreReadObserver&) = delete;

  ~PrefStoreReadObserver() override { pref_store_->RemoveObserver(this); }

  PersistentPrefStore::PrefReadError Read() {
    base::RunLoop run_loop;
    stop_waiting_ = run_loop.QuitClosure();
    pref_store_->ReadPrefsAsync(nullptr);
    run_loop.Run();
    return pref_store_->GetReadError();
  }

  // PrefStore::Observer implementation
  void OnInitializationCompleted(bool succeeded) override {
    if (stop_waiting_) {
      std::move(stop_waiting_).Run();
    }
  }

 private:
  scoped_refptr<PersistentPrefStore> pref_store_;
  base::OnceClosure stop_waiting_;
};

const char kUnprotectedPref[] = "unprotected_pref";
const char kTrackedAtomic[] = "tracked_atomic";
const char kProtectedAtomic[] = "protected_atomic";

const char kFoobar[] = "FOOBAR";
const char kBarfoo[] = "BARFOO";
const char kHelloWorld[] = "HELLOWORLD";
const char kGoodbyeWorld[] = "GOODBYEWORLD";

const prefs::TrackedPreferenceMetadata kConfiguration[] = {
    {0u, kTrackedAtomic, EnforcementLevel::NO_ENFORCEMENT,
     PrefTrackingStrategy::ATOMIC},
    {1u, kProtectedAtomic, EnforcementLevel::ENFORCE_ON_LOAD,
     PrefTrackingStrategy::ATOMIC}};

const size_t kExtraReportingId = 2u;
const size_t kReportingIdCount = 3u;

}  // namespace

class ProfilePrefStoreManagerTest : public testing::Test,
                                    public prefs::mojom::ResetOnLoadObserver {
 public:
  ProfilePrefStoreManagerTest()
      : configuration_(prefs::ConstructTrackedConfiguration(kConfiguration)),
        profile_pref_registry_(new user_prefs::PrefRegistrySyncable),
        registry_verifier_(profile_pref_registry_.get()),
        seed_("seed"),
        reset_recorded_(false) {}

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    registry_override_.OverrideRegistry(HKEY_CURRENT_USER);
#endif  // BUILDFLAG(IS_WIN)
    mock_validation_delegate_record_ = new MockValidationDelegateRecord;
    mock_validation_delegate_ = std::make_unique<MockValidationDelegate>(
        mock_validation_delegate_record_);

    ProfilePrefStoreManager::RegisterProfilePrefs(profile_pref_registry_.get());
    for (const prefs::TrackedPreferenceMetadata* it = kConfiguration;
         it != kConfiguration + std::size(kConfiguration); ++it) {
      if (it->strategy == PrefTrackingStrategy::ATOMIC) {
        profile_pref_registry_->RegisterStringPref(it->name, std::string());
      } else {
        profile_pref_registry_->RegisterDictionaryPref(it->name);
      }
    }
    profile_pref_registry_->RegisterStringPref(kUnprotectedPref, std::string());

    // As in chrome_pref_service_factory.cc, kPreferencesResetTime needs to be
    // declared as protected in order to be read from the proper store by the
    // SegregatedPrefStore. Only declare it after configured prefs have been
    // registered above for this test as kPreferenceResetTime is already
    // registered in ProfilePrefStoreManager::RegisterProfilePrefs.
    prefs::TrackedPreferenceMetadata pref_reset_time_config = {
        static_cast<size_t>((*configuration_.rbegin())->reporting_id + 1),
        user_prefs::kPreferenceResetTime, EnforcementLevel::ENFORCE_ON_LOAD,
        PrefTrackingStrategy::ATOMIC};
    configuration_.push_back(
        prefs::ConstructTrackedMetadata(pref_reset_time_config));

    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    ReloadConfiguration();
  }

  void ReloadConfiguration() {
    manager_ = std::make_unique<ProfilePrefStoreManager>(profile_dir_.GetPath(),
                                                         seed_, "device_id");
  }

  void TearDown() override {
    DestroyPrefStore();
  }

 protected:
  // Verifies whether a reset was reported via the OnResetOnLoad() hook. Also
  // verifies that GetResetTime() was set (or not) accordingly.
  void VerifyResetRecorded(bool reset_expected) {
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(reset_expected, reset_recorded_);

    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(pref_store_);

    std::unique_ptr<PrefService> pref_service(
        pref_service_factory.Create(profile_pref_registry_.get()));

    EXPECT_EQ(
        reset_expected,
        !ProfilePrefStoreManager::GetResetTime(pref_service.get()).is_null());
  }

  void ClearResetRecorded() {
    reset_recorded_ = false;

    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(pref_store_);

    std::unique_ptr<PrefService> pref_service(
        pref_service_factory.Create(profile_pref_registry_.get()));

    ProfilePrefStoreManager::ClearResetTime(pref_service.get());
  }

  void InitializePrefs() {
    // According to the implementation of ProfilePrefStoreManager, this is
    // actually a SegregatedPrefStore backed by two underlying pref stores.
    mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver> observer;
    reset_on_load_observer_receivers_.Add(
        this, observer.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<prefs::mojom::TrackedPreferenceValidationDelegate>
        validation_delegate;
    mock_validation_delegate_receivers_.Add(
        mock_validation_delegate_.get(),
        validation_delegate.InitWithNewPipeAndPassReceiver());
    scoped_refptr<PersistentPrefStore> pref_store =
        manager_->CreateProfilePrefStore(
            prefs::CloneTrackedConfiguration(configuration_), kReportingIdCount,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            std::move(observer), std::move(validation_delegate));
    InitializePrefStore(pref_store.get());
    pref_store = nullptr;
  }

  void DestroyPrefStore() {
    if (pref_store_.get()) {
      ClearResetRecorded();
      // Force everything to be written to disk, triggering the PrefHashFilter
      // while our RegistryVerifier is watching.
      base::RunLoop run_loop;
      pref_store_->CommitPendingWrite(run_loop.QuitClosure());
      run_loop.Run();

      pref_store_->RemoveObserver(&registry_verifier_);
      pref_store_.reset();
      // Nothing should have to happen on the background threads, but just in
      // case...
      base::RunLoop().RunUntilIdle();
    }
  }

  void InitializePrefStore(PersistentPrefStore* pref_store) {
    pref_store->AddObserver(&registry_verifier_);
    PrefStoreReadObserver read_observer(pref_store);
    PersistentPrefStore::PrefReadError error = read_observer.Read();
    EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE, error);
    pref_store->SetValue(kTrackedAtomic, base::Value(kFoobar),
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store->SetValue(kProtectedAtomic, base::Value(kHelloWorld),
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store->SetValue(kUnprotectedPref, base::Value(kFoobar),
                         WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
    pref_store->RemoveObserver(&registry_verifier_);
    base::RunLoop run_loop;
    pref_store->CommitPendingWrite(run_loop.QuitClosure());
    run_loop.Run();
  }

  void LoadExistingPrefs() {
    DestroyPrefStore();
    mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver> observer;
    reset_on_load_observer_receivers_.Add(
        this, observer.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<prefs::mojom::TrackedPreferenceValidationDelegate>
        validation_delegate;
    mock_validation_delegate_receivers_.Add(
        mock_validation_delegate_.get(),
        validation_delegate.InitWithNewPipeAndPassReceiver());
    pref_store_ = manager_->CreateProfilePrefStore(
        prefs::CloneTrackedConfiguration(configuration_), kReportingIdCount,
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(observer),
        std::move(validation_delegate));
    pref_store_->AddObserver(&registry_verifier_);
    PrefStoreReadObserver read_observer(pref_store_);
    read_observer.Read();
  }

  void ReplaceStringInPrefs(const std::string& find,
                            const std::string& replace) {
    base::FileEnumerator file_enum(profile_dir_.GetPath(), true,
                                   base::FileEnumerator::FILES);

    for (base::FilePath path = file_enum.Next(); !path.empty();
         path = file_enum.Next()) {
      // Tamper with the file's contents
      std::string contents;
      EXPECT_TRUE(base::ReadFileToString(path, &contents));
      base::ReplaceSubstringsAfterOffset(&contents, 0u, find, replace);
      EXPECT_TRUE(base::WriteFile(path, contents));
    }
  }

  void ExpectStringValueEquals(const std::string& name,
                               const std::string& expected) {
    const base::Value* value = nullptr;
    if (!pref_store_->GetValue(name, &value)) {
      ADD_FAILURE() << name << " is not a defined value.";
    } else if (!value->is_string()) {
      ADD_FAILURE() << name << " could not be coerced to a string.";
    } else {
      EXPECT_EQ(expected, value->GetString());
    }
  }

  void ExpectValidationObserved(const std::string& pref_path) {
    // No validations are expected for platforms that do not support tracking.
    if (!ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking)
      return;
    if (!mock_validation_delegate_record_->GetEventForPath(pref_path))
      ADD_FAILURE() << "No validation observed for preference: " << pref_path;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
#if BUILDFLAG(IS_WIN)
  // This is used to ensure that the registry starts in a well known state, and
  // any registry changes by this test don't affect other parts of the registry
  // on the machine running the test, and are cleaned up.
  registry_util::RegistryOverrideManager registry_override_;
#endif  // BUILDFLAG(IS_WIN)
  std::vector<prefs::mojom::TrackedPreferenceMetadataPtr> configuration_;
  base::ScopedTempDir profile_dir_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> profile_pref_registry_;
  RegistryVerifier registry_verifier_;
  scoped_refptr<MockValidationDelegateRecord> mock_validation_delegate_record_;
  std::unique_ptr<MockValidationDelegate> mock_validation_delegate_;
  mojo::ReceiverSet<prefs::mojom::TrackedPreferenceValidationDelegate>
      mock_validation_delegate_receivers_;
  std::unique_ptr<ProfilePrefStoreManager> manager_;
  scoped_refptr<PersistentPrefStore> pref_store_;

  std::string seed_;

 private:
  void OnResetOnLoad() override {
    // As-is |reset_recorded_| is only designed to remember a single reset, make
    // sure none was previously recorded (or that ClearResetRecorded() was
    // called).
    EXPECT_FALSE(reset_recorded_);
    reset_recorded_ = true;
  }

  base::test::ScopedFeatureList feature_list_;
  bool reset_recorded_;
  mojo::ReceiverSet<prefs::mojom::ResetOnLoadObserver>
      reset_on_load_observer_receivers_;
};

TEST_F(ProfilePrefStoreManagerTest, StoreValues) {
  InitializePrefs();

  LoadExistingPrefs();

  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  VerifyResetRecorded(false);
  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);
}

TEST_F(ProfilePrefStoreManagerTest, ProtectValues) {
  InitializePrefs();

  ReplaceStringInPrefs(kFoobar, kBarfoo);
  ReplaceStringInPrefs(kHelloWorld, kGoodbyeWorld);

  LoadExistingPrefs();

  // kTrackedAtomic is unprotected and thus will be loaded as it appears on
  // disk.
  ExpectStringValueEquals(kTrackedAtomic, kBarfoo);

  // If preference tracking is supported, the tampered value of kProtectedAtomic
  // will be discarded at load time, leaving this preference undefined.
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kProtectedAtomic, nullptr));
  VerifyResetRecorded(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking);

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);
}

TEST_F(ProfilePrefStoreManagerTest, InitializePrefsFromMasterPrefs) {
  base::Value::Dict master_prefs;
  master_prefs.Set(kTrackedAtomic, kFoobar);
  master_prefs.Set(kProtectedAtomic, kHelloWorld);
  EXPECT_TRUE(manager_->InitializePrefsFromMasterPrefs(
      prefs::CloneTrackedConfiguration(configuration_), kReportingIdCount,
      std::move(master_prefs)));

  LoadExistingPrefs();

  // Verify that InitializePrefsFromMasterPrefs correctly applied the MACs
  // necessary to authenticate these values.
  ExpectStringValueEquals(kTrackedAtomic, kFoobar);
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  VerifyResetRecorded(false);
}

TEST_F(ProfilePrefStoreManagerTest, UnprotectedToProtected) {
  InitializePrefs();

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);

  LoadExistingPrefs();
  ExpectStringValueEquals(kUnprotectedPref, kFoobar);

  // Ensure everything is written out to disk.
  DestroyPrefStore();

  ReplaceStringInPrefs(kFoobar, kBarfoo);

  // It's unprotected, so we can load the modified value.
  LoadExistingPrefs();
  ExpectStringValueEquals(kUnprotectedPref, kBarfoo);

  // Now update the configuration to protect it.
  prefs::TrackedPreferenceMetadata new_protected = {
      kExtraReportingId, kUnprotectedPref, EnforcementLevel::ENFORCE_ON_LOAD,
      PrefTrackingStrategy::ATOMIC};
  configuration_.push_back(prefs::ConstructTrackedMetadata(new_protected));
  ReloadConfiguration();

  // And try loading with the new configuration.
  LoadExistingPrefs();

  // Since there was a valid super MAC we were able to extend the existing trust
  // to the newly protected preference.
  ExpectStringValueEquals(kUnprotectedPref, kBarfoo);
  VerifyResetRecorded(false);

  // Ensure everything is written out to disk.
  DestroyPrefStore();

  // It's protected now, so (if the platform supports it) any tampering should
  // lead to a reset.
  ReplaceStringInPrefs(kBarfoo, kFoobar);
  LoadExistingPrefs();
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kUnprotectedPref, nullptr));
  VerifyResetRecorded(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking);
}

TEST_F(ProfilePrefStoreManagerTest, NewPrefWhenFirstProtecting) {
  std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>
      original_configuration = prefs::CloneTrackedConfiguration(configuration_);
  for (const auto& metadata : configuration_) {
    metadata->enforcement_level = EnforcementLevel::NO_ENFORCEMENT;
  }
  ReloadConfiguration();

  InitializePrefs();

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);

  LoadExistingPrefs();
  ExpectStringValueEquals(kUnprotectedPref, kFoobar);

  // Ensure everything is written out to disk.
  DestroyPrefStore();

  // Now introduce protection, including the never-before tracked "new_pref".
  configuration_ = std::move(original_configuration);
  prefs::TrackedPreferenceMetadata new_protected = {
      kExtraReportingId, kUnprotectedPref, EnforcementLevel::ENFORCE_ON_LOAD,
      PrefTrackingStrategy::ATOMIC};
  configuration_.push_back(prefs::ConstructTrackedMetadata(new_protected));
  ReloadConfiguration();

  // And try loading with the new configuration.
  LoadExistingPrefs();

  // Since there was a valid super MAC we were able to extend the existing trust
  // to the newly tracked & protected preference.
  ExpectStringValueEquals(kUnprotectedPref, kFoobar);
  VerifyResetRecorded(false);
}

TEST_F(ProfilePrefStoreManagerTest, UnprotectedToProtectedWithoutTrust) {
  InitializePrefs();

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);

  // Now update the configuration to protect it.
  prefs::TrackedPreferenceMetadata new_protected = {
      kExtraReportingId, kUnprotectedPref, EnforcementLevel::ENFORCE_ON_LOAD,
      PrefTrackingStrategy::ATOMIC};
  configuration_.push_back(prefs::ConstructTrackedMetadata(new_protected));
  seed_ = "new-seed-to-break-trust";
  ReloadConfiguration();

  // And try loading with the new configuration.
  LoadExistingPrefs();

  // If preference tracking is supported, kUnprotectedPref will have been
  // discarded because new values are not accepted without a valid super MAC.
  EXPECT_NE(ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking,
            pref_store_->GetValue(kUnprotectedPref, nullptr));
  VerifyResetRecorded(
      ProfilePrefStoreManager::kPlatformSupportsPreferenceTracking);
}

// This test verifies that preference values are correctly maintained when a
// preference's protection state changes from protected to unprotected.
TEST_F(ProfilePrefStoreManagerTest, ProtectedToUnprotected) {
  InitializePrefs();

  ExpectValidationObserved(kTrackedAtomic);
  ExpectValidationObserved(kProtectedAtomic);

  DestroyPrefStore();

  // Unconfigure protection for kProtectedAtomic
  for (const auto& metadata : configuration_) {
    if (metadata->name == kProtectedAtomic) {
      metadata->enforcement_level = EnforcementLevel::NO_ENFORCEMENT;
      break;
    }
  }

  seed_ = "new-seed-to-break-trust";
  ReloadConfiguration();
  LoadExistingPrefs();

  // Verify that the value was not reset.
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);
  VerifyResetRecorded(false);

  // Accessing the value of the previously protected pref didn't trigger its
  // move to the unprotected preferences file, though the loading of the pref
  // store should still have caused the MAC store to be recalculated.
  LoadExistingPrefs();
  ExpectStringValueEquals(kProtectedAtomic, kHelloWorld);

  // Trigger the logic that migrates it back to the unprotected preferences
  // file.
  pref_store_->SetValue(kProtectedAtomic, base::Value(kGoodbyeWorld),
                        WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  LoadExistingPrefs();
  ExpectStringValueEquals(kProtectedAtomic, kGoodbyeWorld);
  VerifyResetRecorded(false);
}
