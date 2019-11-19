// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/extension_prefs_unittest.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;

namespace extensions {

namespace {

const char kPref1[] = "path1.subpath";
const char kPref2[] = "path2";
const char kPref3[] = "path3";
const char kPref4[] = "path4";

// Default values in case an extension pref value is not overridden.
const char kDefaultPref1[] = "default pref 1";
const char kDefaultPref2[] = "default pref 2";
const char kDefaultPref3[] = "default pref 3";
const char kDefaultPref4[] = "default pref 4";

}  // namespace

// An implementation of the PreferenceAPI which returns the ExtensionPrefs and
// ExtensionPrefValueMap from the TestExtensionPrefs, rather than from a
// profile (which we don't create in unittests).
class TestPreferenceAPI : public PreferenceAPIBase {
 public:
  explicit TestPreferenceAPI(TestExtensionPrefs* test_extension_prefs,
                             ContentSettingsService* content_settings)
      : test_extension_prefs_(test_extension_prefs),
        content_settings_(content_settings) {}
  ~TestPreferenceAPI() {}

 private:
  // PreferenceAPIBase implementation.
  ExtensionPrefs* extension_prefs() override {
    return test_extension_prefs_->prefs();
  }
  ExtensionPrefValueMap* extension_pref_value_map() override {
    return test_extension_prefs_->extension_pref_value_map();
  }
  scoped_refptr<ContentSettingsStore> content_settings_store() override {
    return content_settings_->content_settings_store();
  }

  TestExtensionPrefs* test_extension_prefs_;
  ContentSettingsService* content_settings_;

  DISALLOW_COPY_AND_ASSIGN(TestPreferenceAPI);
};

class ExtensionControlledPrefsTest : public PrefsPrepopulatedTestBase {
 public:
  ExtensionControlledPrefsTest();
  ~ExtensionControlledPrefsTest() override;

  void RegisterPreferences(user_prefs::PrefRegistrySyncable* registry) override;
  void InstallExtensionControlledPref(Extension* extension,
                                      const std::string& key,
                                      base::Value value);
  void InstallExtensionControlledPrefIncognito(Extension* extension,
                                               const std::string& key,
                                               base::Value value);
  void InstallExtensionControlledPrefIncognitoSessionOnly(
      Extension* extension,
      const std::string& key,
      base::Value value);
  void InstallExtension(Extension* extension);
  void UninstallExtension(const std::string& extension_id);

  scoped_refptr<ContentSettingsStore> content_settings_store() {
    return content_settings_->content_settings_store();
  }

 protected:
  void EnsureExtensionInstalled(Extension* extension);
  void EnsureExtensionUninstalled(const std::string& extension_id);

  TestingProfile profile_;
  ContentSettingsService* content_settings_;
  TestPreferenceAPI test_preference_api_;
};

ExtensionControlledPrefsTest::ExtensionControlledPrefsTest()
    : PrefsPrepopulatedTestBase(),
      content_settings_(ContentSettingsService::Get(&profile_)),
      test_preference_api_(&prefs_, content_settings_) {
  content_settings_->OnExtensionPrefsAvailable(prefs_.prefs());
}

ExtensionControlledPrefsTest::~ExtensionControlledPrefsTest() {
}

void ExtensionControlledPrefsTest::RegisterPreferences(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kPref1, kDefaultPref1);
  registry->RegisterStringPref(kPref2, kDefaultPref2);
  registry->RegisterStringPref(kPref3, kDefaultPref3);
  registry->RegisterStringPref(kPref4, kDefaultPref4);
}

void ExtensionControlledPrefsTest::InstallExtensionControlledPref(
    Extension* extension,
    const std::string& key,
    base::Value value) {
  EnsureExtensionInstalled(extension);
  test_preference_api_.SetExtensionControlledPref(
      extension->id(), key, kExtensionPrefsScopeRegular, std::move(value));
}

