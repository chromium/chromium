// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_installer_test.h"
#include "chrome/browser/extensions/webstore_reinstaller.h"
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

const char kWebstoreDomain[] = "cws.com";
const char kAppDomain[] = "app.com";
const char kNonAppDomain[] = "nonapp.com";
const char kTestExtensionId[] = "ecglahbcnmdpdciemllbhojghbkagdje";
const char kTestDataPath[] = "extensions/api_test/webstore_inline_install";
const char kCrxFilename[] = "extension.crx";

}  // namespace

class WebstoreReinstallerBrowserTest : public WebstoreInstallerTest {
 public:
  WebstoreReinstallerBrowserTest()
      : WebstoreInstallerTest(
            kWebstoreDomain,
            kTestDataPath,
            kCrxFilename,
            kAppDomain,
            kNonAppDomain) {}
  ~WebstoreReinstallerBrowserTest() override {}

  void OnInstallCompletion(base::OnceClosure quit_closure,
                           bool success,
                           const std::string& error,
                           webstore_install::Result result);

  bool last_install_result() const { return last_install_result_; }

 private:
  bool last_install_result_;
};

void WebstoreReinstallerBrowserTest::OnInstallCompletion(
    base::OnceClosure quit_closure,
    bool success,
    const std::string& error,
    webstore_install::Result result) {
  last_install_result_ = success;
  std::move(quit_closure).Run();
}

IN_PROC_BROWSER_TEST_F(WebstoreReinstallerBrowserTest, TestWebstoreReinstall) {
  // Build an extension with the same id as our test extension and add it.
  const std::string kExtensionName("ReinstallerExtension");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetID(kTestExtensionId)
          .SetManifest(DictionaryBuilder()
                           .Set("name", kExtensionName)
                           .Set("description", "Foo")
                           .Set("manifest_version", 2)
                           .Set("version", "1.0")
                           .Build())
          .Build();
  extension_service()->AddExtension(extension.get());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(kTestExtensionId));

  // WebstoreReinstaller expects corrupted extension.
  extension_service()->DisableExtension(kTestExtensionId,
                                        disable_reason::DISABLE_CORRUPTED);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // Start by canceling the repair prompt.
  AutoCancelInstall();

  // Create and run a WebstoreReinstaller.
  base::RunLoop run_loop;
  scoped_refptr<WebstoreReinstaller> reinstaller(new WebstoreReinstaller(
      active_web_contents, kTestExtensionId,
      base::BindOnce(&WebstoreReinstallerBrowserTest::OnInstallCompletion,
                     base::Unretained(this), run_loop.QuitClosure())));
  reinstaller->BeginReinstall();
  run_loop.Run();

  // We should have failed, and the old extension should still be present.
  EXPECT_FALSE(last_install_result());
  extension = registry->disabled_extensions().GetByID(kTestExtensionId);
  ASSERT_TRUE(extension.get());
  EXPECT_EQ(kExtensionName, extension->name());

  // Now accept the repair prompt.
  AutoAcceptInstall();
  base::RunLoop run_loop2;
  reinstaller = new WebstoreReinstaller(
      active_web_contents, kTestExtensionId,
      base::BindOnce(&WebstoreReinstallerBrowserTest::OnInstallCompletion,
                     base::Unretained(this), run_loop2.QuitClosure()));
  reinstaller->BeginReinstall();
  run_loop2.Run();

  // The reinstall should have succeeded, and the extension should have been
  // "updated" (which in this case means that it should have been replaced with
  // the inline install test extension, since that's the id we used).
  EXPECT_TRUE(last_install_result());
  extension = registry->enabled_extensions().GetByID(kTestExtensionId);
  ASSERT_TRUE(extension.get());
  // The name should not match, since the extension changed.
  EXPECT_NE(kExtensionName, extension->name());
}

}  // namespace extensions
