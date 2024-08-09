// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/preferences/preferences.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/containers/to_vector.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/mock_component_extension_ime_manager_delegate.h"
#include "ui/base/ime/ash/mock_input_method_manager_impl.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kIdentityIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpIdentityIME";
constexpr char kToUpperIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpToUpperIME";
constexpr char kAPIArgumentIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpAPIArgumentIME";
constexpr char kUnknownIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpUnknownIME";

syncer::SyncData
CreatePrefSyncData(const std::string& name, const base::Value& value) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  json.Serialize(value);
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref =
      specifics.mutable_os_preference()->mutable_preference();
  pref->set_name(name);
  pref->set_value(serialized);
  return syncer::SyncData::CreateRemoteData(
      specifics, syncer::ClientTagHash::FromHashed("unused"));
}

}  // anonymous namespace

namespace input_method {
namespace {

class MyMockInputMethodManager : public MockInputMethodManagerImpl {
 public:
  class State : public MockInputMethodManagerImpl::State {
   public:
    explicit State(MyMockInputMethodManager* manager)
        : MockInputMethodManagerImpl::State(manager), manager_(manager) {
      input_method_extensions_ = std::make_unique<InputMethodDescriptors>();
    }

    void ChangeInputMethod(const std::string& input_method_id,
                           bool show_message) override {
      manager_->last_input_method_id_ = input_method_id;
      const std::string current_input_method_on_pref =
          manager_->current_->GetValue();
      if (current_input_method_on_pref == input_method_id)
        return;
      manager_->previous_->SetValue(current_input_method_on_pref);
      manager_->current_->SetValue(input_method_id);
    }

    void GetInputMethodExtensions(InputMethodDescriptors* result) override {
      *result = *input_method_extensions_;
    }

    void AddInputMethodExtension(const std::string& id,
                                 const InputMethodDescriptors& descriptors,
                                 TextInputMethod* instance) override {
      InputMethodDescriptor descriptor(
          id, std::string(), std::string(), std::string(),
          std::vector<std::string>(), false, GURL(), GURL(),
          /*handwriting_language=*/std::nullopt);
      input_method_extensions_->push_back(descriptor);
    }

   protected:
    ~State() override {}

   private:
    const raw_ptr<MyMockInputMethodManager> manager_;
    std::unique_ptr<InputMethodDescriptors> input_method_extensions_;
  };

  MyMockInputMethodManager(StringPrefMember* previous,
                           StringPrefMember* current)
      : previous_(previous),
        current_(current) {
    state_ = new State(this);
  }

  ~MyMockInputMethodManager() override {}

  std::string last_input_method_id_;

 private:
  raw_ptr<StringPrefMember> previous_;
  raw_ptr<StringPrefMember> current_;
};

}  // anonymous namespace
}  // namespace input_method

class PreferencesTest : public testing::Test {
 public:
  PreferencesTest() {}

  PreferencesTest(const PreferencesTest&) = delete;
  PreferencesTest& operator=(const PreferencesTest&) = delete;

