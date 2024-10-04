// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/devtools/devtools_infobar_delegate.h"
#include "chrome/browser/extensions/api/debugger/extension_dev_tools_infobar_delegate.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability_infobar_delegate.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_observer.h"
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
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/crx_verifier.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/policy/switches.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/hung_plugin_infobar_delegate.h"
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/plugins/reload_plugin_infobar_delegate.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_delegate.h"
#endif

#if BUILDFLAG(IS_MAC) && BUILDFLAG(ENABLE_UPDATER)
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html")));
  infobars::ContentInfoBarManager* infobar_manager1 =
      infobars::ContentInfoBarManager::FromWebContents(
          browser()->tab_strip_model()->GetActiveWebContents());

  // Adding a theme should create an infobar.
  {
    InfoBarObserver observer(infobar_manager1,
                             InfoBarObserver::Type::kInfoBarAdded);
    InstallExtension("theme.crx");
    observer.Wait();
    EXPECT_EQ(1u, infobar_manager1->infobars().size());
  }

  infobars::ContentInfoBarManager* infobar_manager2 = nullptr;

  // Adding a theme in a new tab should close the old tab's infobar.
  {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), embedded_test_server()->GetURL("/simple.html"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    infobar_manager2 = infobars::ContentInfoBarManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    InfoBarObserver observer_added(infobar_manager2,
                                   InfoBarObserver::Type::kInfoBarAdded);
    InfoBarObserver observer_removed(infobar_manager1,
                                     InfoBarObserver::Type::kInfoBarRemoved);
    InstallExtension("theme2.crx");
    observer_removed.Wait();
    observer_added.Wait();
    EXPECT_EQ(0u, infobar_manager1->infobars().size());
    EXPECT_EQ(1u, infobar_manager2->infobars().size());
  }

  // Switching back to the default theme should close the infobar.
  {
    InfoBarObserver observer(infobar_manager2,
                             InfoBarObserver::Type::kInfoBarRemoved);
    ThemeServiceFactory::GetForProfile(browser()->profile())->UseDefaultTheme();
    observer.Wait();
    EXPECT_EQ(0u, infobar_manager2->infobars().size());
  }
}

class InfoBarUiTest : public TestInfoBar {
 public:
  InfoBarUiTest() = default;

  InfoBarUiTest(const InfoBarUiTest&) = delete;
  InfoBarUiTest& operator=(const InfoBarUiTest&) = delete;

  // TestInfoBar:
  void ShowUi(const std::string& name) override;
  bool VerifyUi() override;

 private:
  using IBD = infobars::InfoBarDelegate;
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

  constexpr auto kIdentifiers =
      base::MakeFixedFlatMap<std::string_view, IBD::InfoBarIdentifier>({
          {"dev_tools", IBD::DEV_TOOLS_INFOBAR_DELEGATE},
          {"extension_dev_tools", IBD::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE},
          {"incognito_connectability",
           IBD::INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE},
          {"theme_installed", IBD::THEME_INSTALLED_INFOBAR_DELEGATE},
          {"nacl", IBD::NACL_INFOBAR_DELEGATE},
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
          {"tab_sharing", IBD::TAB_SHARING_INFOBAR_DELEGATE},

#if BUILDFLAG(ENABLE_PLUGINS)
          {"hung_plugin", IBD::HUNG_PLUGIN_INFOBAR_DELEGATE},
          {"reload_plugin", IBD::RELOAD_PLUGIN_INFOBAR_DELEGATE},
          {"plugin_observer", IBD::PLUGIN_OBSERVER_INFOBAR_DELEGATE},
#endif  // BUILDFLAG(ENABLE_PLUGINS)
      });
  const auto id_entry = kIdentifiers.find(name);
  if (id_entry == kIdentifiers.end()) {
    ADD_FAILURE() << "Unexpected infobar " << name;
    return;
  }
  const auto infobar_identifier = id_entry->second;
  AddExpectedInfoBar(infobar_identifier);
  switch (infobar_identifier) {
    case IBD::DEV_TOOLS_INFOBAR_DELEGATE:
      DevToolsInfoBarDelegate::Create(
          l10n_util::GetStringFUTF16(
              IDS_DEV_TOOLS_CONFIRM_ADD_FILE_SYSTEM_MESSAGE, u"file_path"),
          base::DoNothing());
      break;

    case IBD::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE:
      std::ignore = extensions::ExtensionDevToolsInfoBarDelegate::Create(
          "id", "Extension", base::DoNothing());
      break;

    case IBD::INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE: {
      extensions::IncognitoConnectabilityInfoBarDelegate::Create(
          GetInfoBarManager(),
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PROMPT_EXTENSION_CONNECT_FROM_INCOGNITO,
              u"http://example.com", u"Test Extension"),
          base::DoNothing());
      break;
    }