void ExtensionControlledPrefsTest::InstallExtensionControlledPrefIncognito(
    Extension* extension,
    const std::string& key,
    base::Value value) {
  EnsureExtensionInstalled(extension);
  test_preference_api_.SetExtensionControlledPref(
      extension->id(), key, kExtensionPrefsScopeIncognitoPersistent,
      std::move(value));
}

void ExtensionControlledPrefsTest::
    InstallExtensionControlledPrefIncognitoSessionOnly(Extension* extension,
                                                       const std::string& key,
                                                       base::Value value) {
  EnsureExtensionInstalled(extension);
  test_preference_api_.SetExtensionControlledPref(
      extension->id(), key, kExtensionPrefsScopeIncognitoSessionOnly,
      std::move(value));
}

void ExtensionControlledPrefsTest::InstallExtension(Extension* extension) {
  EnsureExtensionInstalled(extension);
}

void ExtensionControlledPrefsTest::UninstallExtension(
    const std::string& extension_id) {
  EnsureExtensionUninstalled(extension_id);
}

void ExtensionControlledPrefsTest::EnsureExtensionInstalled(
    Extension* extension) {
  // Install extension the first time a preference is set for it.
  Extension* extensions[] = {extension1(), extension2(), extension3(),
                             extension4(), internal_extension()};
  for (size_t i = 0; i < kNumInstalledExtensions; ++i) {
    if (extension == extensions[i] && !installed_[i]) {
      prefs()->OnExtensionInstalled(extension,
                                    Extension::ENABLED,
                                    syncer::StringOrdinal(),
                                    std::string());
      prefs()->SetIsIncognitoEnabled(extension->id(), true);
      installed_[i] = true;
      break;
    }
  }
}

void ExtensionControlledPrefsTest::EnsureExtensionUninstalled(
    const std::string& extension_id) {
  Extension* extensions[] = {extension1(), extension2(), extension3(),
                             extension4(), internal_extension()};
  for (size_t i = 0; i < kNumInstalledExtensions; ++i) {
    if (extensions[i]->id() == extension_id) {
      installed_[i] = false;
      break;
    }
  }
  prefs()->OnExtensionUninstalled(extension_id, Manifest::INTERNAL, false);
}

class ControlledPrefsInstallOneExtension
    : public ExtensionControlledPrefsTest {
  void Initialize() override {
    InstallExtensionControlledPref(extension1(), kPref1, base::Value("val1"));
  }
  void Verify() override {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
  }
};
TEST_F(ControlledPrefsInstallOneExtension,
       ControlledPrefsInstallOneExtension) { }

// Check that we do not forget persistent incognito values after a reload.
class ControlledPrefsInstallIncognitoPersistent
    : public ExtensionControlledPrefsTest {
 public:
  void Initialize() override {
    InstallExtensionControlledPref(extension1(), kPref1, base::Value("val1"));
    InstallExtensionControlledPrefIncognito(extension1(), kPref1,
                                            base::Value("val2"));
    std::unique_ptr<PrefService> incog_prefs =
        prefs_.CreateIncognitoPrefService();
    std::string actual = incog_prefs->GetString(kPref1);
    EXPECT_EQ("val2", actual);
  }

  void Verify() override {
    // Main pref service shall see only non-incognito settings.
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
    // Incognito pref service shall see incognito values.
    std::unique_ptr<PrefService> incog_prefs =
        prefs_.CreateIncognitoPrefService();
    actual = incog_prefs->GetString(kPref1);
    EXPECT_EQ("val2", actual);
  }
};
TEST_F(ControlledPrefsInstallIncognitoPersistent,
       ControlledPrefsInstallIncognitoPersistent) { }