  ~PreferencesTest() override {}

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    user_manager_ = new FakeChromeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager_.get()));

    const char test_user_email[] = "test_user@example.com";
    const AccountId test_account_id(AccountId::FromUserEmail(test_user_email));
    test_user_ = user_manager_->AddUser(test_account_id);
    user_manager_->LoginUser(test_account_id);
    user_manager_->SwitchActiveUser(test_account_id);

    test_profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile);
    pref_service_ = test_profile_->GetTestingPrefService();

    previous_input_method_.Init(
        prefs::kLanguagePreviousInputMethod, pref_service_);
    previous_input_method_.SetValue("KeyboardA");
    current_input_method_.Init(
        prefs::kLanguageCurrentInputMethod, pref_service_);
    current_input_method_.SetValue("KeyboardB");
    consumer_auto_update_toggle_.Init(::prefs::kConsumerAutoUpdateToggle,
                                      g_browser_process->local_state());
    consumer_auto_update_toggle_.SetValue(true);

    mock_manager_ = new input_method::MyMockInputMethodManager(
        &previous_input_method_, &current_input_method_);
    input_method::InitializeForTesting(mock_manager_);

    fake_update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();

    prefs_ = std::make_unique<Preferences>(mock_manager_);
  }

  void TearDown() override {
    // `prefs_` accesses UpdateEngineClient in its destructor.
    prefs_.reset();
    UpdateEngineClient::Shutdown();

    input_method::Shutdown();
    // UserSessionManager doesn't listen to profile destruction, so make sure
    // the default IME state isn't still cached in case test_profile_ is
    // given the same address in the next test.
    UserSessionManager::GetInstance()->RemoveProfileForTesting(test_profile_);
  }

  void InitPreferences() {
    prefs_->InitUserPrefsForTesting(
        pref_service_, test_user_, mock_manager_->GetActiveIMEState());
    prefs_->SetInputMethodListForTesting();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<Preferences> prefs_;
  StringPrefMember previous_input_method_;
  StringPrefMember current_input_method_;
  BooleanPrefMember consumer_auto_update_toggle_;
  base::test::ScopedFeatureList feature_list_;

  // Not owned.
  raw_ptr<FakeChromeUserManager> user_manager_;
  raw_ptr<const user_manager::User> test_user_;
  raw_ptr<TestingProfile> test_profile_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  raw_ptr<input_method::MyMockInputMethodManager, DanglingUntriaged>
      mock_manager_;
  raw_ptr<FakeUpdateEngineClient, DanglingUntriaged> fake_update_engine_client_;
};

TEST_F(PreferencesTest, TestUpdatePrefOnBrowserScreenDetails) {
  InitPreferences();

  // Confirm the current and previous input methods are unchanged.
  EXPECT_EQ("KeyboardA", previous_input_method_.GetValue());
  EXPECT_EQ("KeyboardB", current_input_method_.GetValue());
  EXPECT_EQ("KeyboardB", mock_manager_->last_input_method_id_);
}

TEST_F(PreferencesTest, TestConsumerAutoUpdateToggleOnSignals) {
  InitPreferences();

  auto CreateCAUFeatureStatus = [](bool enabled) {
    update_engine::StatusResult status;
    auto* feature = status.add_features();
    feature->set_name(update_engine::kFeatureConsumerAutoUpdate);
    feature->set_enabled(enabled);
    return status;
  };

  consumer_auto_update_toggle_.SetValue(true);

  fake_update_engine_client_->NotifyObserversThatStatusChanged(
      CreateCAUFeatureStatus(false));
  EXPECT_FALSE(consumer_auto_update_toggle_.GetValue());

  fake_update_engine_client_->NotifyObserversThatStatusChanged(
      CreateCAUFeatureStatus(true));
  EXPECT_TRUE(consumer_auto_update_toggle_.GetValue());
}

TEST_F(PreferencesTest, TestDeviceOwnerInitCAUFeatureEnabled) {
  feature_list_.InitAndEnableFeature(
      features::kConsumerAutoUpdateToggleAllowed);
  user_manager_->SetOwnerId(test_user_->GetAccountId());
  InitPreferences();
  EXPECT_EQ(0, fake_update_engine_client_->toggle_feature_count());
  EXPECT_EQ(1, fake_update_engine_client_->is_feature_enabled_count());
}

TEST_F(PreferencesTest, TestDeviceOwnerInitCAUFeatureDisabled) {
  feature_list_.InitAndDisableFeature(
      features::kConsumerAutoUpdateToggleAllowed);
  user_manager_->SetOwnerId(test_user_->GetAccountId());
  InitPreferences();
  EXPECT_EQ(1, fake_update_engine_client_->toggle_feature_count());
  EXPECT_EQ(0, fake_update_engine_client_->is_feature_enabled_count());
}

