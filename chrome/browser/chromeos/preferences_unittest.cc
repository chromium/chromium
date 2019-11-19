// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/preference_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_whitelist.h"
#include "ui/base/ime/chromeos/mock_component_extension_ime_manager_delegate.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

const char kIdentityIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpIdentityIME";
const char kToUpperIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpToUpperIME";
const char kAPIArgumentIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpAPIArgumentIME";
const char kUnknownIMEID[] =
    "_ext_ime_iafoklpfplgfnoimmaejoeondnjnlcfpUnknownIME";

syncer::SyncData
CreatePrefSyncData(const std::string& name, const base::Value& value) {
  std::string serialized;
  JSONStringValueSerializer json(&serialized);
  json.Serialize(value);
  sync_pb::EntitySpecifics specifics;
  sync_pb::PreferenceSpecifics* pref = specifics.mutable_preference();
  pref->set_name(name);
  pref->set_value(serialized);
  return syncer::SyncData::CreateRemoteData(1, specifics);
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
      input_method_extensions_.reset(new InputMethodDescriptors);
    }

    void ChangeInputMethod(const std::string& input_method_id,
                           bool show_message) override {
      manager_->last_input_method_id_ = input_method_id;
      // Do the same thing as BrowserStateMonitor::UpdateUserPreferences.
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

    void AddInputMethodExtension(
        const std::string& id,
        const InputMethodDescriptors& descriptors,
        ui::IMEEngineHandlerInterface* instance) override {
      InputMethodDescriptor descriptor(
          id, std::string(), std::string(), std::vector<std::string>(),
          std::vector<std::string>(), false, GURL(), GURL());
      input_method_extensions_->push_back(descriptor);
    }

   protected:
    ~State() override {}

   private:
    MyMockInputMethodManager* const manager_;
    std::unique_ptr<InputMethodDescriptors> input_method_extensions_;
  };

  MyMockInputMethodManager(StringPrefMember* previous,
                           StringPrefMember* current)
      : previous_(previous),
        current_(current) {
    state_ = new State(this);
  }

  ~MyMockInputMethodManager() override {}

  std::unique_ptr<InputMethodDescriptors> GetSupportedInputMethods()
      const override {
    return whitelist_.GetSupportedInputMethods();
  }

  std::string last_input_method_id_;

 private:
  StringPrefMember* previous_;
  StringPrefMember* current_;
  InputMethodWhitelist whitelist_;
};

}  // anonymous namespace
}  // namespace input_method

class PreferencesTest : public testing::Test {
 public:
  PreferencesTest() {}
  ~PreferencesTest() override {}

  void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    chromeos::FakeChromeUserManager* user_manager =
        new chromeos::FakeChromeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager));

    const char test_user_email[] = "test_user@example.com";
    const AccountId test_account_id(AccountId::FromUserEmail(test_user_email));
    test_user_ = user_manager->AddUser(test_account_id);
    user_manager->LoginUser(test_account_id);
    user_manager->SwitchActiveUser(test_account_id);

    test_profile_ = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile);
    pref_service_ = test_profile_->GetTestingPrefService();

    previous_input_method_.Init(
        prefs::kLanguagePreviousInputMethod, pref_service_);
    previous_input_method_.SetValue("KeyboardA");
    current_input_method_.Init(
        prefs::kLanguageCurrentInputMethod, pref_service_);
    current_input_method_.SetValue("KeyboardB");

    mock_manager_ = new input_method::MyMockInputMethodManager(
        &previous_input_method_, &current_input_method_);
    input_method::InitializeForTesting(mock_manager_);

    prefs_.reset(new Preferences(mock_manager_));
  }

  void TearDown() override {
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

  // Not owned.
  const user_manager::User* test_user_;
  TestingProfile* test_profile_;
  sync_preferences::TestingPrefServiceSyncable* pref_service_;
  input_method::MyMockInputMethodManager* mock_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreferencesTest);
};

TEST_F(PreferencesTest, TestUpdatePrefOnBrowserScreenDetails) {
  InitPreferences();

  // Confirm the current and previous input methods are unchanged.
  EXPECT_EQ("KeyboardA", previous_input_method_.GetValue());
  EXPECT_EQ("KeyboardB", current_input_method_.GetValue());
  EXPECT_EQ("KeyboardB", mock_manager_->last_input_method_id_);
}

class InputMethodPreferencesTest : public PreferencesTest {
 public:
  InputMethodPreferencesTest() {}
  ~InputMethodPreferencesTest() override {}

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
    input_method::MockComponentExtIMEManagerDelegate* mock_delegate =
        new input_method::MockComponentExtIMEManagerDelegate();
    mock_delegate->set_ime_list(CreateImeList());

    // Pass the mock delegate to a new ComponentExtensionIMEManager.
    std::unique_ptr<ComponentExtensionIMEManagerDelegate> delegate(
        mock_delegate);
    std::unique_ptr<ComponentExtensionIMEManager>
        component_extension_ime_manager(new ComponentExtensionIMEManager);
    component_extension_ime_manager->Initialize(std::move(delegate));

