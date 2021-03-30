// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_installer_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

namespace extensions {

namespace {

const char kExtensionName[] = "InstallerExtension";
const char kWebstoreDomain[] = "cws.com";
const char kAppDomain[] = "app.com";
const char kNonAppDomain[] = "nonapp.com";
const char kTestExtensionId[] = "ecglahbcnmdpdciemllbhojghbkagdje";
const char kTestDataPath[] = "extensions/api_test/webstore_inline_install";
const char kCrxFilename[] = "extension.crx";

}  // namespace

// Test version of WebstoreInstaller that intercepts the destructor.
class TestWebstoreInstaller : public WebstoreInstaller {
 public:
  TestWebstoreInstaller(Profile* profile,
                        Delegate* delegate,
                        content::WebContents* web_contents,
                        const std::string& id,
                        std::unique_ptr<Approval> approval,
                        InstallSource source)
      : WebstoreInstaller(profile,
                          delegate,
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

class WebstoreInstallerBrowserTest
    : public WebstoreInstallerTest,
      public WebstoreInstaller::Delegate {
 public:
  WebstoreInstallerBrowserTest()
      : WebstoreInstallerTest(
            kWebstoreDomain,
            kTestDataPath,
            kCrxFilename,
            kAppDomain,
            kNonAppDomain) {}
  ~WebstoreInstallerBrowserTest() override {}

  void SetDoneClosure(base::OnceClosure done_closure) {
    done_closure_ = std::move(done_closure);
  }

  bool success() const { return success_; }

  // Overridden from WebstoreInstaller::Delegate:
  void OnExtensionDownloadStarted(const std::string& id,
                                  download::DownloadItem* item) override;
  void OnExtensionDownloadProgress(const std::string& id,
                                   download::DownloadItem* item) override;
  void OnExtensionInstallSuccess(const std::string& id) override;
  void OnExtensionInstallFailure(
      const std::string& id,
      const std::string& error,
      WebstoreInstaller::FailureReason reason) override;

 private:
  base::OnceClosure done_closure_;
  bool success_;
};

void WebstoreInstallerBrowserTest::OnExtensionDownloadStarted(
    const std::string& id,
    download::DownloadItem* item) {}

void WebstoreInstallerBrowserTest::OnExtensionDownloadProgress(
    const std::string& id,
    download::DownloadItem* item) {}

void WebstoreInstallerBrowserTest::OnExtensionInstallSuccess(
    const std::string& id) {
  success_ = true;
  std::move(done_closure_).Run();
}

void WebstoreInstallerBrowserTest::OnExtensionInstallFailure(
    const std::string& id,
    const std::string& error,
    WebstoreInstaller::FailureReason reason) {
  success_ = false;
  std::move(done_closure_).Run();
}

IN_PROC_BROWSER_TEST_F(WebstoreInstallerBrowserTest, WebstoreInstall) {
  std::unique_ptr<base::DictionaryValue> manifest(
      DictionaryBuilder()
          .Set("name", kExtensionName)
          .Set("description", "Foo")
          .Set("manifest_version", 2)
          .Set("version", "1.0")
          .Set("permissions", ListBuilder().Append("tabs").Build())
          .Build());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // Create an approval.
  std::unique_ptr<WebstoreInstaller::Approval> approval =
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          browser()->profile(), kTestExtensionId, std::move(manifest), false);

  // Create and run a WebstoreInstaller.
  base::RunLoop run_loop;
  SetDoneClosure(run_loop.QuitClosure());
  TestWebstoreInstaller* installer = new TestWebstoreInstaller(
      browser()->profile(), this, active_web_contents, kTestExtensionId,
      std::move(approval), WebstoreInstaller::INSTALL_SOURCE_OTHER);
  installer->Start();
  run_loop.Run();

  EXPECT_TRUE(success());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(kTestExtensionId));
}

IN_PROC_BROWSER_TEST_F(WebstoreInstallerBrowserTest, SimultaneousInstall) {
  std::unique_ptr<base::DictionaryValue> manifest(
      DictionaryBuilder()
          .Set("name", kExtensionName)
          .Set("description", "Foo")
          .Set("manifest_version", 2)
          .Set("version", "1.0")
          .Set("permissions", ListBuilder().Append("tabs").Build())
          .Build());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // Create an approval.
  std::unique_ptr<WebstoreInstaller::Approval> approval =
      WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          browser()->profile(), kTestExtensionId,
          std::unique_ptr<base::DictionaryValue>(manifest->DeepCopy()), false);

  // Create and run a WebstoreInstaller.
  base::RunLoop run_loop;
  SetDoneClosure(run_loop.QuitClosure());
  scoped_refptr<TestWebstoreInstaller> installer = new TestWebstoreInstaller(
      browser()->profile(), this, active_web_contents, kTestExtensionId,
      std::move(approval), WebstoreInstaller::INSTALL_SOURCE_OTHER);
  installer->Start();

  // Simulate another mechanism installing the same extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetID(kTestExtensionId)
          .SetManifest(std::move(manifest))
          .Build();
  extension_service()->OnExtensionInstalled(extension.get(),
                                            syncer::StringOrdinal(),
                                            0);

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

}  // namespace extensions
