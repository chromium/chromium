// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_background_page_first_load_observer.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::ExtensionService;
using extensions::Manifest;
using policy::PolicyMap;
using testing::_;
using testing::Return;

namespace {

std::string BuildForceInstallPolicyValue(const char* extension_id,
                                         const char* update_url) {
  return base::StringPrintf("%s;%s", extension_id, update_url);
}

// Observes destruction of an extension's ExtensionHost if it is currently
// there.
class ExtensionHostDestructionObserver
    : public extensions::ExtensionHostObserver {
 public:
  ExtensionHostDestructionObserver(Profile* profile,
                                   const extensions::ExtensionId& extension_id)
      : profile_(profile),
        extension_id_(extension_id),
        host_(extensions::ProcessManager::Get(profile)
                  ->GetBackgroundHostForExtension(extension_id_)),
        extension_host_observer_(this) {
    DCHECK(host_);
    extension_host_observer_.Add(host_);
  }

  void WaitForDestructionThenWaitForFirstLoad() {
    run_loop_.Run();

    extensions::TestBackgroundPageFirstLoadObserver first_load_observer(
        profile_, extension_id_);
    first_load_observer.Wait();
  }

  // ExtensionHostObserver:
  void OnExtensionHostDestroyed(
      const extensions::ExtensionHost* host) override {
    if (host == host_) {
      extension_host_observer_.Remove(host_);
      run_loop_.Quit();
    }
  }

 private:
  Profile* const profile_ = nullptr;
  const extensions::ExtensionId extension_id_;
  extensions::ExtensionHost* const host_ = nullptr;
  base::RunLoop run_loop_;
  ScopedObserver<extensions::ExtensionHost, extensions::ExtensionHostObserver>
      extension_host_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHostDestructionObserver);
};

}  // namespace

class ExtensionManagementTest : public extensions::ExtensionBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

 protected:
  void UpdateProviderPolicy(const PolicyMap& policy) {
    policy_provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

  // Helper method that returns whether the extension is at the given version.
  // This calls version(), which must be defined in the extension's bg page,
  // as well as asking the extension itself.
  //
  // Note that 'version' here means something different than the version field
  // in the extension's manifest. We use the version as reported by the
  // background page to test how overinstalling crx files with the same
  // manifest version works.
  bool IsExtensionAtVersion(const Extension* extension,
                            const std::string& expected_version) {
    // Test that the extension's version from the manifest and reported by the
    // background page is correct.  This is to ensure that the processes are in
    // sync with the Extension.
    extensions::ProcessManager* manager =
        extensions::ProcessManager::Get(browser()->profile());
    extensions::ExtensionHost* ext_host =
        manager->GetBackgroundHostForExtension(extension->id());
    EXPECT_TRUE(ext_host);
    if (!ext_host)
      return false;

    std::string version_from_bg;
    bool exec = content::ExecuteScriptAndExtractString(
        ext_host->host_contents(), "version()", &version_from_bg);
    EXPECT_TRUE(exec);
    if (!exec)
      return false;

    if (version_from_bg != expected_version ||
        extension->VersionString() != expected_version)
      return false;
    return true;
  }

 private:
  policy::MockConfigurationPolicyProvider policy_provider_;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass_;
};