// Check that we forget 'session only' incognito values after a reload.
class ControlledPrefsInstallIncognitoSessionOnly
    : public ExtensionControlledPrefsTest {
 public:
  ControlledPrefsInstallIncognitoSessionOnly() : iteration_(0) {}

  void Initialize() override {
    InstallExtensionControlledPref(extension1(), kPref1, base::Value("val1"));
    InstallExtensionControlledPrefIncognitoSessionOnly(extension1(), kPref1,
                                                       base::Value("val2"));
    std::unique_ptr<PrefService> incog_prefs =
        prefs_.CreateIncognitoPrefService();
    std::string actual = incog_prefs->GetString(kPref1);
    EXPECT_EQ("val2", actual);
  }
  void Verify() override {
    // Main pref service shall see only non-incognito settings.
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
    // Incognito pref service shall see session-only incognito values only
    // during first run. Once the pref service was reloaded, all values shall be
    // discarded.
    std::unique_ptr<PrefService> incog_prefs =
        prefs_.CreateIncognitoPrefService();
    actual = incog_prefs->GetString(kPref1);
    if (iteration_ == 0) {
      EXPECT_EQ("val2", actual);
    } else {
      EXPECT_EQ("val1", actual);
    }
    ++iteration_;
  }
  int iteration_;
};
TEST_F(ControlledPrefsInstallIncognitoSessionOnly,
       ControlledPrefsInstallIncognitoSessionOnly) { }

class ControlledPrefsUninstallExtension : public ExtensionControlledPrefsTest {
  void Initialize() override {
    InstallExtensionControlledPref(extension1(), kPref1, base::Value("val1"));
    InstallExtensionControlledPref(extension1(), kPref2, base::Value("val2"));
    scoped_refptr<ContentSettingsStore> store = content_settings_store();
    ContentSettingsPattern pattern =
        ContentSettingsPattern::FromString("http://[*.]example.com");
    store->SetExtensionContentSetting(
        extension1()->id(), pattern, pattern, ContentSettingsType::IMAGES,
        std::string(), CONTENT_SETTING_BLOCK, kExtensionPrefsScopeRegular);

    UninstallExtension(extension1()->id());
  }
  void Verify() override {
    EXPECT_FALSE(prefs()->HasPrefForExtension(extension1()->id()));

    std::string actual;
    actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ(kDefaultPref1, actual);
    actual = prefs()->pref_service()->GetString(kPref2);
    EXPECT_EQ(kDefaultPref2, actual);
  }
};
TEST_F(ControlledPrefsUninstallExtension,
       ControlledPrefsUninstallExtension) { }

