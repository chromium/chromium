// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_installer.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/webstore_installer_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

const char kWebstoreDomain[] = "cws.com";
const char kAppDomain[] = "app.com";
const char kNonAppDomain[] = "nonapp.com";
const char kTestExtensionId[] = "ecglahbcnmdpdciemllbhojghbkagdje";
const char kTestExtensionWithPermissionsId[] =
    "lpbboafeefjeccjhdhcfdibnjcecpmhd";
const char kTestDataPath[] = "extensions/api_test/webstore_inline_install";
const char kCrxFilename[] = "extension.crx";
const char kCrxWithPermissionsFilename[] =
    "extension_with_host_permissions.crx";

}  // namespace

// Test version of WebstoreInstaller that intercepts the destructor.
class TestWebstoreInstaller : public WebstoreInstaller {
 public:
  TestWebstoreInstaller(Profile* profile,
                        SuccessCallback success_callback,
                        FailureCallback failure_callback,
                        content::WebContents* web_contents,
                        const std::string& id,
                        std::unique_ptr<Approval> approval,
                        InstallSource source)
      : WebstoreInstaller(profile,
                          std::move(success_callback),
                          std::move(failure_callback),
                          web_contents,
                          id,
                          std::move(approval),
                          source) {}

  void SetDeletedClosure(base::OnceClosure cb) {
    deleted_closure_ = std::move(cb);
  }

 private:
  ~TestWebstoreInstaller() override {
    if (!deleted_closure_.is_null())
      std::move(deleted_closure_).Run();
  }

  base::OnceClosure deleted_closure_;
};

class WebstoreInstallerBrowserTest : public WebstoreInstallerTest {
 public:
  WebstoreInstallerBrowserTest(const std::string& webstore_domain,
                               const std::string& test_data_path,
                               const std::string& crx_filename,
                               const std::string& verified_domain,
                               const std::string& unverified_domain)
      : WebstoreInstallerTest(webstore_domain,
                              test_data_path,
                              crx_filename,
                              verified_domain,
                              unverified_domain) {}
  ~WebstoreInstallerBrowserTest() override = default;

  void SetDoneClosure(base::OnceClosure done_closure) {
    done_closure_ = std::move(done_closure);
  }

  bool success() const { return success_; }

  void OnExtensionInstallSuccess(const std::string& id) {
    success_ = true;
    std::move(done_closure_).Run();
  }

  void OnExtensionInstallFailure(const std::string& id,
                                 const std::string& error,
                                 WebstoreInstaller::FailureReason reason) {
    success_ = false;
    std::move(done_closure_).Run();
  }

 private:
  base::OnceClosure done_closure_;
  bool success_;
};

class WebstoreInstallerMV2BrowserTest : public WebstoreInstallerBrowserTest {
 public:
  WebstoreInstallerMV2BrowserTest()
      : WebstoreInstallerBrowserTest(kWebstoreDomain,
                                     kTestDataPath,
                                     kCrxFilename,
                                     kAppDomain,
                                     kNonAppDomain) {}
  ~WebstoreInstallerMV2BrowserTest() override = default;

  // The manifest used by the test installer must match `kCrxFilename` manifest
  // in the test directory.
  base::Value::Dict GetManifest() {
    return base::Value::Dict()
        .Set("name", "Installer Extension")
        .Set("manifest_version", 2)
        .Set("version", "1.0")
        .Set("permissions", base::Value::List().Append("tabs"));
  }
};

IN_PROC_BROWSER_TEST_F(WebstoreInstallerMV2BrowserTest, WebstoreInstall) {
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // Create an approval.
  std::unique_ptr<WebstoreInstaller::Approval> approval =
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          browser()->profile(), kTestExtensionId, GetManifest(), false);

  // Create and run a WebstoreInstaller.
  base::RunLoop run_loop;
  SetDoneClosure(run_loop.QuitClosure());
  TestWebstoreInstaller* installer = new TestWebstoreInstaller(
      browser()->profile(),
      base::BindOnce(&WebstoreInstallerBrowserTest::OnExtensionInstallSuccess,
                     base::Unretained(this)),
      base::BindOnce(&WebstoreInstallerBrowserTest::OnExtensionInstallFailure,
                     base::Unretained(this)),
      active_web_contents, kTestExtensionId, std::move(approval),
      WebstoreInstaller::INSTALL_SOURCE_OTHER);
  installer->Start();
  run_loop.Run();

  EXPECT_TRUE(success());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(kTestExtensionId));
}