// Tests that installing the same version overwrites.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallSameVersion) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath root_path = test_data_dir_.AppendASCII("install");
  base::FilePath pem_path = root_path.AppendASCII("install.pem");
  base::FilePath first_path =
      PackExtensionWithOptions(root_path.AppendASCII("install"),
                               temp_dir.GetPath().AppendASCII("install.crx"),
                               pem_path, base::FilePath());
  base::FilePath second_path = PackExtensionWithOptions(
      root_path.AppendASCII("install_same_version"),
      temp_dir.GetPath().AppendASCII("install_same_version.crx"), pem_path,
      base::FilePath());

  const Extension* extension = InstallExtension(first_path, 1);
  ASSERT_TRUE(extension);
  base::FilePath old_path = extension->path();

  const extensions::ExtensionId extension_id = extension->id();
  {
    ExtensionHostDestructionObserver host_destruction_observer(profile(),
                                                               extension_id);

    // Install an extension with the same version. The previous install should
    // be overwritten.
    extension = InstallExtension(second_path, 0);
    ASSERT_TRUE(extension);

    // Wait for the old ExtensionHost destruction first before waiting for the
    // new one to load.
    // Note that this is needed to ensure that |IsExtensionAtVersion| below can
    // successfully execute JS, otherwise this test becomes flaky.
    host_destruction_observer.WaitForDestructionThenWaitForFirstLoad();
  }
  base::FilePath new_path = extension->path();

  EXPECT_FALSE(IsExtensionAtVersion(extension, "1.0"));
  EXPECT_NE(old_path.value(), new_path.value());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallOlderVersion) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath root_path = test_data_dir_.AppendASCII("install");
  base::FilePath pem_path = root_path.AppendASCII("install.pem");
  base::FilePath modern_path =
      PackExtensionWithOptions(root_path.AppendASCII("install"),
                               temp_dir.GetPath().AppendASCII("install.crx"),
                               pem_path, base::FilePath());
  base::FilePath older_path = PackExtensionWithOptions(
      root_path.AppendASCII("install_older_version"),
      temp_dir.GetPath().AppendASCII("install_older_version.crx"), pem_path,
      base::FilePath());

  const Extension* extension = InstallExtension(modern_path, 1);
  ASSERT_TRUE(extension);
  ASSERT_FALSE(InstallExtension(older_path, 0));
  EXPECT_TRUE(IsExtensionAtVersion(extension, "1.0"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, InstallThenCancel) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath root_path = test_data_dir_.AppendASCII("install");
  base::FilePath pem_path = root_path.AppendASCII("install.pem");
  base::FilePath v1_path =
      PackExtensionWithOptions(root_path.AppendASCII("install"),
                               temp_dir.GetPath().AppendASCII("install.crx"),
                               pem_path, base::FilePath());
  base::FilePath v2_path =
      PackExtensionWithOptions(root_path.AppendASCII("install_v2"),
                               temp_dir.GetPath().AppendASCII("install_v2.crx"),
                               pem_path, base::FilePath());

  const Extension* extension = InstallExtension(v1_path, 1);
  ASSERT_TRUE(extension);

  // Cancel this install.
  ASSERT_FALSE(StartInstallButCancel(v2_path));
  EXPECT_TRUE(IsExtensionAtVersion(extension, "1.0"));
}

#if defined(OS_WIN)
// http://crbug.com/141913
#define MAYBE_InstallRequiresConfirm DISABLED_InstallRequiresConfirm
#else
#define MAYBE_InstallRequiresConfirm InstallRequiresConfirm
#endif
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, MAYBE_InstallRequiresConfirm) {
  // Installing the extension without an auto confirming UI should result in
  // it being disabled, since good.crx has permissions that require approval.
  std::string id = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
  ASSERT_FALSE(InstallExtension(test_data_dir_.AppendASCII("good.crx"), 0));
  ASSERT_TRUE(extension_registry()->disabled_extensions().GetByID(id));
  UninstallExtension(id);

  // And the install should succeed when the permissions are accepted.
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(
      test_data_dir_.AppendASCII("good.crx"), 1, browser()));
  UninstallExtension(id);
}

// Tests that disabling and re-enabling an extension works.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, DisableEnable) {
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  const size_t size_before = registry->enabled_extensions().size();

  // Load an extension, expect the background page to be available.
  std::string extension_id = "bjafgdebaacbbbecmhlhpofkepfkgcpa";
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII(extension_id)
                    .AppendASCII("1.0")));
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());
  EXPECT_TRUE(manager->GetBackgroundHostForExtension(extension_id));

  // After disabling, the background page should go away.
  DisableExtension(extension_id);
  EXPECT_EQ(size_before, registry->enabled_extensions().size());
  EXPECT_EQ(1u, registry->disabled_extensions().size());
  EXPECT_FALSE(manager->GetBackgroundHostForExtension(extension_id));

  // And bring it back.
  EnableExtension(extension_id);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());
  EXPECT_TRUE(manager->GetBackgroundHostForExtension(extension_id));
}