// Tests triggering of notifications to registered observers.
class ControlledPrefsNotifyWhenNeeded : public ExtensionControlledPrefsTest {
  void Initialize() override {
    using testing::_;
    using testing::Mock;
    using testing::StrEq;

    MockPrefChangeCallback observer(prefs()->pref_service());
    PrefChangeRegistrar registrar;
    registrar.Init(prefs()->pref_service());
    registrar.Add(kPref1, observer.GetCallback());

    MockPrefChangeCallback incognito_observer(prefs()->pref_service());
    std::unique_ptr<PrefService> incog_prefs =
        prefs_.CreateIncognitoPrefService();
    PrefChangeRegistrar incognito_registrar;
    incognito_registrar.Init(incog_prefs.get());
    incognito_registrar.Add(kPref1, incognito_observer.GetCallback());

    // Write value and check notification.
    EXPECT_CALL(observer, OnPreferenceChanged(_));
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    InstallExtensionControlledPref(extension1(), kPref1,
                                   base::Value("https://www.chromium.org"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Write same value.
    EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(0);
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_)).Times(0);
    InstallExtensionControlledPref(extension1(), kPref1,
                                   base::Value("https://www.chromium.org"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Change value.
    EXPECT_CALL(observer, OnPreferenceChanged(_));
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    InstallExtensionControlledPref(extension1(), kPref1,
                                   base::Value("chrome://newtab"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);
    // Change only incognito persistent value.
    EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(0);
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    InstallExtensionControlledPrefIncognito(extension1(), kPref1,
                                            base::Value("chrome://newtab2"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Change only incognito session-only value.
    EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(0);
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    InstallExtensionControlledPrefIncognito(extension1(), kPref1,
                                            base::Value("chrome://newtab3"));
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    // Uninstall.
    EXPECT_CALL(observer, OnPreferenceChanged(_));
    EXPECT_CALL(incognito_observer, OnPreferenceChanged(_));
    UninstallExtension(extension1()->id());
    Mock::VerifyAndClearExpectations(&observer);
    Mock::VerifyAndClearExpectations(&incognito_observer);

    registrar.Remove(kPref1);
    incognito_registrar.Remove(kPref1);
  }
  void Verify() override {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ(kDefaultPref1, actual);
  }
};
TEST_F(ControlledPrefsNotifyWhenNeeded,
       ControlledPrefsNotifyWhenNeeded) { }

// Tests disabling an extension.
class ControlledPrefsDisableExtension : public ExtensionControlledPrefsTest {
  void Initialize() override {
    InstallExtensionControlledPref(extension1(), kPref1, base::Value("val1"));
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
    prefs()->SetExtensionDisabled(extension1()->id(),
                                  disable_reason::DISABLE_USER_ACTION);
  }
  void Verify() override {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ(kDefaultPref1, actual);
  }
};
TEST_F(ControlledPrefsDisableExtension, ControlledPrefsDisableExtension) { }

// Tests disabling and reenabling an extension.
class ControlledPrefsReenableExtension : public ExtensionControlledPrefsTest {
  void Initialize() override {
    InstallExtensionControlledPref(extension1(), kPref1, base::Value("val1"));
    prefs()->SetExtensionDisabled(extension1()->id(),
                                  disable_reason::DISABLE_USER_ACTION);
    prefs()->SetExtensionEnabled(extension1()->id());
  }
  void Verify() override {
    std::string actual = prefs()->pref_service()->GetString(kPref1);
    EXPECT_EQ("val1", actual);
  }
};
TEST_F(ControlledPrefsDisableExtension, ControlledPrefsReenableExtension) { }

class ControlledPrefsSetExtensionControlledPref
    : public ExtensionControlledPrefsTest {
 public:
  void Initialize() override {
    InstallExtensionControlledPref(extension1(), kPref1,
                                   base::Value("https://www.chromium.org"));
    InstallExtensionControlledPrefIncognito(
        extension1(), kPref1, base::Value("https://www.chromium.org"));
    prefs_.RecreateExtensionPrefs();
  }

  void Verify() override {}
};
TEST_F(ControlledPrefsSetExtensionControlledPref,
       ControlledPrefsSetExtensionControlledPref) { }

// Tests that the switches::kDisableExtensions command-line flag prevents
// extension controlled preferences from being enacted.
class ControlledPrefsDisableExtensions : public ExtensionControlledPrefsTest {
 public:
  ControlledPrefsDisableExtensions()
      : iteration_(0) {}
  ~ControlledPrefsDisableExtensions() override {}
  void Initialize() override {
    InstallExtensionControlledPref(internal_extension(), kPref1,
                                   base::Value("internal extension value"));

    EXPECT_TRUE(Manifest::IsExternalLocation(extension1()->location()));
    InstallExtensionControlledPref(extension1(), kPref2,
                                   base::Value("external extension value"));
    // This becomes only active in the second verification phase.
    prefs_.set_extensions_disabled(true);
  }
  void Verify() override {
    // Internal extensions are not loaded with --disable-extensions. This means
    // that the preference will be reset on the second verification run (when
    // the ExtensionPrefs are recreated).
    std::string pref1_actual = prefs()->pref_service()->GetString(kPref1);
    if (iteration_ == 0) {
      EXPECT_EQ("internal extension value", pref1_actual);
      ++iteration_;
    } else {
      EXPECT_EQ(kDefaultPref1, pref1_actual);
    }

    // External extensions are loaded even when extensions are disabled (though
    // they likely shouldn't be, see https://crbug.com/833540). Because of this,
    // the preference should still be controlled by the external extension.
    // Regression test for https://crbug.com/828295.
    std::string pref2_actual = prefs()->pref_service()->GetString(kPref2);
    EXPECT_EQ("external extension value", pref2_actual);
  }

 private:
  int iteration_;
};
TEST_F(ControlledPrefsDisableExtensions, ControlledPrefsDisableExtensions) { }

}  // namespace extensions