TEST_F(PreferencesTest, TestNonDeviceOwnerInitCAUCheck) {
  InitPreferences();
  EXPECT_EQ(0, fake_update_engine_client_->toggle_feature_count());
  EXPECT_EQ(1, fake_update_engine_client_->is_feature_enabled_count());
}

class InputMethodPreferencesTest : public PreferencesTest {
 public:
  InputMethodPreferencesTest() = default;
  InputMethodPreferencesTest(const InputMethodPreferencesTest&) = delete;
  InputMethodPreferencesTest& operator=(const InputMethodPreferencesTest&) =
      delete;

  ~InputMethodPreferencesTest() override = default;

  void SetUp() override {
    PreferencesTest::SetUp();

    // Initialize pref members.
    preferred_languages_.Init(language::prefs::kPreferredLanguages,
                              pref_service_);
    preferred_languages_syncable_.Init(
        language::prefs::kPreferredLanguagesSyncable, pref_service_);
    preload_engines_.Init(prefs::kLanguagePreloadEngines, pref_service_);
    preload_engines_syncable_.Init(prefs::kLanguagePreloadEnginesSyncable,
                                   pref_service_);
    enabled_imes_.Init(prefs::kLanguageEnabledImes, pref_service_);
    enabled_imes_syncable_.Init(prefs::kLanguageEnabledImesSyncable,
                                pref_service_);

    // Initialize component and 3rd-party input method extensions.
    InitComponentExtensionIMEManager();
    input_method::InputMethodDescriptors descriptors;
    mock_manager_->GetActiveIMEState()->AddInputMethodExtension(
        kIdentityIMEID, descriptors, nullptr);
    mock_manager_->GetActiveIMEState()->AddInputMethodExtension(
        kToUpperIMEID, descriptors, nullptr);
    mock_manager_->GetActiveIMEState()->AddInputMethodExtension(
        kAPIArgumentIMEID, descriptors, nullptr);
  }

  void InitComponentExtensionIMEManager() {
    // Set our custom IME list on the mock delegate.
    input_method::MockComponentExtensionIMEManagerDelegate* mock_delegate =
        new input_method::MockComponentExtensionIMEManagerDelegate();
    mock_delegate->set_ime_list(CreateImeList());

    // Pass the mock delegate to a new ComponentExtensionIMEManager.
    std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate(
        mock_delegate);
    std::unique_ptr<ComponentExtensionIMEManager>
        component_extension_ime_manager(
            new ComponentExtensionIMEManager(std::move(delegate)));

    // Add the ComponentExtensionIMEManager to the mock InputMethodManager.
    mock_manager_->SetComponentExtensionIMEManager(
        std::move(component_extension_ime_manager));
  }