// Used for testing notifications sent during extension updates.
class NotificationListener : public content::NotificationObserver {
 public:
  NotificationListener() : started_(false), finished_(false) {
    int types[] = {extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED,
                   extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND};
    for (size_t i = 0; i < base::size(types); i++) {
      registrar_.Add(
          this, types[i], content::NotificationService::AllSources());
    }
  }
  ~NotificationListener() override {}

  bool started() { return started_; }

  bool finished() { return finished_; }

  const std::set<std::string>& updates() { return updates_; }

  void Reset() {
    started_ = false;
    finished_ = false;
    updates_.clear();
  }

  // Implements content::NotificationObserver interface.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED: {
        EXPECT_FALSE(started_);
        started_ = true;
        break;
      }
      case extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND: {
        const std::string& id =
            content::Details<extensions::UpdateDetails>(details)->id;
        updates_.insert(id);
        break;
      }
      default:
        NOTREACHED();
    }
  }

  void OnFinished() {
    EXPECT_FALSE(finished_);
    finished_ = true;
  }

 private:
  content::NotificationRegistrar registrar_;

  // Did we see EXTENSION_UPDATING_STARTED?
  bool started_;

  // Did we see EXTENSION_UPDATING_FINISHED?
  bool finished_;

  // The set of extension id's we've seen via EXTENSION_UPDATE_FOUND.
  std::set<std::string> updates_;
};

#if defined(OS_WIN)
// Fails consistently on Windows XP, see: http://crbug.com/120640.
#define MAYBE_AutoUpdate DISABLED_AutoUpdate
#else
// See http://crbug.com/103371 and http://crbug.com/120640.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_AutoUpdate DISABLED_AutoUpdate
#else
#define MAYBE_AutoUpdate AutoUpdate
#endif
#endif

// Tests extension autoupdate.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, MAYBE_AutoUpdate) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath basedir = test_data_dir_.AppendASCII("autoupdate");
  NotificationListener notification_listener;

  base::FilePath pem_path = basedir.AppendASCII("key.pem");
  base::FilePath v1_path = PackExtensionWithOptions(
      basedir.AppendASCII("v1"), temp_dir.GetPath().AppendASCII("v1.crx"),
      pem_path, base::FilePath());
  base::FilePath v2_path = PackExtensionWithOptions(
      basedir.AppendASCII("v2"), temp_dir.GetPath().AppendASCII("v2.crx"),
      pem_path, base::FilePath());

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.path() == "/autoupdate/v2.crx") {
          content::URLLoaderInterceptor::WriteResponse(v2_path,
                                                       params->client.get());
          return true;
        }
        if (params->url_request.url.path() == "/autoupdate/v3.crx") {
          content::URLLoaderInterceptor::WriteResponse(
              basedir.AppendASCII("v3.crx"), params->client.get());
          return true;
        }
        if (params->url_request.url.path() == "/autoupdate/manifest") {
          static bool first = true;
          if (first) {
            content::URLLoaderInterceptor::WriteResponse(
                basedir.AppendASCII("manifest_v2.xml"), params->client.get());
            first = false;
          } else {
            content::URLLoaderInterceptor::WriteResponse(
                basedir.AppendASCII("manifest_v3.xml"), params->client.get());
          }
          return true;
        }
        return false;
      }));

  // Install version 1 of the extension.
  ExtensionTestMessageListener listener1("v1 installed", false);
  ExtensionService* service = extension_service();
  ExtensionRegistry* registry = extension_registry();
  const size_t size_before = registry->enabled_extensions().size();
  ASSERT_TRUE(registry->disabled_extensions().is_empty());
  const Extension* extension = InstallExtension(v1_path, 1);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener1.WaitUntilSatisfied());
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf", extension->id());
  ASSERT_EQ("1.0", extension->VersionString());

  // Run autoupdate and make sure version 2 of the extension was installed.
  ExtensionTestMessageListener listener2("v2 installed", false);

  extensions::TestExtensionRegistryObserver install_observer(registry);
  extensions::ExtensionUpdater::CheckParams params1;
  params1.callback = base::BindOnce(&NotificationListener::OnFinished,
                                    base::Unretained(&notification_listener));
  service->updater()->CheckNow(std::move(params1));
  install_observer.WaitForExtensionWillBeInstalled();
  EXPECT_TRUE(listener2.WaitUntilSatisfied());
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  extension = registry->enabled_extensions().GetByID(
      "ogjcoiohnmldgjemafoockdghcjciccf");
  ASSERT_TRUE(extension);
  ASSERT_EQ("2.0", extension->VersionString());
  ASSERT_TRUE(notification_listener.started());
  ASSERT_TRUE(notification_listener.finished());
  ASSERT_TRUE(base::Contains(notification_listener.updates(),
                             "ogjcoiohnmldgjemafoockdghcjciccf"));
  notification_listener.Reset();

  // Now try doing an update to version 3, which has been incorrectly
  // signed. This should fail.

  extensions::ExtensionUpdater::CheckParams params2;
  params2.callback = base::BindOnce(&NotificationListener::OnFinished,
                                    base::Unretained(&notification_listener));
  service->updater()->CheckNow(std::move(params2));
  ASSERT_TRUE(WaitForExtensionInstallError());
  ASSERT_TRUE(notification_listener.started());
  ASSERT_TRUE(notification_listener.finished());
  ASSERT_TRUE(base::Contains(notification_listener.updates(),
                             "ogjcoiohnmldgjemafoockdghcjciccf"));

  // Make sure the extension state is the same as before.
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  extension = registry->enabled_extensions().GetByID(
      "ogjcoiohnmldgjemafoockdghcjciccf");
  ASSERT_TRUE(extension);
  ASSERT_EQ("2.0", extension->VersionString());
}