    // Add the ComponentExtensionIMEManager to the mock InputMethodManager.
    mock_manager_->SetComponentExtensionIMEManager(
        std::move(component_extension_ime_manager));
  }

  std::vector<ComponentExtensionIME> CreateImeList() {
    std::vector<ComponentExtensionIME> ime_list;

    ComponentExtensionIME ext;
    ext.id = extension_ime_util::kMozcExtensionId;
    ext.description = "ext_description";
    ext.path = base::FilePath("ext_file_path");

    ComponentExtensionEngine ext_engine1;
    ext_engine1.engine_id = "nacl_mozc_us";
    ext_engine1.display_name = "ext_engine_1_display_name";
    ext_engine1.language_codes.push_back("ja");
    ext_engine1.layouts.push_back("us");
    ext.engines.push_back(ext_engine1);

    ComponentExtensionEngine ext_engine2;
    ext_engine2.engine_id = "nacl_mozc_jp";
    ext_engine2.display_name = "ext_engine_2_display_name";
    ext_engine2.language_codes.push_back("ja");
    ext_engine2.layouts.push_back("jp");
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
    std::vector<std::string> tokens = base::SplitString(
        value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    std::transform(tokens.begin(), tokens.end(), tokens.begin(),
                   &extension_ime_util::GetInputMethodIDByEngineID);
    return base::JoinString(tokens, ",");
  }

  StringPrefMember preferred_languages_;
  StringPrefMember preferred_languages_syncable_;
  StringPrefMember preload_engines_;
  StringPrefMember preload_engines_syncable_;
  StringPrefMember enabled_imes_;
  StringPrefMember enabled_imes_syncable_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodPreferencesTest);
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
  syncer::SyncableService* sync =
      pref_service_->GetSyncableService(
          syncer::PREFERENCES);
  sync->MergeDataAndStartSyncing(syncer::PREFERENCES, sync_data_list,
                                 std::unique_ptr<syncer::SyncChangeProcessor>(
                                     new syncer::FakeSyncChangeProcessor),
                                 std::unique_ptr<syncer::SyncErrorFactory>(
                                     new syncer::SyncErrorFactoryMock));
  content::RunAllTasksUntilIdle();

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
  syncer::SyncableService* sync =
      pref_service_->GetSyncableService(
          syncer::PREFERENCES);
  sync->MergeDataAndStartSyncing(syncer::PREFERENCES, sync_data_list,
                                 std::unique_ptr<syncer::SyncChangeProcessor>(
                                     new syncer::FakeSyncChangeProcessor),
                                 std::unique_ptr<syncer::SyncErrorFactory>(
                                     new syncer::SyncErrorFactoryMock));
  content::RunAllTasksUntilIdle();
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

  syncer::SyncableService* sync =
      pref_service_->GetSyncableService(
          syncer::PREFERENCES);
  sync->MergeDataAndStartSyncing(syncer::PREFERENCES, sync_data_list,
                                 std::unique_ptr<syncer::SyncChangeProcessor>(
                                     new syncer::FakeSyncChangeProcessor),
                                 std::unique_ptr<syncer::SyncErrorFactory>(
                                     new syncer::SyncErrorFactoryMock));
  content::RunAllTasksUntilIdle();
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
  syncer::SyncableService* sync =
      pref_service_->GetSyncableService(
          syncer::PREFERENCES);
  sync->MergeDataAndStartSyncing(syncer::PREFERENCES, sync_data_list,
                                 std::unique_ptr<syncer::SyncChangeProcessor>(
                                     new syncer::FakeSyncChangeProcessor),
                                 std::unique_ptr<syncer::SyncErrorFactory>(
                                     new syncer::SyncErrorFactoryMock));
  content::RunAllTasksUntilIdle();
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
  syncer::SyncableService* sync =
      pref_service_->GetSyncableService(
          syncer::PREFERENCES);
  sync->MergeDataAndStartSyncing(syncer::PREFERENCES, sync_data_list,
                                 std::unique_ptr<syncer::SyncChangeProcessor>(
                                     new syncer::FakeSyncChangeProcessor),
                                 std::unique_ptr<syncer::SyncErrorFactory>(
                                     new syncer::SyncErrorFactoryMock));
  content::RunAllTasksUntilIdle();
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
  syncer::SyncableService* sync =
      pref_service_->GetSyncableService(
          syncer::PREFERENCES);
  sync->MergeDataAndStartSyncing(syncer::PREFERENCES, sync_data_list,
                                 std::unique_ptr<syncer::SyncChangeProcessor>(
                                     new syncer::FakeSyncChangeProcessor),
                                 std::unique_ptr<syncer::SyncErrorFactory>(
                                     new syncer::SyncErrorFactoryMock));
  content::RunAllTasksUntilIdle();
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

}  // namespace chromeos
