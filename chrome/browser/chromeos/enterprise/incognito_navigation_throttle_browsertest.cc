// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/incognito_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/navigation_extension_enabler.h"
#include "chrome/browser/sync/test/integration/extensions_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

using testing::NotNull;

namespace chromeos {

const char16_t kSimplePageContent[] = u"Basic html test.";
const char kBlockingPageContentSingular[] =
    "To use Incognito, your organization requires an extension";
const char kBlockingPageContentPlural[] =
    "To use Incognito, your organization requires some extensions";
const char kMissingPageContentSingular[] = "Can’t find extension";
const char kMissingPageContentPlural[] = "Can’t find extensions";

class IncognitoNavigationThrottleBrowserTest
    : public extensions::ExtensionBrowserTest {
 protected:
  IncognitoNavigationThrottleBrowserTest() = default;
  ~IncognitoNavigationThrottleBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->AddDefaultHandlers(
        base::FilePath("content/test/data"));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  const extensions::Extension* InstallExtension() {
    extension_ =
        LoadExtension(test_data_dir_.AppendASCII("api_test/proxy/system"));
    registry_ = extensions::ExtensionRegistry::Get(profile());
    EXPECT_TRUE(registry_->enabled_extensions().Contains(extension_->id()));
    return extension_.get();
  }

  void SetMandatoryExtensionsForIncognitoNavigation(
      const std::vector<std::string>& extensions) {
    base::Value::List values;
    for (const auto& ids : extensions) {
      values.Append(ids);
    }
    policy::PolicyMap policies;
    policies.Set(policy::key::kMandatoryExtensionsForIncognitoNavigation,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(std::move(values)),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

  bool IsPageWithContentLoaded(Browser* browser, const std::u16string& text) {
    if (!browser) {
      return false;
    }
    return 1 == ui_test_utils::FindInPage(
                    browser->tab_strip_model()->GetActiveWebContents(), text,
                    /*forward=*/false,
                    /*case_sensitive=*/false,
                    /*ordinal*/ nullptr,
                    /*selection_rect=*/nullptr);
  }

  void NavigateToSimplePage(Browser* browser) {
    GURL url(embedded_test_server()->GetURL("/simple_page.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  bool IsSimplePageSown(Browser* browser) {
    return IsPageWithContentLoaded(browser, kSimplePageContent);
  }

  // Checks that an HTML page which informs the user that navigation is blocked
  // because `extensions` were not allowed to run in Incognito. The method also
  // verifies that the correct singular/plural form of the text is shown,
  // depending on  the size of `extensions`.
  bool IsUnallowedExtensionsBlockingPageSown(
      Browser* browser,
      const std::vector<std::string>& extensions) {
    return VerifyContentExistsInPage(browser,
                                     extensions.size() > 1
                                         ? kBlockingPageContentPlural
                                         : kBlockingPageContentSingular,
                                     extensions);
  }

  // Checks that an HTML page which informs the user that navigation is blocked
  // because `extensions` are not installed in the browser. The method also
  // verifies that the correct singular/plural form of the text is shown,
  // depending on  the size of `extensions`.
  bool IsMissingExtensionsBlockingPageSown(
      Browser* browser,
      const std::vector<std::string>& extensions) {
    return VerifyContentExistsInPage(browser,
                                     extensions.size() > 1
                                         ? kMissingPageContentPlural
                                         : kMissingPageContentSingular,
                                     extensions);
  }

  Browser* incognito_browser() {
    if (!incognito_browser_) {
      incognito_browser_ = CreateIncognitoBrowser();
    }
    return incognito_browser_.get();
  }

 private:
  bool VerifyContentExistsInPage(
      Browser* browser,
      const std::string& page_heading,
      const std::vector<std::string>& extension_names_or_ids) {
    if (!IsPageWithContentLoaded(browser, base::UTF8ToUTF16(page_heading))) {
      return false;
    }
    for (const auto& ext : extension_names_or_ids) {
      if (!IsPageWithContentLoaded(browser, base::UTF8ToUTF16(ext))) {
        return false;
      }
    }
    return true;
  }

  scoped_refptr<const extensions::Extension> extension_;
  raw_ptr<extensions::ExtensionRegistry, AcrossTasksDanglingUntriaged>
      registry_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> incognito_browser_ = nullptr;
};

// Verify that when the `MandatoryExtensionsForIncognitoNavigation` policy is
// set, Incognito mode can only be used if the user allows the configured
// mandatory extensions to run in Incognito.
IN_PROC_BROWSER_TEST_F(IncognitoNavigationThrottleBrowserTest,
                       PolicySetBlockingExtension) {
  const extensions::Extension* extension = InstallExtension();
  ASSERT_THAT(extension, NotNull());

  SetMandatoryExtensionsForIncognitoNavigation({extension->id()});

  // Verify that the primary profile is not affected.
  NavigateToSimplePage(browser());
  EXPECT_TRUE(IsSimplePageSown(browser()));

  // Verify that Incognito mode is blocked.
  NavigateToSimplePage(incognito_browser());
  EXPECT_TRUE(IsUnallowedExtensionsBlockingPageSown(incognito_browser(),
                                                    {extension->name()}));

  // Allow the extension to run in Incognito and verify that navigation in
  // Incognito mode is allowed.
  extensions::util::SetIsIncognitoEnabled(extension->id(), profile(),
                                          /*enabled=*/true);
  NavigateToSimplePage(incognito_browser());
  EXPECT_TRUE(IsSimplePageSown(incognito_browser()));

  // Disallow the extension to run in Incognito and verify that navigaion is
  // again blocked.
  extensions::util::SetIsIncognitoEnabled(extension->id(), browser()->profile(),
                                          /*enabled=*/false);
  NavigateToSimplePage(incognito_browser());
  EXPECT_TRUE(IsUnallowedExtensionsBlockingPageSown(incognito_browser(),
                                                    {extension->name()}));
}

// Verify that Incognito mode can be used when the policy is not set.
IN_PROC_BROWSER_TEST_F(IncognitoNavigationThrottleBrowserTest, PolicyNotSet) {
  NavigateToSimplePage(incognito_browser());
  EXPECT_TRUE(IsSimplePageSown(incognito_browser()));
}

// Verify that Incognito mode can be used if the extension configured via the
// `MandatoryExtensionsForIncognitoNavigation` policy is uninstalled.
IN_PROC_BROWSER_TEST_F(IncognitoNavigationThrottleBrowserTest,
                       ExtensionUninstalled) {
  const extensions::Extension* extension = InstallExtension();

  SetMandatoryExtensionsForIncognitoNavigation({extension->id()});

  // Incognito mode is blocked.
  NavigateToSimplePage(incognito_browser());
  EXPECT_TRUE(IsUnallowedExtensionsBlockingPageSown(incognito_browser(),
                                                    {extension->name()}));

  // Incognito mode is allowed.
  UninstallExtension(extension->id());
  NavigateToSimplePage(incognito_browser());
  EXPECT_TRUE(IsMissingExtensionsBlockingPageSown(incognito_browser(),
                                                  {extension->id()}));
}

IN_PROC_BROWSER_TEST_F(IncognitoNavigationThrottleBrowserTest,
                       MissingExtensionsPluralForm) {
  std::vector<std::string> extensions = {"aaaa", "bbbbb"};
  SetMandatoryExtensionsForIncognitoNavigation(extensions);
  NavigateToSimplePage(incognito_browser());
  EXPECT_TRUE(
      IsMissingExtensionsBlockingPageSown(incognito_browser(), extensions));
}

}  // namespace chromeos