#if defined(OS_WIN)
// Fails consistently on Windows XP, see: http://crbug.com/120640.
#define MAYBE_AutoUpdateDisabledExtensions DISABLED_AutoUpdateDisabledExtensions
#else
#if defined(ADDRESS_SANITIZER)
#define MAYBE_AutoUpdateDisabledExtensions DISABLED_AutoUpdateDisabledExtensions
#else
#define MAYBE_AutoUpdateDisabledExtensions AutoUpdateDisabledExtensions
#endif
#endif

// Tests extension autoupdate.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest,
                       MAYBE_AutoUpdateDisabledExtensions) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath basedir = test_data_dir_.AppendASCII("autoupdate");
  NotificationListener notification_listener;

  base::FilePath pem_path = basedir.AppendASCII("key.pem");
  base::FilePath v1_path = PackExtensionWithOptions(
      basedir.AppendASCII("v1"), temp_dir.GetPath().AppendASCII("v1.crx"),
      pem_path, base::FilePath());
  base::FilePath v2_path = PackExtensionWithOptions(
      basedir.AppendASCII("v2"), temp_dir.GetPath().AppendASCII("v2.crx"),
      pem_path, base::FilePath());

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.path() == "/autoupdate/manifest") {
          content::URLLoaderInterceptor::WriteResponse(
              basedir.AppendASCII("manifest_v2.xml"), params->client.get());
          return true;
        }
        if (params->url_request.url.path() == "/autoupdate/v2.crx") {
          content::URLLoaderInterceptor::WriteResponse(v2_path,
                                                       params->client.get());
          return true;
        }
        return false;
      }));

  // Install version 1 of the extension.
  ExtensionTestMessageListener listener1("v1 installed", false);
  ExtensionService* service = extension_service();
  ExtensionRegistry* registry = extension_registry();
  const size_t enabled_size_before = registry->enabled_extensions().size();
  const size_t disabled_size_before = registry->disabled_extensions().size();
  const Extension* extension = InstallExtension(v1_path, 1);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener1.WaitUntilSatisfied());
  DisableExtension(extension->id());
  ASSERT_EQ(disabled_size_before + 1, registry->disabled_extensions().size());
  ASSERT_EQ(enabled_size_before, registry->enabled_extensions().size());
  ASSERT_EQ("ogjcoiohnmldgjemafoockdghcjciccf", extension->id());
  ASSERT_EQ("1.0", extension->VersionString());

  ExtensionTestMessageListener listener2("v2 installed", false);
  extensions::TestExtensionRegistryObserver install_observer(registry);
  // Run autoupdate and make sure version 2 of the extension was installed but
  // is still disabled.
  extensions::ExtensionUpdater::CheckParams params;
  params.callback = base::BindOnce(&NotificationListener::OnFinished,
                                   base::Unretained(&notification_listener));
  service->updater()->CheckNow(std::move(params));
  install_observer.WaitForExtensionWillBeInstalled();
  ASSERT_EQ(disabled_size_before + 1, registry->disabled_extensions().size());
  ASSERT_EQ(enabled_size_before, registry->enabled_extensions().size());
  extension = registry->disabled_extensions().GetByID(
      "ogjcoiohnmldgjemafoockdghcjciccf");
  ASSERT_TRUE(extension);
  ASSERT_FALSE(registry->enabled_extensions().GetByID(
      "ogjcoiohnmldgjemafoockdghcjciccf"));
  ASSERT_EQ("2.0", extension->VersionString());

  // The extension should have not made the callback because it is disabled.
  // When we enabled it, it should then make the callback.
  ASSERT_FALSE(listener2.was_satisfied());
  EnableExtension(extension->id());
  EXPECT_TRUE(listener2.WaitUntilSatisfied());
  ASSERT_TRUE(notification_listener.started());
  ASSERT_TRUE(notification_listener.finished());
  ASSERT_TRUE(base::Contains(notification_listener.updates(),
                             "ogjcoiohnmldgjemafoockdghcjciccf"));
  notification_listener.Reset();
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, ExternalUrlUpdate) {
  ExtensionService* service = extension_service();
  const char kExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath basedir = test_data_dir_.AppendASCII("autoupdate");
  base::FilePath pem_path = basedir.AppendASCII("key.pem");
  base::FilePath v2_path = PackExtensionWithOptions(
      basedir.AppendASCII("v2"), temp_dir.GetPath().AppendASCII("v2.crx"),
      pem_path, base::FilePath());

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.path() == "/autoupdate/manifest") {
          content::URLLoaderInterceptor::WriteResponse(
              basedir.AppendASCII("manifest_v2.xml"), params->client.get());
          return true;
        }
        if (params->url_request.url.path() == "/autoupdate/v2.crx") {
          content::URLLoaderInterceptor::WriteResponse(v2_path,
                                                       params->client.get());
          return true;
        }
        return false;
      }));

  ExtensionRegistry* registry = extension_registry();
  const size_t size_before = registry->enabled_extensions().size();
  ASSERT_TRUE(registry->disabled_extensions().is_empty());

  extensions::PendingExtensionManager* pending_extension_manager =
      service->pending_extension_manager();

  // The code that reads external_extensions.json uses this method to inform
  // the extensions::ExtensionService of an extension to download.  Using the
  // real code is race-prone, because instantating the
  // extensions::ExtensionService starts a read of external_extensions.json
  // before this test function starts.

  EXPECT_TRUE(pending_extension_manager->AddFromExternalUpdateUrl(
      kExtensionId,
      std::string(),
      GURL("http://localhost/autoupdate/manifest"),
      Manifest::EXTERNAL_PREF_DOWNLOAD,
      Extension::NO_FLAGS,
      false));

  extensions::TestExtensionRegistryObserver install_observer(registry);
  // Run autoupdate and make sure version 2 of the extension was installed.
  service->updater()->CheckNow(extensions::ExtensionUpdater::CheckParams());
  install_observer.WaitForExtensionWillBeInstalled();
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  const Extension* extension =
      registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  ASSERT_EQ("2.0", extension->VersionString());

  // Uninstalling the extension should set a pref that keeps the extension from
  // being installed again the next time external_extensions.json is read.

  UninstallExtension(kExtensionId);

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser()->profile());
  EXPECT_TRUE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "Uninstalling should set kill bit on externaly installed extension.";

  // Try to install the extension again from an external source. It should fail
  // because of the killbit.
  EXPECT_FALSE(pending_extension_manager->AddFromExternalUpdateUrl(
      kExtensionId,
      std::string(),
      GURL("http://localhost/autoupdate/manifest"),
      Manifest::EXTERNAL_PREF_DOWNLOAD,
      Extension::NO_FLAGS,
      false));
  EXPECT_FALSE(pending_extension_manager->IsIdPending(kExtensionId))
      << "External reinstall of a killed extension shouldn't work.";
  EXPECT_TRUE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "External reinstall of a killed extension should leave it killed.";

  // Installing from non-external source.
  ASSERT_TRUE(InstallExtension(v2_path, 1));

  EXPECT_FALSE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "Reinstalling should clear the kill bit.";

  // Uninstalling from a non-external source should not set the kill bit.
  UninstallExtension(kExtensionId);

  EXPECT_FALSE(extension_prefs->IsExternalExtensionUninstalled(kExtensionId))
      << "Uninstalling non-external extension should not set kill bit.";
}

