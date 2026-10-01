// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/page_colors_controller.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/page_colors_controller_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/regular_logged_in_browser_test_mixin.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"  // nogncheck
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

#if BUILDFLAG(IS_CHROMEOS)
constexpr auto kPrimaryUserAccountId =
    AccountId::Literal::FromUserEmailGaiaId("primary@example.com",
                                            GaiaId::Literal("primary-gaia-id"));
constexpr auto kSecondaryUserAccountId =
    AccountId::Literal::FromUserEmailGaiaId(
        "secondary@example.com",
        GaiaId::Literal("secondary-gaia-id"));
#endif  // BUILDFLAG(IS_CHROMEOS)

void WaitForMediaQuery(content::WebContents* web_contents,
                       const std::string& query) {
#if BUILDFLAG(IS_CHROMEOS)
  // Renderer script does not run for WebContents on an inactive user desktop.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const AccountId* account_id =
      ash::AnnotatedAccountId::Get(profile->GetOriginalProfile());
  ASSERT_TRUE(account_id);
  user_manager::UserManager* user_manager_instance =
      user_manager::UserManager::Get();
  if (user_manager_instance->GetActiveUser()->GetAccountId() != *account_id) {
    user_manager_instance->SwitchActiveUser(*account_id);
    ASSERT_EQ(user_manager_instance->GetActiveUser()->GetAccountId(),
              *account_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  ASSERT_EQ(true, content::EvalJs(web_contents, content::JsReplace(R"(
    new Promise(resolve => {
      const media_query = matchMedia($1);
      if (media_query.matches) {
        resolve(true);
        return;
      }
      media_query.addEventListener(
          'change', event => event.matches && resolve(true), {once: true});
    })
  )",
                                                                   query)));
}

}  // namespace

class PageColorsControllerBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  void SetUpLocalStatePrefService(PrefService* local_state) override {
    MixinBasedInProcessBrowserTest::SetUpLocalStatePrefService(local_state);
    user_manager::TestHelper::RegisterPersistedUser(*local_state,
                                                    kSecondaryUserAccountId);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  ui::MockOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }

  content::WebContents* GetWebContents(
      BrowserWindowInterface* browser_window = nullptr) {
    if (!browser_window) {
      browser_window = browser();
    }
    return browser_window->GetActiveTabInterface()->GetContents();
  }

  void WaitForForcedColors(bool active,
                           content::WebContents* web_contents = nullptr) {
    web_contents = web_contents ? web_contents : GetWebContents();
    WaitForMediaQuery(
        web_contents,
        base::StringPrintf("(forced-colors: %s)", active ? "active" : "none"));
  }

  void WaitForPreferredColorScheme(
      blink::mojom::PreferredColorScheme preferred_color_scheme,
      content::WebContents* web_contents = nullptr) {
    web_contents = web_contents ? web_contents : GetWebContents();
    WaitForMediaQuery(
        web_contents,
        base::StringPrintf(
            "(prefers-color-scheme: %s)",
            preferred_color_scheme == blink::mojom::PreferredColorScheme::kDark
                ? "dark"
                : "light"));
  }

  void WaitForPreferredContrast(
      blink::mojom::PreferredContrast preferred_contrast,
      content::WebContents* web_contents = nullptr) {
    web_contents = web_contents ? web_contents : GetWebContents();
    WaitForMediaQuery(
        web_contents,
        base::StringPrintf(
            "(prefers-contrast: %s)",
            preferred_contrast == blink::mojom::PreferredContrast::kMore
                ? "more"
                : "no-preference"));
  }

  ui::ColorProviderKey::ForcedColors forced_colors(
      content::WebContents* web_contents = nullptr) {
    return content::GetContentClientForTesting()
        ->browser()
        ->GetForcedColorsForWebContents(
            *(web_contents ? web_contents : GetWebContents()));
  }

  Profile& CreateSecondProfile() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
#if BUILDFLAG(IS_CHROMEOS)
    const std::string secondary_user_hash =
        user_manager::TestHelper::GetFakeUsernameHash(kSecondaryUserAccountId);
    session_manager::SessionManager::Get()->CreateSession(
        kSecondaryUserAccountId, secondary_user_hash,
        /*new_user=*/false,
        /*has_active_session=*/false);
    return profiles::testing::CreateProfileSync(
        profile_manager,
        profile_manager->user_data_dir().AppendASCII(
            ash::BrowserContextHelper::GetUserBrowserContextDirName(
                secondary_user_hash)));
#else
    return profiles::testing::CreateProfileSync(
        profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
#endif
  }

 private:
#if BUILDFLAG(IS_CHROMEOS)
  ash::RegularLoggedInBrowserTestMixin logged_in_mixin_{&mixin_host_,
                                                        kPrimaryUserAccountId};
#endif  // BUILDFLAG(IS_CHROMEOS)

  ui::MockOsSettingsProvider os_settings_provider_;
};

IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest, PageColorsInIncognito) {
  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  PageColorsControllerFactory::GetForProfile(browser()->GetProfile())
      ->SetRequestedPageColors(PageColors::kWhite);

  BrowserWindowInterface* incognito_browser =
      CreateIncognitoBrowser(browser()->GetProfile());
  content::WebContents* incognito_contents = GetWebContents(incognito_browser);

  // Incognito inherits Page Colors, including the preferred color scheme, from
  // the original profile.
  EXPECT_EQ(forced_colors(incognito_contents),
            ui::ColorProviderKey::ForcedColors::kWhite);
  WaitForForcedColors(true, incognito_contents);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kLight,
                              incognito_contents);
  WaitForPreferredContrast(blink::mojom::PreferredContrast::kMore,
                           incognito_contents);

  PageColorsControllerFactory::GetForProfile(browser()->GetProfile())
      ->SetRequestedPageColors(PageColors::kOff);
  EXPECT_EQ(forced_colors(incognito_contents),
            ui::ColorProviderKey::ForcedColors::kNone);
  WaitForForcedColors(false, incognito_contents);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kDark,
                              incognito_contents);
  WaitForPreferredContrast(blink::mojom::PreferredContrast::kNoPreference,
                           incognito_contents);
}

IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest,
                       PageColorsAreProfileSpecific) {
  Profile& second_profile = CreateSecondProfile();
  BrowserWindowInterface* second_browser = CreateBrowser(&second_profile);
  content::WebContents* first_contents = GetWebContents();
  content::WebContents* second_contents = GetWebContents(second_browser);

  PageColorsControllerFactory::GetForProfile(browser()->GetProfile())
      ->SetRequestedPageColors(PageColors::kDusk);
  EXPECT_EQ(forced_colors(first_contents),
            ui::ColorProviderKey::ForcedColors::kDusk);
  WaitForForcedColors(true, first_contents);
  EXPECT_EQ(forced_colors(second_contents),
            ui::ColorProviderKey::ForcedColors::kNone);
  WaitForForcedColors(false, second_contents);

  PageColorsControllerFactory::GetForProfile(&second_profile)
      ->SetRequestedPageColors(PageColors::kDesert);
  EXPECT_EQ(forced_colors(first_contents),
            ui::ColorProviderKey::ForcedColors::kDusk);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kDark,
                              first_contents);
  EXPECT_EQ(forced_colors(second_contents),
            ui::ColorProviderKey::ForcedColors::kDesert);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kLight,
                              second_contents);
}

IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest,
                       ApplyPageColorsOnIncreasedContrast) {
  // When `kApplyPageColorsOnlyOnIncreasedContrast` is true but the OS is not in
  // increased contrast mode, there should be no forced colors.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast, true);
  PageColorsControllerFactory::GetForProfile(browser()->GetProfile())
      ->SetRequestedPageColors(PageColors::kDusk);
  WaitForForcedColors(false);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kNone);

  // Once the OS is in increased contrast mode, the requested page colors should
  // be honored.
  os_settings_provider().SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kMore);
  WaitForForcedColors(true);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kDusk);

  // Switching increased contrast back off should turn forced colors back off
  // since `kApplyPageColorsOnlyOnIncreasedContrast` is still true.
  os_settings_provider().SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kNoPreference);
  WaitForForcedColors(false);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kNone);

  // Setting `kApplyPageColorsOnlyOnIncreasedContrast` to false should lead to
  // honoring the requested page colors.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kApplyPageColorsOnlyOnIncreasedContrast, false);
  WaitForForcedColors(true);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kDusk);
}

IN_PROC_BROWSER_TEST_F(PageColorsControllerBrowserTest,
                       BrowserThemeAndOSTheme) {
  auto* const page_colors =
      PageColorsControllerFactory::GetForProfile(browser()->GetProfile());
  ThemeServiceFactory::GetForProfile(browser()->GetProfile())
      ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kLight);

  // The web theme should be in a default state.
  WaitForForcedColors(false);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kNone);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kLight);
  WaitForPreferredContrast(blink::mojom::PreferredContrast::kNoPreference);

  // Changing page colors to Dusk should be reflected in the web theme's
  // contrast and color scheme.
  page_colors->SetRequestedPageColors(PageColors::kDusk);
  WaitForForcedColors(true);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kDusk);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kDark);
  WaitForPreferredContrast(blink::mojom::PreferredContrast::kMore);

  // Changing page colors to White should similarly be reflected.
  page_colors->SetRequestedPageColors(PageColors::kWhite);
  WaitForForcedColors(true);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kWhite);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kLight);
  WaitForPreferredContrast(blink::mojom::PreferredContrast::kMore);

  // Changing the OS theme to high contrast should not overwrite explicit page
  // colors.
  os_settings_provider().SetForcedColorsActive(true);
  os_settings_provider().SetPreferredContrast(
      ui::NativeTheme::PreferredContrast::kMore);
  os_settings_provider().SetPreferredColorScheme(
      ui::NativeTheme::PreferredColorScheme::kDark);
  WaitForForcedColors(true);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kWhite);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kLight);
  WaitForPreferredContrast(blink::mojom::PreferredContrast::kMore);

  // Changing page colors to Off should disable forced colors and restore the
  // normal browser color scheme.
  page_colors->SetRequestedPageColors(PageColors::kOff);
  WaitForForcedColors(false);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kNone);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kLight);
  WaitForPreferredContrast(blink::mojom::PreferredContrast::kNoPreference);

  // Changing the browser color scheme should be reflected in the web theme
  // while page colors are Off.
  ThemeServiceFactory::GetForProfile(browser()->GetProfile())
      ->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kDark);
  WaitForForcedColors(false);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kNone);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kDark);
  WaitForPreferredContrast(blink::mojom::PreferredContrast::kNoPreference);

  // Changing the page colors to No Preference should restore the native high
  // contrast state and its preferred color scheme.
  page_colors->SetRequestedPageColors(PageColors::kNoPreference);
  WaitForForcedColors(true);
  EXPECT_EQ(forced_colors(), ui::ColorProviderKey::ForcedColors::kSystem);
  WaitForPreferredColorScheme(blink::mojom::PreferredColorScheme::kDark);
  WaitForPreferredContrast(blink::mojom::PreferredContrast::kMore);
}