    case IBD::THEME_INSTALLED_INFOBAR_DELEGATE:
      ThemeInstalledInfoBarDelegate::Create(
          GetInfoBarManager(),
          ThemeServiceFactory::GetForProfile(browser()->profile()), "New Theme",
          "id",
          std::make_unique<ThemeService::ThemeReinstaller>(
              browser()->profile(), base::OnceClosure()));
      break;

    case IBD::NACL_INFOBAR_DELEGATE:
#if BUILDFLAG(ENABLE_NACL)
      NaClInfoBarDelegate::Create(GetInfoBarManager());
#else
      ADD_FAILURE() << "This infobar is not supported when NaCl is disabled.";
#endif
      break;

#if BUILDFLAG(ENABLE_PLUGINS)
    case IBD::HUNG_PLUGIN_INFOBAR_DELEGATE:
      HungPluginInfoBarDelegate::Create(GetInfoBarManager(), nullptr, 0,
                                        u"Test Plugin");
      break;

    case IBD::RELOAD_PLUGIN_INFOBAR_DELEGATE:
      ReloadPluginInfoBarDelegate::Create(
          GetInfoBarManager(), nullptr,
          l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT,
                                     u"Test Plugin"));
      break;

    case IBD::PLUGIN_OBSERVER_INFOBAR_DELEGATE:
      PluginObserver::CreatePluginObserverInfoBar(GetInfoBarManager(),
                                                  u"Test Plugin");
      break;
#endif  // BUILDFLAG(ENABLE_PLUGINS)

    case IBD::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE:
      ChromeSelectFilePolicy(GetWebContents()).SelectFileDenied();
      break;

    case IBD::KEYSTONE_PROMOTION_INFOBAR_DELEGATE_MAC:
#if BUILDFLAG(IS_MAC) && BUILDFLAG(ENABLE_UPDATER)
      KeystonePromotionInfoBarDelegate::Create(GetWebContents());
#else
      ADD_FAILURE() << "This infobar is not supported on this OS.";
#endif
      break;

    case IBD::COLLECTED_COOKIES_INFOBAR_DELEGATE:
      CollectedCookiesInfoBarDelegate::Create(GetInfoBarManager());
      break;

    case IBD::INSTALLATION_ERROR_INFOBAR_DELEGATE: {
      const std::u16string msg =
          l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE);
      InstallationErrorInfoBarDelegate::Create(
          GetInfoBarManager(),
          extensions::CrxInstallError(
              extensions::CrxInstallErrorType::OTHER,
              extensions::CrxInstallErrorDetail::OFFSTORE_INSTALL_DISALLOWED,
              msg));
      break;
    }

    case IBD::BAD_FLAGS_INFOBAR_DELEGATE:
      ShowBadFlagsInfoBar(GetWebContents(), IDS_BAD_FLAGS_WARNING_MESSAGE,
                          sandbox::policy::switches::kNoSandbox);
      break;

    case IBD::DEFAULT_BROWSER_INFOBAR_DELEGATE:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ADD_FAILURE() << "This infobar is not supported on this OS.";
#else
      DefaultBrowserInfoBarDelegate::Create(GetInfoBarManager(),
                                            browser()->profile());
