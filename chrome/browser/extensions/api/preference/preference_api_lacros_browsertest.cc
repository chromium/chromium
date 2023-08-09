// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/prefs.mojom-shared.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ContextType = extensions::ExtensionBrowserTest::ContextType;

void SetPref(crosapi::mojom::PrefPath path, base::Value value) {
  base::test::TestFuture<void> future;
  chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      path, std::move(value), future.GetCallback());
  ASSERT_TRUE(future.Wait());
}

absl::optional<base::Value> GetPref(crosapi::mojom::PrefPath path) {
  base::test::TestFuture<absl::optional<base::Value>> future;
  chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>()->GetPref(
      path, future.GetCallback());
  return future.Take();
}

}  // namespace

// Tests for extension-controlled prefs, where an extension in lacros sets a
// pref where the underlying feature lives in ash.
class ExtensionPreferenceApiLacrosBrowserTest
    : public extensions::ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionPreferenceApiLacrosBrowserTest(
      const ExtensionPreferenceApiLacrosBrowserTest&) = delete;
  ExtensionPreferenceApiLacrosBrowserTest& operator=(
      const ExtensionPreferenceApiLacrosBrowserTest&) = delete;

 protected:
  ExtensionPreferenceApiLacrosBrowserTest() : ExtensionApiTest(GetParam()) {}
  ~ExtensionPreferenceApiLacrosBrowserTest() override = default;

  void CheckPreferencesSet() {
    PrefService* prefs = profile_->GetPrefs();
    // From the lacros perspective, the pref should look extension controlled.
    const PrefService::Preference* pref =
        prefs->FindPreference(prefs::kLacrosAccessibilitySpokenFeedbackEnabled);
    ASSERT_TRUE(pref);
    EXPECT_TRUE(pref->IsExtensionControlled());
    EXPECT_TRUE(
        prefs->GetBoolean(prefs::kLacrosAccessibilitySpokenFeedbackEnabled));

    const PrefService::Preference* proxy_pref =
        prefs->FindPreference(proxy_config::prefs::kProxy);
    ASSERT_TRUE(proxy_pref);
    EXPECT_TRUE(proxy_pref->IsExtensionControlled());
    EXPECT_EQ(ProxyConfigDictionary::CreateDirect(),
              proxy_pref->GetValue()->GetDict());
  }

  void CheckPreferencesCleared() {
    PrefService* prefs = profile_->GetPrefs();
    const PrefService::Preference* pref =
        prefs->FindPreference(prefs::kLacrosAccessibilitySpokenFeedbackEnabled);
    ASSERT_TRUE(pref);
    EXPECT_FALSE(pref->IsExtensionControlled());
    EXPECT_FALSE(
        prefs->GetBoolean(prefs::kLacrosAccessibilitySpokenFeedbackEnabled));

    const PrefService::Preference* proxy_pref =
        prefs->FindPreference(proxy_config::prefs::kProxy);
    ASSERT_TRUE(proxy_pref);
    EXPECT_FALSE(proxy_pref->IsExtensionControlled());
    EXPECT_EQ(ProxyConfigDictionary::CreateSystem(),
              proxy_pref->GetValue()->GetDict());
  }

  void SetUp() override {
    // When the test changes the value of
    // chrome.accessibilityFeatures.autoclick in Ash, the pref value change is
    // observed by AccessibilityController and will trigger popping up a dialog
    // in Ash with the prompt about confirmation of disabling autoclick. The
    // dialog is not closed when the test is torn down in Lacros, and will
    // affect other tests running after it if the test runs with shared Ash.
    // Therefore, we start a unique Ash to run with this test suite to avoid
    // the test isolation issue.
    StartUniqueAshChrome(
        {}, {}, {},
        "crbug.com/1435317 Switch to shared ash when autoclick disable "
        "confirmation dialog issue is fixed");
    ExtensionApiTest::SetUp();
  }

  void SetUpOnMainThread() override {
    if (!IsServiceAvailable()) {
      GTEST_SKIP() << "The Lacros service is not available.";
    }
    extensions::ExtensionApiTest::SetUpOnMainThread();

    // The browser might get closed later (and therefore be destroyed), so we
    // save the profile.
    profile_ = browser()->profile();

    // Closing the last browser window also releases a module reference. Make
    // sure it's not the last one, so the message loop doesn't quit
    // unexpectedly.
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);
  }

  void TearDownOnMainThread() override {
    // BrowserProcess::Shutdown() needs to be called in a message loop, so we
    // post a task to release the keep alive, then run the message loop.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&std::unique_ptr<ScopedKeepAlive>::reset,
                                  base::Unretained(&keep_alive_), nullptr));
    content::RunAllTasksUntilIdle();

    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  bool IsServiceAvailable() {
    if (chromeos::LacrosService::Get()
            ->GetInterfaceVersion<crosapi::mojom::Prefs>() <
        static_cast<int>(crosapi::mojom::Prefs::MethodMinVersions::
                             kGetExtensionPrefWithControlMinVersion)) {
      LOG(WARNING) << "Unsupported ash version.";
      return false;
    }
    return true;
  }

  bool IsLacrosServiceSyncingProxyPref() {
    static constexpr int kMinVersionProxyPolicy = 4;
    const int version = chromeos::LacrosService::Get()
                            ->GetInterfaceVersion<crosapi::mojom::Prefs>();
    return version >= kMinVersionProxyPolicy;
  }

  bool DoesAshSupportObservers() {
    // Versions of ash without this capability cannot create observers for prefs
    // writing to the ash standalone browser prefstore.
    constexpr char kExtensionControlledPrefObserversCapability[] =
        "crbug/1334964";
    return chromeos::BrowserParamsProxy::Get()->AshCapabilities().has_value() &&
           base::Contains(
               chromeos::BrowserParamsProxy::Get()->AshCapabilities().value(),
               kExtensionControlledPrefObserversCapability);
  }

  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