  std::vector<ComponentExtensionIME> CreateImeList() {
    std::vector<ComponentExtensionIME> ime_list;

    ComponentExtensionIME ext_xkb;
    ext_xkb.id = extension_ime_util::kXkbExtensionId;
    ext_xkb.description = "ext_xkb_description";
    ext_xkb.path = base::FilePath("ext_xkb_file_path");

    ComponentExtensionEngine ext_xkb_engine_se;
    ext_xkb_engine_se.engine_id = "xkb:se::swe";
    ext_xkb_engine_se.display_name = "xkb:se::swe";
    ext_xkb_engine_se.language_codes.push_back("sv");
    ext_xkb_engine_se.layout = "se";
    ext_xkb.engines.push_back(ext_xkb_engine_se);

    ComponentExtensionEngine ext_xkb_engine_jp;
    ext_xkb_engine_jp.engine_id = "xkb:jp::jpn";
    ext_xkb_engine_jp.display_name = "xkb:jp::jpn";
    ext_xkb_engine_jp.language_codes.push_back("ja");
    ext_xkb_engine_jp.layout = "jp";
    ext_xkb.engines.push_back(ext_xkb_engine_jp);

    ComponentExtensionEngine ext_xkb_engine_ru;
    ext_xkb_engine_ru.engine_id = "xkb:ru::rus";
    ext_xkb_engine_ru.display_name = "xkb:ru::rus";
    ext_xkb_engine_ru.language_codes.push_back("ru");
    ext_xkb_engine_ru.layout = "ru";
    ext_xkb.engines.push_back(ext_xkb_engine_ru);

    ime_list.push_back(ext_xkb);

    ComponentExtensionIME ext;
    ext.id = extension_ime_util::kMozcExtensionId;
    ext.description = "ext_description";
    ext.path = base::FilePath("ext_file_path");

    ComponentExtensionEngine ext_engine1;
    ext_engine1.engine_id = "nacl_mozc_us";
    ext_engine1.display_name = "ext_engine_1_display_name";
    ext_engine1.language_codes.push_back("ja");
    ext_engine1.layout = "us";
    ext.engines.push_back(ext_engine1);

    ComponentExtensionEngine ext_engine2;
    ext_engine2.engine_id = "nacl_mozc_jp";
    ext_engine2.display_name = "ext_engine_2_display_name";
    ext_engine2.language_codes.push_back("ja");
    ext_engine2.layout = "jp";
    ext.engines.push_back(ext_engine2);

    ime_list.push_back(ext);
    return ime_list;
  }

  // Helper function to set local language and input values.
  void SetLocalValues(const std::string& preferred_languages,
                      const std::string& preload_engines,
                      const std::string& enabled_imes) {
    preferred_languages_.SetValue(preferred_languages);
    preload_engines_.SetValue(preload_engines);
    enabled_imes_.SetValue(enabled_imes);
  }

  // Helper function to set global language and input values.
  void SetGlobalValues(const std::string& preferred_languages,
                       const std::string& preload_engines,
                       const std::string& enabled_imes) {
    preferred_languages_syncable_.SetValue(preferred_languages);
    preload_engines_syncable_.SetValue(preload_engines);
    enabled_imes_syncable_.SetValue(enabled_imes);
  }

  // Helper function to check local language and input values.
  void ExpectLocalValues(const std::string& preferred_languages,
                         const std::string& preload_engines,
                         const std::string& enabled_imes) {
    EXPECT_EQ(preferred_languages, preferred_languages_.GetValue());
    EXPECT_EQ(preload_engines, preload_engines_.GetValue());
    EXPECT_EQ(enabled_imes, enabled_imes_.GetValue());
  }

  // Helper function to check global language and input values.
  void ExpectGlobalValues(const std::string& preferred_languages,
                          const std::string& preload_engines,
                          const std::string& enabled_imes) {
    EXPECT_EQ(preferred_languages, preferred_languages_syncable_.GetValue());
    EXPECT_EQ(preload_engines, preload_engines_syncable_.GetValue());
    EXPECT_EQ(enabled_imes, enabled_imes_syncable_.GetValue());
  }

  // Translates engine IDs in a CSV string to input method IDs.
  std::string ToInputMethodIds(const std::string& value) {
    return base::JoinString(
        base::ToVector(base::SplitString(value, ",", base::TRIM_WHITESPACE,
                                         base::SPLIT_WANT_ALL),
                       &extension_ime_util::GetInputMethodIDByEngineID),
        ",");
  }

  // Simulates the initial sync of preferences.
  syncer::SyncableService* SyncPreferences(
      const syncer::SyncDataList& sync_data_list) {
    syncer::SyncableService* sync =
        pref_service_->GetSyncableService(syncer::OS_PREFERENCES);
    sync->MergeDataAndStartSyncing(
        syncer::OS_PREFERENCES, sync_data_list,
        std::make_unique<syncer::FakeSyncChangeProcessor>());
    content::RunAllTasksUntilIdle();
    return sync;
  }

  StringPrefMember preferred_languages_;
  StringPrefMember preferred_languages_syncable_;
  StringPrefMember preload_engines_;
  StringPrefMember preload_engines_syncable_;
  StringPrefMember enabled_imes_;
  StringPrefMember enabled_imes_syncable_;
};