#endif
      break;

    case IBD::GOOGLE_API_KEYS_INFOBAR_DELEGATE:
      GoogleApiKeysInfoBarDelegate::Create(GetInfoBarManager());
      break;

    case IBD::OBSOLETE_SYSTEM_INFOBAR_DELEGATE:
      ObsoleteSystemInfoBarDelegate::Create(GetInfoBarManager());
      break;

    case IBD::SESSION_CRASHED_INFOBAR_DELEGATE_IOS:
      ADD_FAILURE() << "This infobar is not supported on this OS.";
      break;

    case IBD::PAGE_INFO_INFOBAR_DELEGATE:
      PageInfoInfoBarDelegate::Create(GetInfoBarManager());
      break;

    case IBD::TRANSLATE_INFOBAR_DELEGATE_NON_AURA: {
#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
      ADD_FAILURE() << "This infobar is not supported on this toolkit.";
#else
      // The translate infobar is only used on Android and iOS, neither of
      // which currently runs browser_tests. So this is currently dead code.
      ChromeTranslateClient::CreateForWebContents(GetWebContents());
      ChromeTranslateClient* translate_client =
          ChromeTranslateClient::FromWebContents(GetWebContents());
      translate::TranslateInfoBarDelegate::Create(
          false, translate_client->GetTranslateManager()->GetWeakPtr(),
          GetInfoBarManager(), translate::TRANSLATE_STEP_BEFORE_TRANSLATE, "ja",
          "en", translate::TranslateErrors::NONE, false);
#endif
      break;
    }

    case IBD::AUTOMATION_INFOBAR_DELEGATE:
      AutomationInfoBarDelegate::Create();
      break;

    case IBD::TAB_SHARING_INFOBAR_DELEGATE:
      TabSharingInfoBarDelegate::Create(
          /*infobar_manager=*/GetInfoBarManager(),
          /*old_infobar=*/nullptr,
          /*shared_tab_name=*/u"example.com",
          /*capturer_name=*/u"application.com",
          /*web_contents=*/nullptr,
          /*role=*/TabSharingInfoBarDelegate::TabRole::kOtherTab,
          /*share_this_tab_instead_button_state=*/
          TabSharingInfoBarDelegate::ButtonState::ENABLED,
          /*focus_target=*/std::nullopt,
          /*captured_surface_control_active=*/false,
          /*ui=*/nullptr, TabSharingInfoBarDelegate::TabShareType::CAPTURE);
      break;

    default:
      ADD_FAILURE() << "Unhandled infobar " << name;
      break;
  }
}

bool InfoBarUiTest::VerifyUi() {
  const auto* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  return TestInfoBar::VerifyUi() &&
         (VerifyPixelUi(BrowserView::GetBrowserViewForBrowser(browser())
                            ->infobar_container(),
                        test_info->test_suite_name(),
                        test_info->name()) != ui::test::ActionResult::kFailed);
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/40261456): This test case has been frequently failing on
// "Win10 Tests x64" since 2024-05-08.
#define MAYBE_InvokeUi_dev_tools DISABLED_InvokeUi_dev_tools
#else
#define MAYBE_InvokeUi_dev_tools InvokeUi_dev_tools
#endif
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, MAYBE_InvokeUi_dev_tools) {
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

#if BUILDFLAG(ENABLE_PLUGINS)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_hung_plugin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_reload_plugin) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_plugin_observer) {
  ShowAndVerifyUi();
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_file_access_disabled) {
  ShowAndVerifyUi();
}

#if BUILDFLAG(IS_MAC) && BUILDFLAG(ENABLE_UPDATER)
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
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

#if !defined(USE_AURA) && !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_translate) {
  ShowAndVerifyUi();
}
#endif

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/40261456): This test case has been frequently failing on
// "Win10 Tests x64" since 2024-05-08.
#define MAYBE_InvokeUi_automation DISABLED_InvokeUi_automation
#else
#define MAYBE_InvokeUi_automation InvokeUi_automation
#endif
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, MAYBE_InvokeUi_automation) {
  ShowAndVerifyUi();
}

// Consistently failing on Windows https://crbug.com/1462107.
#if BUILDFLAG(IS_WIN)
#define MAYBE_InvokeUi_tab_sharing DISABLED_InvokeUi_tab_sharing
#else
#define MAYBE_InvokeUi_tab_sharing InvokeUi_tab_sharing
#endif
IN_PROC_BROWSER_TEST_F(InfoBarUiTest, MAYBE_InvokeUi_tab_sharing) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(InfoBarUiTest, InvokeUi_multiple_infobars) {
  ShowAndVerifyUi();
}