INSTANTIATE_TEST_SUITE_P(EventPage,
                         ExtensionPreferenceApiLacrosBrowserTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionPreferenceApiLacrosBrowserTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiLacrosBrowserTest, Lacros) {
  // At start, the value in ash should not be set.
  absl::optional<base::Value> out_value =
      GetPref(crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled);
  EXPECT_FALSE(out_value.value().GetBool());

  extensions::ExtensionId test_extension_id;
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("preference/lacros");
  {
    extensions::ResultCatcher catcher;
    ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
    const extensions::Extension* extension = LoadExtension(extension_path);
    EXPECT_TRUE(extension) << message_;
    // Save the test extension ID rather than using last_loaded_extension_id as
    // toggling ChromeVox will cause a ChromeVox helper extension to be
    // installed in Lacros.
    test_extension_id = extension->id();
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    // Run the tests.
    listener.Reply("run test");
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
  CheckPreferencesSet();

  // In ash, the value should now be set.
  out_value =
      GetPref(crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled);
  EXPECT_TRUE(out_value.value().GetBool());
  if (IsLacrosServiceSyncingProxyPref()) {
    out_value = GetPref(crosapi::mojom::PrefPath::kProxy);
    EXPECT_EQ(out_value.value().GetDict(),
              ProxyConfigDictionary::CreateDirect());
  }
  // The settings should not be reset when the extension is reloaded.
  {
    ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
    ReloadExtension(test_extension_id);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    listener.Reply("");
  }
  CheckPreferencesSet();

  // Uninstalling and installing the extension (without running the test that
  // calls the extension API) should clear the settings.
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(profile_), test_extension_id);
  UninstallExtension(test_extension_id);
  observer.WaitForExtensionUninstalled();
  CheckPreferencesCleared();

  if (DoesAshSupportObservers()) {
    // When the extension in uninstalled, the pref in lacros should be the
    // default value (false). This only works if Ash correctly implements
    // extension-controlled pref observers.
    out_value =
        GetPref(crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled);
    EXPECT_FALSE(out_value.value().GetBool());

    if (IsLacrosServiceSyncingProxyPref()) {
      out_value = GetPref(crosapi::mojom::PrefPath::kProxy);
      EXPECT_EQ(out_value.value().GetDict(),
                ProxyConfigDictionary::CreateSystem());
    }
  }

  {
    ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
    EXPECT_TRUE(LoadExtension(extension_path));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    listener.Reply("");
  }
  CheckPreferencesCleared();
}

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiLacrosBrowserTest,
                       LacrosSecondaryProfile) {
  // At start, the value in ash should not be set.
  absl::optional<base::Value> out_value =
      GetPref(crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled);
  EXPECT_FALSE(out_value.value().GetBool());

  // Create a secondary profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile& secondary_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  ASSERT_FALSE(secondary_profile.IsMainProfile());

  // Load the testing extension in secondary profile.
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener_1("ready", ReplyBehavior::kWillReply);
  extensions::ChromeTestExtensionLoader loader(&secondary_profile);
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("preference/lacros_secondary_profile_read");
  scoped_refptr<const extensions::Extension> extension =
      loader.LoadExtension(extension_path);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener_1.WaitUntilSatisfied());

  // Run the test to verify that testing extension running in secondary
  // profile reads the default values of the Prefs correctly.
  listener_1.Reply("run test default value");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Set the pref value in ash.
  SetPref(crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
          base::Value(true));

  // Verify the value is set in ash side.
  out_value =
      GetPref(crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled);
  EXPECT_TRUE(out_value.value().GetBool());

  // Reload the testing extension in the secondary profile.
  ExtensionTestMessageListener listener_2("ready", ReplyBehavior::kWillReply);
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(&secondary_profile), extension->id());
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(&secondary_profile)->extension_service();
  extension_service->ReloadExtension(extension->id());
  observer.WaitForExtensionLoaded();
  EXPECT_TRUE(listener_2.WaitUntilSatisfied());

  // Run the test to verify that testing extension running in secondary
  // profile reads the changed value of the accessibilityFeatures correctly.
  listener_2.Reply("run test changed value");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Since lacros browser tests shared the same ash instance, we need to restore
  // the modified pref in ash to default before exiting the test, so that
  // it won't affect other lacros browser tests.
  SetPref(crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled,
          base::Value(false));
  out_value =
      GetPref(crosapi::mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled);
  EXPECT_FALSE(out_value.value().GetBool());
}

IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiLacrosBrowserTest, OnChange) {
  if (!DoesAshSupportObservers()) {
    LOG(WARNING) << "Ash does not support observers, skipping the test.";
    return;
  }
  EXPECT_TRUE(RunExtensionTest("preference/onchange_lacros", {},
                               {.allow_in_incognito = false}))
      << message_;
}

// The extension controlled pref observers are only instantiated when a listener
// is actually attached. The purpose of running this test is to ensure that they
// are instantiated as we use the prefs api to create the observer. See also
// crbug/1334985.
IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiLacrosBrowserTest,
                       CreateObservers) {
  EXPECT_TRUE(
      RunExtensionTest("preference/onchange", {}, {.allow_in_incognito = true}))
      << message_;
}

base::Value::Dict GetAshProxyPrefValue() {
  absl::optional<::base::Value> out_value =
      GetPref(crosapi::mojom::PrefPath::kProxy);
  return out_value.value().GetDict().Clone();
}

scoped_refptr<const extensions::Extension> InstallExtensionForProfile(
    Profile* profile,
    const base::FilePath& path) {
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(profile).LoadExtension(path);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  // Run the tests.
  listener.Reply("run test");
  EXPECT_TRUE(catcher.GetNextResult());
  return extension;
}

void ExpectThatProxyIsControlledByExtension(Profile* profile) {
  const PrefService::Preference* pref =
      profile->GetPrefs()->FindPreference(proxy_config::prefs::kProxy);
  EXPECT_TRUE(pref->IsExtensionControlled());
  EXPECT_EQ(ProxyConfigDictionary::CreateDirect(), pref->GetValue()->GetDict());
}

void ExpectThatProxyHasDefaultValue(Profile* profile) {
  const PrefService::Preference* pref =
      profile->GetPrefs()->FindPreference(proxy_config::prefs::kProxy);
  EXPECT_FALSE(pref->IsExtensionControlled());
  EXPECT_EQ(ProxyConfigDictionary::CreateSystem(), pref->GetValue()->GetDict());
}

