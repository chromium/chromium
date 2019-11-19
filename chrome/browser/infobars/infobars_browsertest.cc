// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/devtools/devtools_infobar_delegate.h"
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability_infobar_delegate.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_observer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/pepper_broker_infobar_delegate.h"
#include "chrome/browser/plugins/flash_deprecation_infobar_delegate.h"
#include "chrome/browser/plugins/hung_plugin_infobar_delegate.h"
#include "chrome/browser/plugins/plugin_infobar_delegates.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/plugins/reload_plugin_infobar_delegate.h"
#include "chrome/browser/previews/previews_lite_page_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/extensions/installation_error_infobar_delegate.h"
#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"
#include "chrome/browser/ui/startup/automation_infobar_delegate.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/startup/google_api_keys_infobar_delegate.h"
#include "chrome/browser/ui/startup/obsolete_system_infobar_delegate.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/crx_verifier.h"
#include "components/infobars/core/infobar.h"
#include "components/nacl/common/buildflags.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/service_manager/sandbox/switches.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/startup/default_browser_infobar_delegate.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"
#endif

#if !defined(USE_AURA)
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/nacl_host/nacl_infobar_delegate.h"
#endif

class InfoBarsTest : public InProcessBrowserTest {
 public:
  InfoBarsTest() {}

  void InstallExtension(const char* filename) {
    base::FilePath path = ui_test_utils::GetTestFilePath(
        base::FilePath().AppendASCII("extensions"),
        base::FilePath().AppendASCII(filename));
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();

    extensions::TestExtensionRegistryObserver observer(
        extensions::ExtensionRegistry::Get(browser()->profile()));

    std::unique_ptr<ExtensionInstallPrompt> client(new ExtensionInstallPrompt(
        browser()->tab_strip_model()->GetActiveWebContents()));
    scoped_refptr<extensions::CrxInstaller> installer(
        extensions::CrxInstaller::Create(service, std::move(client)));
    installer->set_install_cause(extension_misc::INSTALL_CAUSE_AUTOMATION);
    installer->InstallCrx(path);

    observer.WaitForExtensionLoaded();
  }
};

IN_PROC_BROWSER_TEST_F(InfoBarsTest, TestInfoBarsCloseOnNewTheme) {
  extensions::SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override(crx_file::VerifierFormat::CRX3);
  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html"));
  InfoBarService* infobar_service1 = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Adding a theme should create an infobar.
  {
    InfoBarObserver observer(infobar_service1,
                             InfoBarObserver::Type::kInfoBarAdded);
    InstallExtension("theme.crx");
    observer.Wait();
    EXPECT_EQ(1u, infobar_service1->infobar_count());
  }

  InfoBarService* infobar_service2 = nullptr;

  // Adding a theme in a new tab should close the old tab's infobar.
  {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), embedded_test_server()->GetURL("/simple.html"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    infobar_service2 = InfoBarService::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    InfoBarObserver observer_added(infobar_service2,
                                   InfoBarObserver::Type::kInfoBarAdded);
    InfoBarObserver observer_removed(infobar_service1,
                                     InfoBarObserver::Type::kInfoBarRemoved);
    InstallExtension("theme2.crx");
    observer_removed.Wait();
    observer_added.Wait();
    EXPECT_EQ(0u, infobar_service1->infobar_count());
    EXPECT_EQ(1u, infobar_service2->infobar_count());
  }

  // Switching back to the default theme should close the infobar.
  {
    InfoBarObserver observer(infobar_service2,
                             InfoBarObserver::Type::kInfoBarRemoved);
    ThemeServiceFactory::GetForProfile(browser()->profile())->UseDefaultTheme();
    observer.Wait();
    EXPECT_EQ(0u, infobar_service2->infobar_count());
  }
}

class InfoBarUiTest : public TestInfoBar {
 public:
  InfoBarUiTest() = default;

  // TestInfoBar:
  void ShowUi(const std::string& name) override;

 private:
  using IBD = infobars::InfoBarDelegate;

  DISALLOW_COPY_AND_ASSIGN(InfoBarUiTest);
};