IN_PROC_BROWSER_TEST_F(WebstoreInstallerMV2BrowserTest, SimultaneousInstall) {
  base::Value::Dict manifest = GetManifest();

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // Create an approval.
  std::unique_ptr<WebstoreInstaller::Approval> approval =
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          browser()->profile(), kTestExtensionId, manifest.Clone(), false);

  // Create and run a WebstoreInstaller.
  base::RunLoop run_loop;
  SetDoneClosure(run_loop.QuitClosure());
  scoped_refptr<TestWebstoreInstaller> installer = new TestWebstoreInstaller(
      browser()->profile(),
      base::BindOnce(&WebstoreInstallerBrowserTest::OnExtensionInstallSuccess,
                     base::Unretained(this)),
      base::BindOnce(&WebstoreInstallerBrowserTest::OnExtensionInstallFailure,
                     base::Unretained(this)),
      active_web_contents, kTestExtensionId, std::move(approval),
      WebstoreInstaller::INSTALL_SOURCE_OTHER);
  installer->Start();

  // Simulate another mechanism installing the same extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetID(kTestExtensionId)
          .SetManifest(std::move(manifest))
          .Build();
  extension_service()->OnExtensionInstalled(extension.get(),
                                            syncer::StringOrdinal(), 0);

  run_loop.Run();

  // Wait for the WebstoreInstaller to be destroyed. Bad things happen if we
  // don't wait for this.
  base::RunLoop run_loop2;
  installer->SetDeletedClosure(run_loop2.QuitClosure());
  installer = nullptr;
  run_loop2.Run();

  EXPECT_TRUE(success());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  // Extension ends up as disabled because of permissions.
  ASSERT_TRUE(registry->disabled_extensions().GetByID(kTestExtensionId));
}

class WebstoreInstallerWithWithholdingUIBrowserTest
    : public WebstoreInstallerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  WebstoreInstallerWithWithholdingUIBrowserTest()
      : WebstoreInstallerBrowserTest(kWebstoreDomain,
                                     kTestDataPath,
                                     kCrxWithPermissionsFilename,
                                     kAppDomain,
                                     kNonAppDomain) {
    feature_list_.InitAndEnableFeature(
        extensions_features::kAllowWithholdingExtensionPermissionsOnInstall);
  }
  ~WebstoreInstallerWithWithholdingUIBrowserTest() override = default;

  // Th manifest used by the test installer must match
  // `kCrxWithPermissionsFilename` manifest in the test directory.
  base::Value::Dict GetManifest() {
    return base::Value::Dict()
        .Set("name", "Installer Extension")
        .Set("manifest_version", 3)
        .Set("version", "1.0")
        .Set("host_permissions", base::Value::List().Append("<all_urls>"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests host permissions are withheld at installation only when the checkbox is
// selected.
IN_PROC_BROWSER_TEST_P(WebstoreInstallerWithWithholdingUIBrowserTest,
                       WithholdingHostsOnInstall) {
  bool shoud_withhold_permissions = GetParam();

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // Create an approval that withhelds permissions when the checkbox is not
  // selected.
  std::unique_ptr<WebstoreInstaller::Approval> approval =
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          browser()->profile(), kTestExtensionWithPermissionsId, GetManifest(),
          false);
  approval->withhold_permissions = shoud_withhold_permissions;

  // Create and run a WebstoreInstaller.
  base::RunLoop run_loop;
  SetDoneClosure(run_loop.QuitClosure());
  TestWebstoreInstaller* installer = new TestWebstoreInstaller(
      browser()->profile(),
      base::BindOnce(&WebstoreInstallerBrowserTest::OnExtensionInstallSuccess,
                     base::Unretained(this)),
      base::BindOnce(&WebstoreInstallerBrowserTest::OnExtensionInstallFailure,
                     base::Unretained(this)),
      active_web_contents, kTestExtensionWithPermissionsId, std::move(approval),
      WebstoreInstaller::INSTALL_SOURCE_OTHER);
  installer->Start();
  run_loop.Run();

  // Verify extension was installed.
  EXPECT_TRUE(success());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  const Extension* extension =
      registry->enabled_extensions().GetByID(kTestExtensionWithPermissionsId);
  ASSERT_TRUE(extension);

  // Host permissions should be withheld only when the params indicate so.
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser()->profile());
  EXPECT_EQ(permissions_manager->HasWithheldHostPermissions(*extension),
            shoud_withhold_permissions);

  // Access to google.com should be withheld only when the params indicate so.
  const PermissionsManager::ExtensionSiteAccess site_access =
      permissions_manager->GetSiteAccess(*extension,
                                         GURL("https://www.google.com"));
  EXPECT_EQ(site_access.withheld_site_access, shoud_withhold_permissions);
  EXPECT_EQ(site_access.has_site_access, !shoud_withhold_permissions);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebstoreInstallerWithWithholdingUIBrowserTest,
                         testing::Bool());

}  // namespace extensions