// Tests that the server values are added to the values chosen at OOBE.
TEST_F(InputMethodPreferencesTest, TestOobeAndSync) {
  // Choose options at OOBE.
  pref_service_->SetBoolean(
      prefs::kLanguageShouldMergeInputMethods, true);
  SetLocalValues("es",
                 ToInputMethodIds("xkb:us:altgr-intl:eng"),
                 std::string());
  InitPreferences();

  // Suppose we add an input method before syncing starts.
  preload_engines_.SetValue(
      ToInputMethodIds("xkb:us:altgr-intl:eng,xkb:us:intl:eng"));

  // Create some values to come from the server.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreatePrefSyncData(
      language::prefs::kPreferredLanguagesSyncable, base::Value("ru,fi")));
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kLanguagePreloadEnginesSyncable, base::Value("xkb:se::swe")));
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kLanguageEnabledImesSyncable, base::Value(kIdentityIMEID)));

  // Sync for the first time.
  syncer::SyncableService* sync = SyncPreferences(sync_data_list);

  // Note that we expect the preload_engines to have been translated to input
  // method IDs during the merge.
  std::string expected_languages("es,ru,fi");
  std::string expected_preload_engines(
      "xkb:us:altgr-intl:eng,xkb:us:intl:eng,xkb:se::swe");
  std::string expected_extensions(kIdentityIMEID);
  {
    SCOPED_TRACE("Server values should have merged into local values.");
    ExpectLocalValues(
        expected_languages,
        ToInputMethodIds(expected_preload_engines),
        expected_extensions);
  }
  {
    SCOPED_TRACE("Server values should have been updated to local values.");
    ExpectGlobalValues(
        expected_languages, expected_preload_engines, expected_extensions);
  }

  // Update the global values from the server again.
  syncer::SyncChangeList change_list;
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreatePrefSyncData(language::prefs::kPreferredLanguagesSyncable,
                         base::Value("de"))));
  change_list.push_back(syncer::SyncChange(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreatePrefSyncData(prefs::kLanguagePreloadEnginesSyncable,
                         base::Value(ToInputMethodIds("xkb:de::ger")))));
  change_list.push_back(
      syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                         CreatePrefSyncData(prefs::kLanguageEnabledImesSyncable,
                                            base::Value(kToUpperIMEID))));
  sync->ProcessSyncChanges(FROM_HERE, change_list);
  content::RunAllTasksUntilIdle();

  {
    SCOPED_TRACE("Local preferences should have remained the same.");
    ExpectLocalValues(
        expected_languages,
        ToInputMethodIds(expected_preload_engines),
        expected_extensions);
  }
  // Change local preferences.
  SetLocalValues("ja", ToInputMethodIds("xkb:jp::jpn"), "ime2");
  {
    SCOPED_TRACE("Global preferences should have been updated.");
    ExpectGlobalValues("ja", "xkb:jp::jpn", "ime2");
  }
  content::RunAllTasksUntilIdle();
}

// Tests that logging in after sync has completed changes nothing.
TEST_F(InputMethodPreferencesTest, TestLogIn) {
  // Set up existing preference values.
  std::string languages("es");
  std::string preload_engines(ToInputMethodIds("xkb:es::spa"));
  std::string extensions(kIdentityIMEID);

  SetLocalValues(languages, preload_engines, extensions);
  SetGlobalValues(languages, preload_engines, extensions);
  pref_service_->SetBoolean(
      prefs::kLanguageShouldMergeInputMethods, false);
  InitPreferences();

  // Create some values to come from the server.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreatePrefSyncData(
      language::prefs::kPreferredLanguages, base::Value("ru,fi")));
  sync_data_list.push_back(
      CreatePrefSyncData(prefs::kLanguagePreloadEngines,
                         base::Value(ToInputMethodIds("xkb:ru::rus"))));
  sync_data_list.push_back(CreatePrefSyncData(prefs::kLanguageEnabledImes,
                                              base::Value(kIdentityIMEID)));

  // Sync.
  SyncPreferences(sync_data_list);
  {
    SCOPED_TRACE("Local preferences should have remained the same.");
    ExpectLocalValues(languages, preload_engines, extensions);
  }
  // Change local preferences.
  SetLocalValues("ja", ToInputMethodIds("xkb:jp::jpn"), kToUpperIMEID);
  content::RunAllTasksUntilIdle();
  {
    SCOPED_TRACE("Global preferences should have been updated.");
    ExpectGlobalValues("ja", "xkb:jp::jpn", kToUpperIMEID);
  }
}