void InfoBarUiTest::ShowUi(const std::string& name) {
  if (name == "multiple_infobars") {
    ShowUi("hung_plugin");
    ShowUi("dev_tools");
    ShowUi("extension_dev_tools");
    ShowUi("incognito_connectability");
    ShowUi("theme_installed");
    return;
  }

  const base::flat_map<std::string, IBD::InfoBarIdentifier> kIdentifiers = {
      {"hung_plugin", IBD::HUNG_PLUGIN_INFOBAR_DELEGATE},
      {"dev_tools", IBD::DEV_TOOLS_INFOBAR_DELEGATE},
      {"extension_dev_tools", IBD::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE},
      {"incognito_connectability",
       IBD::INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE},
      {"theme_installed", IBD::THEME_INSTALLED_INFOBAR_DELEGATE},
      {"nacl", IBD::NACL_INFOBAR_DELEGATE},
      {"pepper_broker", IBD::PEPPER_BROKER_INFOBAR_DELEGATE},
      {"outdated_plugin", IBD::OUTDATED_PLUGIN_INFOBAR_DELEGATE},
      {"reload_plugin", IBD::RELOAD_PLUGIN_INFOBAR_DELEGATE},
      {"plugin_observer", IBD::PLUGIN_OBSERVER_INFOBAR_DELEGATE},
      {"file_access_disabled", IBD::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE},
      {"keystone_promotion", IBD::KEYSTONE_PROMOTION_INFOBAR_DELEGATE_MAC},
      {"collected_cookies", IBD::COLLECTED_COOKIES_INFOBAR_DELEGATE},
      {"installation_error", IBD::INSTALLATION_ERROR_INFOBAR_DELEGATE},
      {"bad_flags", IBD::BAD_FLAGS_INFOBAR_DELEGATE},
      {"default_browser", IBD::DEFAULT_BROWSER_INFOBAR_DELEGATE},
      {"google_api_keys", IBD::GOOGLE_API_KEYS_INFOBAR_DELEGATE},
      {"obsolete_system", IBD::OBSOLETE_SYSTEM_INFOBAR_DELEGATE},
      {"page_info", IBD::PAGE_INFO_INFOBAR_DELEGATE},
      {"translate", IBD::TRANSLATE_INFOBAR_DELEGATE_NON_AURA},
      {"automation", IBD::AUTOMATION_INFOBAR_DELEGATE},
      {"previews_lite_page", IBD::LITE_PAGE_PREVIEWS_INFOBAR},
      {"flash_deprecation", IBD::FLASH_DEPRECATION_INFOBAR_DELEGATE},
      {"tab_sharing", IBD::TAB_SHARING_INFOBAR_DELEGATE},
  };
  auto id_entry = kIdentifiers.find(name);
  if (id_entry == kIdentifiers.end()) {
    ADD_FAILURE() << "Unexpected infobar " << name;
    return;
  }
  auto infobar_identifier = id_entry->second;
  AddExpectedInfoBar(infobar_identifier);
  switch (infobar_identifier) {
    case IBD::HUNG_PLUGIN_INFOBAR_DELEGATE:
      HungPluginInfoBarDelegate::Create(GetInfoBarService(), nullptr, 0,
                                        base::ASCIIToUTF16("Test Plugin"));
      break;

    case IBD::DEV_TOOLS_INFOBAR_DELEGATE:
      DevToolsInfoBarDelegate::Create(
          l10n_util::GetStringFUTF16(
              IDS_DEV_TOOLS_CONFIRM_ADD_FILE_SYSTEM_MESSAGE,
              base::ASCIIToUTF16("file_path")),
          base::DoNothing());
      break;

    case IBD::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE:
      extensions::ExtensionDevToolsInfoBar::Create("id", "Extension", nullptr,
                                                   base::DoNothing());
      break;

    case IBD::INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE: {
      extensions::IncognitoConnectabilityInfoBarDelegate::Create(
          GetInfoBarService(),
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT_EXTENSION_CONNECT_FROM_INCOGNITO,
              base::ASCIIToUTF16("http://example.com"),
              base::ASCIIToUTF16("Test Extension")),
          base::DoNothing());
      break;
    }

    case IBD::THEME_INSTALLED_INFOBAR_DELEGATE:
      ThemeInstalledInfoBarDelegate::Create(
          GetInfoBarService(),
          ThemeServiceFactory::GetForProfile(browser()->profile()), "New Theme",
          "id",
          std::make_unique<ThemeService::ThemeReinstaller>(
              browser()->profile(), base::OnceClosure()));
      break;

    case IBD::NACL_INFOBAR_DELEGATE:
#if BUILDFLAG(ENABLE_NACL)
      NaClInfoBarDelegate::Create(GetInfoBarService());
#else
      ADD_FAILURE() << "This infobar is not supported when NaCl is disabled.";
#endif
      break;

    case IBD::PEPPER_BROKER_INFOBAR_DELEGATE:
      PepperBrokerInfoBarDelegate::Create(GetInfoBarService(),
                                          GURL("http://example.com/"),
                                          base::ASCIIToUTF16("Test Plugin"),
                                          nullptr, nullptr, base::DoNothing());
      break;

    case IBD::OUTDATED_PLUGIN_INFOBAR_DELEGATE:
      OutdatedPluginInfoBarDelegate::Create(
          GetInfoBarService(), nullptr,
          std::make_unique<PluginMetadata>(
              "test-plugin", base::ASCIIToUTF16("Test Plugin"), true, GURL(),
              GURL(), base::ASCIIToUTF16("Test"), std::string(), false));
      break;

    case IBD::RELOAD_PLUGIN_INFOBAR_DELEGATE:
      ReloadPluginInfoBarDelegate::Create(
          GetInfoBarService(), nullptr,
          l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT,
                                     base::ASCIIToUTF16("Test Plugin")));
      break;

    case IBD::PLUGIN_OBSERVER_INFOBAR_DELEGATE:
      PluginObserver::CreatePluginObserverInfoBar(
          GetInfoBarService(), base::ASCIIToUTF16("Test Plugin"));
      break;

    case IBD::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE:
      ChromeSelectFilePolicy(GetWebContents()).SelectFileDenied();
      break;

    case IBD::KEYSTONE_PROMOTION_INFOBAR_DELEGATE_MAC:
#if defined(OS_MACOSX)
      KeystonePromotionInfoBarDelegate::Create(GetWebContents());
#else
      ADD_FAILURE() << "This infobar is not supported on this OS.";
#endif
      break;

    case IBD::COLLECTED_COOKIES_INFOBAR_DELEGATE:
      CollectedCookiesInfoBarDelegate::Create(GetInfoBarService());
      break;

    case IBD::INSTALLATION_ERROR_INFOBAR_DELEGATE: {
      const base::string16 msg =
          l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE);
      InstallationErrorInfoBarDelegate::Create(
          GetInfoBarService(),
          extensions::CrxInstallError(
              extensions::CrxInstallErrorType::OTHER,
              extensions::CrxInstallErrorDetail::OFFSTORE_INSTALL_DISALLOWED,
              msg));
      break;
    }

    case IBD::BAD_FLAGS_INFOBAR_DELEGATE:
      chrome::ShowBadFlagsInfoBar(GetWebContents(),
                                  IDS_BAD_FLAGS_WARNING_MESSAGE,
                                  service_manager::switches::kNoSandbox);
      break;

    case IBD::DEFAULT_BROWSER_INFOBAR_DELEGATE:
#if defined(OS_CHROMEOS)
      ADD_FAILURE() << "This infobar is not supported on this OS.";
#else
      chrome::DefaultBrowserInfoBarDelegate::Create(GetInfoBarService(),
                                                    browser()->profile());
#endif
      break;

    case IBD::GOOGLE_API_KEYS_INFOBAR_DELEGATE:
      GoogleApiKeysInfoBarDelegate::Create(GetInfoBarService());
      break;

    case IBD::OBSOLETE_SYSTEM_INFOBAR_DELEGATE:
      ObsoleteSystemInfoBarDelegate::Create(GetInfoBarService());
      break;

    case IBD::SESSION_CRASHED_INFOBAR_DELEGATE_IOS:
      ADD_FAILURE() << "This infobar is not supported on this OS.";
      break;

    case IBD::PAGE_INFO_INFOBAR_DELEGATE:
      PageInfoInfoBarDelegate::Create(GetInfoBarService());
      break;

    case IBD::TRANSLATE_INFOBAR_DELEGATE_NON_AURA: {
#if defined(USE_AURA) || defined(OS_MACOSX)
      ADD_FAILURE() << "This infobar is not supported on this toolkit.";
#else
      // The translate infobar is only used on Android and iOS, neither of
      // which currently runs browser_tests. So this is currently dead code.
      ChromeTranslateClient::CreateForWebContents(GetWebContents());
      ChromeTranslateClient* translate_client =
          ChromeTranslateClient::FromWebContents(GetWebContents());
      translate::TranslateInfoBarDelegate::Create(
          false, translate_client->GetTranslateManager()->GetWeakPtr(),
          GetInfoBarService(), false,
          translate::TRANSLATE_STEP_BEFORE_TRANSLATE, "ja", "en",
          translate::TranslateErrors::NONE, false);
#endif
      break;
    }

    case IBD::AUTOMATION_INFOBAR_DELEGATE:
      AutomationInfoBarDelegate::Create();
      break;

    case IBD::LITE_PAGE_PREVIEWS_INFOBAR:
      PreviewsLitePageInfoBarDelegate::Create(GetWebContents());
      break;

    case IBD::FLASH_DEPRECATION_INFOBAR_DELEGATE:
      FlashDeprecationInfoBarDelegate::Create(GetInfoBarService(), nullptr);
      break;
    case IBD::TAB_SHARING_INFOBAR_DELEGATE:
      TabSharingInfoBarDelegate::Create(
          GetInfoBarService(), base::ASCIIToUTF16("example.com"),
          base::ASCIIToUTF16("application.com"), true, nullptr);
      break;

    default:
      break;
  }
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_hung_plugin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_dev_tools) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_extension_dev_tools) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_incognito_connectability) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_theme_installed) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(ENABLE_NACL)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_nacl) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_pepper_broker) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_outdated_plugin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_reload_plugin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_plugin_observer) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_file_access_disabled) {
  ShowAndVerifyUi();
}

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_keystone_promotion) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_collected_cookies) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_installation_error) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_bad_flags) {
  ShowAndVerifyUi();
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_default_browser) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_google_api_keys) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_obsolete_system) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_page_info) {
  ShowAndVerifyUi();
}

#if !defined(USE_AURA) && !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_translate) {
  ShowAndVerifyUi();
}
#endif

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_automation) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_previews_lite_page) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_flash_deprecation) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_tab_sharing) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_multiple_infobars) {
  ShowAndVerifyUi();
}
