// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr SystemExtensionId kTestSystemExtensionId = {1, 2, 3, 4};

constexpr char kTestSystemExtensionIndexURL[] =
    "chrome-untrusted://system-extension-echo-01020304/html/index.html";

constexpr char kTestSystemExtensionWrongURL[] =
    "chrome-untrusted://system-extension-echo-01020304/html/wrong.html";

constexpr char kTestSystemExtensionEmptyPathURL[] =
    "chrome-untrusted://system-extension-echo-01020304/";

base::FilePath GetBasicSystemExtensionDir() {
  base::FilePath test_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
  return test_dir.Append("system_extensions").Append("basic_system_extension");
}

// Class that returns the result of the first System Extension service worker it
// sees.
class ServiceWorkerRegistrationObserver
    : public SystemExtensionsInstallManager::Observer {
 public:
  explicit ServiceWorkerRegistrationObserver(SystemExtensionsProvider& provider)
      : install_manager_(provider.install_manager()) {
    install_manager_.AddObserver(this);
  }
  ~ServiceWorkerRegistrationObserver() override {}

  // Returns the saved result or waits to get a result and returns it.
  std::pair<SystemExtensionId, blink::ServiceWorkerStatusCode>
  GetIdAndStatusCode() {
    if (id_.has_value())
      return {id_.value(), status_code_.value()};

    run_loop_.Run();
    return {id_.value(), status_code_.value()};
  }

  // SystemExtensionsInstallManager::Observer
  void OnServiceWorkerRegistered(
      const SystemExtensionId& id,
      blink::ServiceWorkerStatusCode status_code) override {
    install_manager_.RemoveObserver(this);

    // Should happen because we unregistered as observers.
    DCHECK(!id_.has_value());

    id_ = id;
    status_code_ = status_code;
    run_loop_.Quit();
  }

 private:
  // Should be present for the duration of the test.
  SystemExtensionsInstallManager& install_manager_;

  base::RunLoop run_loop_;
  absl::optional<SystemExtensionId> id_;
  absl::optional<blink::ServiceWorkerStatusCode> status_code_;
};

class SystemExtensionsBrowserTest : public InProcessBrowserTest {
 public:
  SystemExtensionsBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kSystemExtensions,
         ::features::kEnableServiceWorkersForChromeUntrusted},
        {});
  }
  ~SystemExtensionsBrowserTest() override = default;

  void TestInstalledTestExtensionWorks() {
    auto* provider = SystemExtensionsProvider::Get(browser()->profile());
    auto& install_manager = provider->install_manager();

    auto extension_ids = install_manager.GetSystemExtensionIds();
    EXPECT_EQ(std::vector<SystemExtensionId>({kTestSystemExtensionId}),
              extension_ids);
    EXPECT_TRUE(install_manager.GetSystemExtensionById(kTestSystemExtensionId));

    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), GURL(kTestSystemExtensionIndexURL)));
      EXPECT_EQ(u"SystemExtension", tab->GetTitle());
    }
    {
      // Check that navigating to non-existing resources doesn't crash the
      // browser.
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), GURL(kTestSystemExtensionWrongURL)));
      content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
      EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
    }
    {
      // Check that navigating to a directory, like the root directory, doesn't
      // crash the browser.
      ASSERT_TRUE(ui_test_utils::NavigateToURL(
          browser(), GURL(kTestSystemExtensionEmptyPathURL)));
      content::NavigationEntry* entry = tab->GetController().GetVisibleEntry();
      EXPECT_EQ(content::PAGE_TYPE_ERROR, entry->GetPageType());
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SystemExtensionsSwitchBrowserTest : public SystemExtensionsBrowserTest {
 public:
  ~SystemExtensionsSwitchBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchPath(ash::switches::kInstallSystemExtension,
                                   GetBasicSystemExtensionDir());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SystemExtensionsBrowserTest, InstallFromDir_Success) {
  auto* provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider->install_manager();

  ServiceWorkerRegistrationObserver sw_registration_observer(*provider);
  base::RunLoop run_loop;
  install_manager.InstallUnpackedExtensionFromDir(
      GetBasicSystemExtensionDir(),
      base::BindLambdaForTesting([&](InstallStatusOrSystemExtensionId result) {
        EXPECT_TRUE(result.ok());
        EXPECT_EQ(kTestSystemExtensionId, result.value());
        run_loop.Quit();
      }));
  run_loop.Run();

  const auto [id, status_code] = sw_registration_observer.GetIdAndStatusCode();
  EXPECT_EQ(kTestSystemExtensionId, id);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status_code);

  TestInstalledTestExtensionWorks();
}

IN_PROC_BROWSER_TEST_F(SystemExtensionsSwitchBrowserTest, ExtensionInstalled) {
  auto* provider = SystemExtensionsProvider::Get(browser()->profile());
  auto& install_manager = provider->install_manager();

  base::RunLoop run_loop;
  install_manager.on_command_line_install_finished().Post(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  TestInstalledTestExtensionWorks();
}

}  // namespace ash