// Tests that logging in with preferences from before a) XKB component
// extensions and b) the IME syncing logic doesn't overwrite settings.
TEST_F(InputMethodPreferencesTest, TestLogInLegacy) {
  // Simulate existing local preferences from M-36.
  SetLocalValues("es", "xkb:es::spa", kIdentityIMEID);
  InitPreferences();

  // Sync. Since this is an existing profile, the local values shouldn't change.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreatePrefSyncData(
      language::prefs::kPreferredLanguagesSyncable, base::Value("ru,fi")));
  sync_data_list.push_back(
      CreatePrefSyncData(prefs::kLanguagePreloadEnginesSyncable,
                         base::Value(ToInputMethodIds("xkb:ru::rus"))));
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kLanguageEnabledImesSyncable, base::Value(kToUpperIMEID)));

  // Sync.
  SyncPreferences(sync_data_list);
  {
    SCOPED_TRACE("Local preferences should have remained the same.");
    ExpectLocalValues("es", "xkb:es::spa", kIdentityIMEID);
  }
  {
    SCOPED_TRACE("Global preferences should have remained the same.");
    ExpectGlobalValues("ru,fi", ToInputMethodIds("xkb:ru::rus"), kToUpperIMEID);
  }
  // Change local preferences.
  SetLocalValues("ja", ToInputMethodIds("xkb:jp::jp"), kAPIArgumentIMEID);
  {
    SCOPED_TRACE("Global preferences should have been updated.");
    ExpectGlobalValues("ja", "xkb:jp::jp", kAPIArgumentIMEID);
  }
}

// Tests some edge cases: empty strings, lots of values, duplicates.
TEST_F(InputMethodPreferencesTest, MergeStressTest) {
  SetLocalValues("hr,lv,lt,es-419,he,el,da,ca,es,cs,bg",
                 ToInputMethodIds("xkb:es::spa,xkb:us::eng"),
                 std::string());
  pref_service_->SetBoolean(
      prefs::kLanguageShouldMergeInputMethods, true);
  InitPreferences();

  // Change input methods and languages before syncing starts.
  std::string local_extensions =
      kToUpperIMEID + std::string(",") +
      kAPIArgumentIMEID + std::string(",") +
      kIdentityIMEID;
  SetLocalValues(
      "en,es,ja,hr,lv,lt,es-419,he,el,da,ca,es,cs,bg,ar",
      ToInputMethodIds("xkb:es::spa,xkb:us:dvorak,xkb:ua::ukr"),
      local_extensions);

  // Create some tricky values to come from the server.
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(
      CreatePrefSyncData(language::prefs::kPreferredLanguagesSyncable,
                         base::Value("ar,fi,es,de,ar")));
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kLanguagePreloadEnginesSyncable,
      base::Value(
          "nacl_mozc_us,xkb:ru::rus,xkb:ru::rus,xkb:es::spa,xkb:es::spa")));
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kLanguageEnabledImesSyncable, base::Value(std::string())));

  // Sync for the first time.
  SyncPreferences(sync_data_list);
  {
    SCOPED_TRACE("Server values should have merged into local values.");
    ExpectLocalValues(
        "en,es,ja,hr,lv,lt,es-419,he,el,da,ca,es,cs,bg,ar,fi,de",
        ToInputMethodIds("xkb:es::spa,xkb:us:dvorak,xkb:ua::ukr,"
                         "nacl_mozc_us,xkb:ru::rus"),
        local_extensions);
  }
  {
    SCOPED_TRACE("Server values should have incorporated local values.");
    ExpectGlobalValues(
        "en,es,ja,hr,lv,lt,es-419,he,el,da,ca,es,cs,bg,ar,fi,de",
        "xkb:es::spa,xkb:us:dvorak,xkb:ua::ukr,nacl_mozc_us,xkb:ru::rus",
        local_extensions);
  }
}