namespace {

const char kForceInstallNotEmptyHelp[] =
    "A policy may already be controlling the list of force-installed "
    "extensions. Please remove all policy settings from your computer "
    "before running tests. E.g. from /etc/chromium/policies Linux or "
    "from the registry on Windows, etc.";

}

// See http://crbug.com/57378 for flakiness details.
IN_PROC_BROWSER_TEST_F(ExtensionManagementTest, ExternalPolicyRefresh) {
  const char kExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath basedir = test_data_dir_.AppendASCII("autoupdate");
  base::FilePath pem_path = basedir.AppendASCII("key.pem");
  base::FilePath v2_path = PackExtensionWithOptions(
      basedir.AppendASCII("v2"), temp_dir.GetPath().AppendASCII("v2.crx"),
      pem_path, base::FilePath());

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.path() == "/autoupdate/manifest") {
          content::URLLoaderInterceptor::WriteResponse(
              basedir.AppendASCII("manifest_v2.xml"), params->client.get());
          return true;
        }
        if (params->url_request.url.path() == "/autoupdate/v2.crx") {
          content::URLLoaderInterceptor::WriteResponse(v2_path,
                                                       params->client.get());
          return true;
        }
        return false;
      }));

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  const size_t size_before = registry->enabled_extensions().size();
  ASSERT_TRUE(registry->disabled_extensions().is_empty());

  ASSERT_TRUE(extensions::ExtensionManagementFactory::GetForBrowserContext(
                  browser()->profile())
                  ->GetForceInstallList()
                  ->empty())
      << kForceInstallNotEmptyHelp;

  base::ListValue forcelist;
  forcelist.AppendString(BuildForceInstallPolicyValue(
      kExtensionId, "http://localhost/autoupdate/manifest"));
  PolicyMap policies;
  policies.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, forcelist.CreateDeepCopy(),
               nullptr);
  extensions::TestExtensionRegistryObserver install_observer(registry);
  UpdateProviderPolicy(policies);
  install_observer.WaitForExtensionWillBeInstalled();

  // Check if the extension got installed.
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  const Extension* extension =
      registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  ASSERT_EQ("2.0", extension->VersionString());
  EXPECT_EQ(Manifest::EXTERNAL_POLICY_DOWNLOAD, extension->location());

  // Try to disable and uninstall the extension which should fail.
  DisableExtension(kExtensionId);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());
  UninstallExtension(kExtensionId);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());

  // Now try to disable it through the management api, again failing.
  ExtensionTestMessageListener listener1("ready", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/uninstall_extension")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  EXPECT_EQ(size_before + 2, registry->enabled_extensions().size());
  EXPECT_EQ(0u, registry->disabled_extensions().size());

  // Check that emptying the list triggers uninstall.
  policies.Erase(policy::key::kExtensionInstallForcelist);
  UpdateProviderPolicy(policies);
  EXPECT_EQ(size_before + 1, registry->enabled_extensions().size());
  EXPECT_FALSE(
      registry->GetExtensionById(kExtensionId, ExtensionRegistry::EVERYTHING));
}