// Secondary profiles should apply extension set proxy at browser level, but not
// in Ash.
IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiLacrosBrowserTest,
                       SecondaryProfilePrefs) {
  if (!IsServiceAvailable()) {
    return;
  }
  if (!IsLacrosServiceSyncingProxyPref()) {
    GTEST_SKIP() << "Skipping test because the current version of Ash does not "
                    "support getting the proxy preference from a Lacros "
                    "extension via the preferences service";
  }
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath path_profile =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& secondary_profile =
      profiles::testing::CreateProfileSync(profile_manager, path_profile);
  scoped_refptr<const extensions::Extension> extension =
      InstallExtensionForProfile(
          &secondary_profile,
          test_data_dir_.AppendASCII("preference/lacros_secondary_profile"));
  // Verify that the proxy is set by the extension for the secondary profile.
  ExpectThatProxyIsControlledByExtension(&secondary_profile);
  // The proxy should not be set in the primary profile and Ash.
  ExpectThatProxyHasDefaultValue(profile());
  EXPECT_EQ(GetAshProxyPrefValue(), ProxyConfigDictionary::CreateSystem());
}

// Clearing an extension set proxy in a secondary profile should not clear the
// extension set proxy in the primary profile and Ash (if the primary profile
// has an extension which controls the proxy). The test setup:
// - Create a secondary profile;
// - Install an extension which controls the proxy pref in the primary profile;
// - Install an extension which controls the proxy pref in the secondary
// profile;
// - Verify that both profiles have extension controlled proxy prefs;
// - Uninstall the proxy controlling extension in the secondary profile;
// - Verify that the secondary profile does not have an extension set proxy;
// - Verify that the primary profile and Ash still have an extension set proxy.
// This test can be extended to other prefs for which the primary profile
// controls the value in Ash but secondary profiles only control the pref
// value at browser level.
IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiLacrosBrowserTest,
                       SecondaryProfilePrefsClearPref) {
  if (!IsServiceAvailable()) {
    return;
  }
  if (!IsLacrosServiceSyncingProxyPref()) {
    GTEST_SKIP() << "Skipping test because the current version of Ash does not "
                    "support getting the proxy preference from a Lacros "
                    "extension via the preferences service";
  }
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath path_profile =
      profile_manager->GenerateNextProfileDirectoryPath();
  Profile& secondary_profile =
      profiles::testing::CreateProfileSync(profile_manager, path_profile);

  scoped_refptr<const extensions::Extension> extension_primary =
      InstallExtensionForProfile(
          profile(),
          test_data_dir_.AppendASCII("preference/lacros_secondary_profile"));

  scoped_refptr<const extensions::Extension> extension_secondary =
      InstallExtensionForProfile(
          &secondary_profile,
          test_data_dir_.AppendASCII("preference/lacros_secondary_profile"));

  ExpectThatProxyIsControlledByExtension(&secondary_profile);
  ExpectThatProxyIsControlledByExtension(profile());

  // Uninstall the extension in the secondary profile and test that Ash is still
  // returning the pref set by the extension running in the Lacros primary
  // profile.
  {
    extensions::TestExtensionRegistryObserver observer(
        extensions::ExtensionRegistry::Get(&secondary_profile),
        extension_secondary->id());
    auto* service_ = extensions::ExtensionSystem::Get(&secondary_profile)
                         ->extension_service();
    service_->UninstallExtension(extension_secondary->id(),
                                 extensions::UNINSTALL_REASON_FOR_TESTING,
                                 NULL);
    observer.WaitForExtensionUninstalled();
  }

  ExpectThatProxyHasDefaultValue(&secondary_profile);
  ExpectThatProxyIsControlledByExtension(profile());
  EXPECT_EQ(GetAshProxyPrefValue(), ProxyConfigDictionary::CreateDirect());

  // Uninstall the extension in the primary profile.
  {
    extensions::TestExtensionRegistryObserver observer(
        extensions::ExtensionRegistry::Get(profile()), extension_primary->id());
    auto* service_ =
        extensions::ExtensionSystem::Get(profile())->extension_service();
    service_->UninstallExtension(extension_primary->id(),
                                 extensions::UNINSTALL_REASON_FOR_TESTING,
                                 NULL);
    observer.WaitForExtensionUninstalled();
  }
  EXPECT_EQ(GetAshProxyPrefValue(), ProxyConfigDictionary::CreateSystem());
}