// Tests non-existent IDs.
TEST_F(InputMethodPreferencesTest, MergeInvalidValues) {
  SetLocalValues("es",
                 ToInputMethodIds("xkb:es::spa,xkb:us::eng"),
                 kIdentityIMEID);
  pref_service_->SetBoolean(
      prefs::kLanguageShouldMergeInputMethods, true);
  InitPreferences();

  // Create some valid and some non-existent IDs from the server.
  std::string preload_engines(
      "xkb:ru::rus,xkb:xy::xyz,"
      "_comp_ime_nothisisnotactuallyanextensionidxkb:es::spa," +
      ToInputMethodIds("xkb:jp::jpn"));
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(
      CreatePrefSyncData(language::prefs::kPreferredLanguagesSyncable,
                         base::Value("klingon,en-US")));
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kLanguagePreloadEnginesSyncable, base::Value(preload_engines)));
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kLanguageEnabledImesSyncable, base::Value(kUnknownIMEID)));

  // Sync for the first time.
  SyncPreferences(sync_data_list);
  {
    SCOPED_TRACE("Only valid server values should have been merged in.");
    ExpectLocalValues(
        "es,en-US",
        ToInputMethodIds("xkb:es::spa,xkb:us::eng,xkb:ru::rus,xkb:jp::jpn"),
        kIdentityIMEID);
  }
}

// Tests that we merge input methods even if syncing has started before
// initialization of Preferences.
TEST_F(InputMethodPreferencesTest, MergeAfterSyncing) {
  SetLocalValues("es",
                 ToInputMethodIds("xkb:es::spa,xkb:us::eng"),
                 kIdentityIMEID);
  pref_service_->SetBoolean(
      prefs::kLanguageShouldMergeInputMethods, true);

  // Create some valid and some non-existent IDs from the server.
  std::string preload_engines(
      "xkb:ru::rus,xkb:xy::xyz," + ToInputMethodIds("xkb:jp::jpn"));
  syncer::SyncDataList sync_data_list;
  sync_data_list.push_back(CreatePrefSyncData(
      language::prefs::kPreferredLanguagesSyncable, base::Value("en-US")));
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kLanguagePreloadEnginesSyncable, base::Value(preload_engines)));
  sync_data_list.push_back(CreatePrefSyncData(
      prefs::kLanguageEnabledImesSyncable, base::Value(kUnknownIMEID)));

  // Sync for the first time.
  SyncPreferences(sync_data_list);

  InitPreferences();
  content::RunAllTasksUntilIdle();

  {
    SCOPED_TRACE("Local values should have been merged on initialization.");
    ExpectLocalValues(
        "es,en-US",
        ToInputMethodIds("xkb:es::spa,xkb:us::eng,xkb:ru::rus,xkb:jp::jpn"),
        kIdentityIMEID);
  }
  {
    SCOPED_TRACE(
        "Syncable values should have added local values on initialization.");
    ExpectGlobalValues(
        "es,en-US",
        "xkb:es::spa,xkb:us::eng,xkb:ru::rus,xkb:xy::xyz," +
            ToInputMethodIds("xkb:jp::jpn"),
        std::string(kIdentityIMEID) + "," + kUnknownIMEID);
  }
}

}  // namespace ash
