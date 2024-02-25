// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TestFuture;

namespace extensions {

class ExtensionFunctionalTest : public ExtensionBrowserTest {
 public:
  void InstallExtensionSilently(ExtensionService* service,
                                const char* filename) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
    size_t num_before = registry->enabled_extensions().size();

    base::FilePath path = test_data_dir_.AppendASCII(filename);

    TestExtensionRegistryObserver extension_observer(registry);

    scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service));
    installer->set_is_gallery_install(false);
    installer->set_allow_silent_install(true);
    installer->set_install_source(mojom::ManifestLocation::kInternal);
    installer->set_off_store_install_allow_reason(
        CrxInstaller::OffStoreInstallAllowedInTest);

    TestFuture<std::optional<CrxInstallError>> installer_done_future;
    installer->AddInstallerCallback(
        installer_done_future
            .GetCallback<const std::optional<CrxInstallError>&>());
    installer->InstallCrx(path);

    const std::optional<CrxInstallError>& error = installer_done_future.Get();
    EXPECT_FALSE(error);

    size_t num_after = registry->enabled_extensions().size();
    EXPECT_EQ(num_before + 1, num_after);

    extension_observer.WaitForExtensionLoaded();
    const Extension* extension =
        registry->enabled_extensions().GetByID(last_loaded_extension_id());
    EXPECT_TRUE(extension);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest, TestSetExtensionsState) {
  InstallExtensionSilently(extension_service(), "google_talk.crx");

  // Disable the extension and verify.
  util::SetIsIncognitoEnabled(last_loaded_extension_id(), profile(), false);
  ExtensionService* service = extension_service();
  service->DisableExtension(last_loaded_extension_id(),
                            disable_reason::DISABLE_USER_ACTION);
  EXPECT_FALSE(service->IsExtensionEnabled(last_loaded_extension_id()));

  // Enable the extension and verify.
  util::SetIsIncognitoEnabled(last_loaded_extension_id(), profile(), false);
  service->EnableExtension(last_loaded_extension_id());
  EXPECT_TRUE(service->IsExtensionEnabled(last_loaded_extension_id()));

  // Allow extension in incognito mode and verify.
  service->EnableExtension(last_loaded_extension_id());
  util::SetIsIncognitoEnabled(last_loaded_extension_id(), profile(), true);
  EXPECT_TRUE(util::IsIncognitoEnabled(last_loaded_extension_id(), profile()));

  // Disallow extension in incognito mode and verify.
  service->EnableExtension(last_loaded_extension_id());
  util::SetIsIncognitoEnabled(last_loaded_extension_id(), profile(), false);
  EXPECT_FALSE(util::IsIncognitoEnabled(last_loaded_extension_id(), profile()));
}

IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest,
                       FindingUnrelatedExtensionFramesFromAboutBlank) {
  // Load an extension before adding tabs.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("simple_with_file"));
  ASSERT_TRUE(extension);
  GURL extension_url = extension->GetResourceURL("file.html");

  // Load the extension in two unrelated tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Sanity-check test setup: 2 frames share a renderer process, but are not in
  // a related browsing instance.
  content::RenderFrameHost* tab1 =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame();
  content::RenderFrameHost* tab2 =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetPrimaryMainFrame();
  EXPECT_EQ(tab1->GetProcess(), tab2->GetProcess());
  EXPECT_FALSE(
      tab1->GetSiteInstance()->IsRelatedSiteInstance(tab2->GetSiteInstance()));

  // Name the 2 frames.
  EXPECT_TRUE(content::ExecJs(tab1, "window.name = 'tab1';"));
  EXPECT_TRUE(content::ExecJs(tab2, "window.name = 'tab2';"));

  // Open a new window from tab1 and store it in tab1_popup.
  content::RenderFrameHost* tab1_popup = nullptr;
  {
    content::WebContentsAddedObserver new_window_observer;
    ASSERT_EQ(true, EvalJs(tab1, "!!window.open('about:blank', 'new_popup');"));
    content::WebContents* popup_window = new_window_observer.GetWebContents();
    EXPECT_TRUE(WaitForLoadStop(popup_window));
    tab1_popup = popup_window->GetPrimaryMainFrame();
  }
  EXPECT_EQ(GURL(url::kAboutBlankURL), tab1_popup->GetLastCommittedURL());

  // Verify that |tab1_popup| can find unrelated frames from the same extension
  // (i.e. that it can find |tab2|.
  std::string location_of_opened_window =
      EvalJs(tab1_popup,
             "var w = window.open('', 'tab2');\n"
             "w.location.href;")
          .ExtractString();
  EXPECT_EQ(tab2->GetLastCommittedURL(), location_of_opened_window);
}

IN_PROC_BROWSER_TEST_F(ExtensionFunctionalTest, DownloadExtensionResource) {
  auto* download_manager = profile()->GetDownloadManager();
  content::DownloadTestObserverTerminal download_observer(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("download")));
  download_observer.WaitForFinished();

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
      download_items;
  download_manager->GetAllDownloads(&download_items);

  base::ScopedAllowBlockingForTesting allow_blocking;
  auto file_path = download_items[0]->GetTargetFilePath();

  base::FilePath expected_path = ui_test_utils::GetTestFilePath(
      base::FilePath(),
      base::FilePath().AppendASCII("extensions/download/download.dat"));

  std::string actual_contents, expected_contents;
  ASSERT_TRUE(base::ReadFileToString(file_path, &actual_contents));
  ASSERT_TRUE(base::ReadFileToString(expected_path, &expected_contents));
  ASSERT_EQ(expected_contents, actual_contents);
}

}  // namespace extensions