// See http://crbug.com/103371 and http://crbug.com/120640.
#if defined(ADDRESS_SANITIZER) || defined(OS_WIN)
#define MAYBE_PolicyOverridesUserInstall DISABLED_PolicyOverridesUserInstall
#else
#define MAYBE_PolicyOverridesUserInstall PolicyOverridesUserInstall
#endif

IN_PROC_BROWSER_TEST_F(ExtensionManagementTest,
                       MAYBE_PolicyOverridesUserInstall) {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser()->profile());
  const char kExtensionId[] = "ogjcoiohnmldgjemafoockdghcjciccf";
  const size_t size_before = registry->enabled_extensions().size();
  base::FilePath basedir = test_data_dir_.AppendASCII("autoupdate");
  ASSERT_TRUE(registry->disabled_extensions().is_empty());

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath pem_path = basedir.AppendASCII("key.pem");
  base::FilePath v2_path = PackExtensionWithOptions(
      basedir.AppendASCII("v2"), temp_dir.GetPath().AppendASCII("v2.crx"),
      pem_path, base::FilePath());

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.path() == "/autoupdate/manifest") {
          content::URLLoaderInterceptor::WriteResponse(
              basedir.AppendASCII("manifest_v2.xml"), params->client.get());
          return true;
        }
        if (params->url_request.url.path() == "/autoupdate/v2.crx") {
          content::URLLoaderInterceptor::WriteResponse(v2_path,
                                                       params->client.get());
          return true;
        }
        return false;
      }));

  // Check that the policy is initially empty.
  ASSERT_TRUE(extensions::ExtensionManagementFactory::GetForBrowserContext(
                  browser()->profile())
                  ->GetForceInstallList()
                  ->empty())
      << kForceInstallNotEmptyHelp;

  // User install of the extension.
  ASSERT_TRUE(InstallExtension(v2_path, 1));
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  const Extension* extension =
      registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_EQ(Manifest::INTERNAL, extension->location());
  EXPECT_TRUE(service->IsExtensionEnabled(kExtensionId));

  // Setup the force install policy. It should override the location.
  base::ListValue forcelist;
  forcelist.AppendString(BuildForceInstallPolicyValue(
      kExtensionId, "http://localhost/autoupdate/manifest"));
  PolicyMap policies;
  policies.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, forcelist.CreateDeepCopy(),
               nullptr);
  extensions::TestExtensionRegistryObserver install_observer(registry);
  UpdateProviderPolicy(policies);
  install_observer.WaitForExtensionWillBeInstalled();

  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  extension = registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_EQ(Manifest::EXTERNAL_POLICY_DOWNLOAD, extension->location());
  EXPECT_TRUE(service->IsExtensionEnabled(kExtensionId));

  // Remove the policy, and verify that the extension was uninstalled.
  // TODO(joaodasilva): it would be nicer if the extension was kept instead,
  // and reverted location to INTERNAL or whatever it was before the policy
  // was applied.
  policies.Erase(policy::key::kExtensionInstallForcelist);
  UpdateProviderPolicy(policies);
  ASSERT_EQ(size_before, registry->enabled_extensions().size());
  extension =
      registry->GetExtensionById(kExtensionId, ExtensionRegistry::EVERYTHING);
  EXPECT_FALSE(extension);

  // User install again, but have it disabled too before setting the policy.
  ASSERT_TRUE(InstallExtension(v2_path, 1));
  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  extension = registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_EQ(Manifest::INTERNAL, extension->location());
  EXPECT_TRUE(service->IsExtensionEnabled(kExtensionId));
  EXPECT_TRUE(registry->disabled_extensions().is_empty());

  DisableExtension(kExtensionId);
  EXPECT_EQ(1u, registry->disabled_extensions().size());
  extension = registry->disabled_extensions().GetByID(kExtensionId);
  EXPECT_TRUE(extension);
  EXPECT_FALSE(service->IsExtensionEnabled(kExtensionId));

  // Install the policy again. It should overwrite the extension's location,
  // and force enable it too.
  policies.Set(policy::key::kExtensionInstallForcelist,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, forcelist.CreateDeepCopy(),
               nullptr);

  extensions::TestExtensionRegistryObserver extension_observer(registry);
  UpdateProviderPolicy(policies);
  extension_observer.WaitForExtensionWillBeInstalled();

  ASSERT_EQ(size_before + 1, registry->enabled_extensions().size());
  extension = registry->enabled_extensions().GetByID(kExtensionId);
  ASSERT_TRUE(extension);
  EXPECT_EQ(Manifest::EXTERNAL_POLICY_DOWNLOAD, extension->location());
  EXPECT_TRUE(service->IsExtensionEnabled(kExtensionId));
  EXPECT_TRUE(registry->disabled_extensions().is_empty());
}