// An implementation of the `crosapi::mojom::Prefs` mojo service which returns
// null when fetching a pref value. Used for testing the Preference API against
// Ash-Lacros version skew where Ash does not recognize the Lacros extension
// pref. Since it's not possible to extend the PrefPath enum at runtime, this
// class implements the same behaviour like the Ash implementation when it does
// not recognize the pref i.e, sends a null value as a response to
// `GetExtensionPrefWithControl`.
class FakePrefsAshService : public crosapi::mojom::Prefs {
 public:
  FakePrefsAshService() = default;
  FakePrefsAshService(const FakePrefsAshService&) = delete;
  FakePrefsAshService& operator=(const FakePrefsAshService&) = delete;
  ~FakePrefsAshService() override {}

 private:
  // crosapi::mojom::Prefs:
  void GetPref(crosapi::mojom::PrefPath path,
               GetPrefCallback callback) override {
    std::move(callback).Run(absl::nullopt);
  }
  void SetPref(crosapi::mojom::PrefPath path,
               base::Value value,
               SetPrefCallback callback) override {
    std::move(callback).Run();
  }
  void AddObserver(
      crosapi::mojom::PrefPath path,
      mojo::PendingRemote<crosapi::mojom::PrefObserver> observer) override {
    mojo::Remote<crosapi::mojom::PrefObserver> remote(std::move(observer));
    observers_.Add(std::move(remote));
  }
  void GetExtensionPrefWithControl(
      crosapi::mojom::PrefPath path,
      GetExtensionPrefWithControlCallback callback) override {
    // Not a valid prefpath
    std::move(callback).Run(absl::nullopt,
                            crosapi::mojom::PrefControlState::kDefaultUnknown);
  }
  void ClearExtensionControlledPref(
      crosapi::mojom::PrefPath path,
      ClearExtensionControlledPrefCallback callback) override {
    std::move(callback).Run();
  }

  mojo::RemoteSet<crosapi::mojom::PrefObserver> observers_;
};

class ExtensionPreferenceApiUnsupportedInAshBrowserTest
    : public ExtensionPreferenceApiLacrosBrowserTest {
 public:
  ExtensionPreferenceApiUnsupportedInAshBrowserTest(
      const ExtensionPreferenceApiUnsupportedInAshBrowserTest&) = delete;
  ExtensionPreferenceApiUnsupportedInAshBrowserTest& operator=(
      const ExtensionPreferenceApiUnsupportedInAshBrowserTest&) = delete;

 protected:
  ExtensionPreferenceApiUnsupportedInAshBrowserTest() = default;
  ~ExtensionPreferenceApiUnsupportedInAshBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    // If the lacros service or the network settings service interface are not
    // available on this version of ash-chrome, this test suite will no-op.
    if (!IsServiceAvailable()) {
      GTEST_SKIP() << "The Lacros service is not available.";
    }
    // Replace the production prefs service with a fake for testing.
    mojo::Remote<crosapi::mojom::Prefs>& remote =
        chromeos::LacrosService::Get()->GetRemote<crosapi::mojom::Prefs>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }

  FakePrefsAshService service_;
  mojo::Receiver<crosapi::mojom::Prefs> receiver_{&service_};
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionPreferenceApiUnsupportedInAshBrowserTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionPreferenceApiUnsupportedInAshBrowserTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Tests that verifies that an error message is returned when an extension is
// requesting the value of a pref that should be controlled in Ash but it's not
// supported in the Ash version due to the Ash-Lacros version skew.
IN_PROC_BROWSER_TEST_P(ExtensionPreferenceApiUnsupportedInAshBrowserTest,
                       UnsupportedInAsh) {
  EXPECT_TRUE(RunExtensionTest("preference/unsupported_in_ash", {},
                               {.allow_in_incognito = false}))
      << message_;
}
