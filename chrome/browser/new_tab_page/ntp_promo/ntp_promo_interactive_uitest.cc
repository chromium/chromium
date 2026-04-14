// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <string_view>

#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/modules/modules_switches.h"
#include "chrome/browser/new_tab_page/modules/test_support.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_context.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/browser/user_education/ntp_promo_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_metadata.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_urls.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/identifier/typed_identifier.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "url/gurl.h"

namespace {

using ntp_promo::mojom::ShowNtpPromosResult;
using ::testing::ValuesIn;
using user_education::features::NtpBrowserPromoType;
using Eligibility = user_education::NtpPromoSpecification::Eligibility;

inline constexpr char kTestPromoName[] = "test_promo";
const InteractiveBrowserTestApi::DeepQuery kPathToNtp = {"ntp-app"};
const InteractiveBrowserTestApi::DeepQuery kPathToPromo = {
    "ntp-app", "individual-promos", "#promos", "#promo"};
const InteractiveBrowserTestApi::DeepQuery kPathToMenuButton = {
    "ntp-app", "individual-promos", "#promos", "#promo", "#menuButton"};
const InteractiveBrowserTestApi::DeepQuery kPathToDismissOption = {
    "ntp-app", "individual-promos", ".dropdown-item"};

constexpr char kPromoTextId[] = "#bodyText";
constexpr char kPromoIconId[] = "#bodyIcon";
constexpr char kSignInIconName[] = "account_circle";
constexpr char kExtensionsIconName[] = "my_extensions";
constexpr char kCustomizationIconName[] = "palette";
const std::u16string kShortPromoText = u"Short promo text";
const std::u16string kLongPromoText =
    u"This is a long promo text string that should cause the promo to grow "
    "vertically, stretching the bounds of the promo; no text should be "
    "truncated or harmed in the adjustment of this element's sizing";
constexpr std::string_view kNtpURL = chrome::kChromeUINewTabURL;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBrowser2NtpElementId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTestPromoShownEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTestPromoClickedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMetricsRecordedEvent);

// Contains variables on which these tests may be parameterized. This approach
// makes it easy to build sets of relevant tests, vs. the brute-force
// testing::Combine() approach.
struct NtpPromoUiTestParams {
  bool long_text = false;
  ui::NativeTheme::PreferredColorScheme color_scheme =
      ui::NativeTheme::PreferredColorScheme::kLight;
  bool rtl = false;

  std::string ToString() const {
    std::ostringstream oss;
    oss << "promo";
    if (long_text) {
      oss << "_long_text";
    }
    if (color_scheme == ui::NativeTheme::PreferredColorScheme::kDark) {
      oss << "_dark";
    }
    if (rtl) {
      oss << "_rtl";
    }

    return oss.str();
  }
};

DEFINE_LOCAL_POLLING_VIEW_PROPERTY_STATE_IDENTIFIER(OmniboxViewViews,
                                                    GetText,
                                                    kLocationBarTextValue);

MATCHER_P(OptionalStringContains, text, "Optional string contains") {
  return arg.has_value() && arg.value().find(text) != std::u16string::npos;
}

}  // namespace

class NtpPromoUiTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  NtpPromoUiTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        user_education::features::kEnableNtpBrowserPromos,
        {{user_education::features::kNtpBrowserPromoType.name, "simple"}});
  }
  ~NtpPromoUiTest() override = default;

  void ClearRegisteredPromos() {
    UserEducationService* const service =
        UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
    service->ntp_promo_registry()->ClearPromosForTesting();
  }

  // Removes all promos except the specified ID. This is an easy way to test
  // individual promo functionality, as opposed to trying to target a specific
  // promo on-screen (which may not even be shown due to promo-count limits).
  void ClearRegisteredPromosExcept(
      const user_education::NtpPromoIdentifier& keep_id) {
    UserEducationService* const service =
        UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
    auto ids = service->ntp_promo_registry()->GetNtpPromoIdentifiers();
    for (const auto& id : ids) {
      if (id != keep_id) {
        service->ntp_promo_registry()->ClearPromoForTesting(id);
      }
    }
  }

  void RegisterTestPromo(const user_education::NtpPromoIdentifier& id,
                         Eligibility eligibility,
                         int text_id) {
    UserEducationService* const service =
        UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
    user_education::NtpPromoRegistry* registry = service->ntp_promo_registry();
    user_education::NtpPromoSpecification spec(
        id, user_education::NtpPromoContent(kSignInIconName, text_id, text_id),
        base::BindLambdaForTesting(
            [=](const user_education::UserEducationContextPtr& context) {
              return eligibility;
            }),
        base::BindRepeating(&NtpPromoUiTest::OnTestPromoShown,
                            base::Unretained(this)),
        base::BindRepeating(&NtpPromoUiTest::OnTestPromoClicked,
                            base::Unretained(this)),
        {}, user_education::Metadata());
    registry->AddPromo(std::move(spec));
  }

  void InstallTestPromo(Eligibility eligibility) {
    ClearRegisteredPromos();
    RegisterTestPromo(kTestPromoName, eligibility, IDS_NTP_CUSTOMIZATION_PROMO);
  }

  auto WaitForPromoIcon(
      std::string_view expected_icon,
      const ui::ElementIdentifier& tab_element_id = kNtpElementId) {
    const auto path = kPathToPromo + kPromoIconId;
    auto steps = Steps(
        WaitForAndScrollToElement(tab_element_id, path),
        // Verify the icon shows the correct image.
        CheckJsResultAt(tab_element_id, path, "el => el.icon", expected_icon));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto WaitForPromoVisible(
      std::string_view expected_icon,
      const ui::ElementIdentifier& tab_element_id = kNtpElementId) {
    MultiStep steps = Steps(
        WaitForPromoIcon(std::string("ntp-promo:") + std::string(expected_icon),
                         tab_element_id));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto VerifyTestPromoText() {
    return CheckJsResultAt(
               kNtpElementId, kPathToPromo + kPromoTextId, "el => el.innerText",
               l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZATION_PROMO))
        .AddDescriptionPrefix(__func__);
  }

  auto ClickPromo(const ui::ElementIdentifier& tab_element_id = kNtpElementId) {
    auto steps = ClickElement(tab_element_id, kPathToPromo + kPromoIconId);
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  static constexpr std::string_view kShowResultHistogramName =
      "UserEducation.NtpPromos.ShowResult";

  auto CheckShowMetrics(ShowNtpPromosResult expected_result) {
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = kPathToNtp;
    state_change.test_function =
        "el => {"
        "return el.getAttribute('modules-loaded-status_') !== '0' || "
        "!el.querySelector('ntp-modules');"
        "}";
    state_change.event = kMetricsRecordedEvent;
    auto steps = Steps(
        WaitForStateChange(kNtpElementId, std::move(state_change)), Do([]() {
          metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
        }),
        Do([this, expected_result]() {
          histogram_tester_.ExpectBucketCount(kShowResultHistogramName,
                                              expected_result, 1);
          histogram_tester_.ExpectTotalCount(kShowResultHistogramName, 1);
        }));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  // Ensures that a single tab is open, ie. no second tab has spawned.
  auto CheckOneTabOpen() {
    return Check([this]() -> bool {
      return browser()->tab_strip_model()->count() == 1;
    });
  }

 private:
  void OnTestPromoShown() {
    BrowserElements::From(browser())->NotifyEvent(kBrowserViewElementId,
                                                  kTestPromoShownEvent);
  }

  void OnTestPromoClicked(
      const user_education::UserEducationContextPtr& context) {
    auto* browser_context = context->AsA<BrowserUserEducationContext>();
    BrowserWindowInterface* window =
        browser_context->GetBrowserView().browser();
    EXPECT_EQ(browser(), window);
    BrowserElements::From(window)->NotifyEvent(kBrowserViewElementId,
                                               kTestPromoClickedEvent);
  }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(NtpPromoUiTest, TestPromoEligible) {
  InstallTestPromo(Eligibility::kEligible);
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      // Because the "promo was shown" event is fired asynchronously as the page
      // is loading, watch for it in parallel with navigating to the NTP.
      InParallel(
          RunSubsequence(NavigateWebContents(kNtpElementId, GURL(kNtpURL))),
          RunSubsequence(
              WaitForEvent(kBrowserViewElementId, kTestPromoShownEvent))),
      // Should already be visible at this point, but confirm it is and that it
      // is in the correct state.
      WaitForPromoVisible(kSignInIconName), VerifyTestPromoText(),
      // As before, because the click and the event are sent asynchronously,
      // run these in parallel.
      InParallel(RunSubsequence(ClickPromo()),
                 RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                             kTestPromoClickedEvent))),
      // Verify that the correct histogram was recorded.
      CheckShowMetrics(ShowNtpPromosResult::kShown));
}

IN_PROC_BROWSER_TEST_F(NtpPromoUiTest, DismissPromo) {
  InstallTestPromo(Eligibility::kEligible);

  StateChange promo_hidden;
  promo_hidden.type = StateChange::Type::kDoesNotExist;
  promo_hidden.where = kPathToPromo;
  DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kPromoHiddenEvent);
  promo_hidden.event = kPromoHiddenEvent;

  RunTestSequence(InstrumentTab(kNtpElementId),
                  NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
                  WaitForPromoVisible(kSignInIconName),
                  ClickElement(kNtpElementId, kPathToMenuButton),
                  ClickElement(kNtpElementId, kPathToDismissOption),
                  // Verify promo is gone
                  WaitForStateChange(kNtpElementId, promo_hidden));
}

namespace {
const InteractiveBrowserTestApi::DeepQuery kPathToModules = {"ntp-app",
                                                             "ntp-modules"};
}  // namespace

class NtpPromoWithModuleUiTest : public NtpPromoUiTest {
 public:
  NtpPromoWithModuleUiTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpTabGroupsModule,
        {{ntp_features::kNtpTabGroupsModuleDataParam, "Fake Data"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NtpPromoWithModuleUiTest, ModuleEnabled) {
  InstallTestPromo(Eligibility::kEligible);
  RunTestSequence(InstrumentTab(kNtpElementId),
                  NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
                  WaitForAndScrollToElement(kNtpElementId, kPathToModules),
                  EnsureNotPresent(kNtpElementId, kPathToPromo),
                  CheckShowMetrics(ShowNtpPromosResult::kNotShownDueToPolicy));
}

IN_PROC_BROWSER_TEST_F(NtpPromoWithModuleUiTest, ModuleDisabled) {
  // Disable the Tab Groups module in prefs.
  {
    ScopedListPrefUpdate update(browser()->profile()->GetPrefs(),
                                prefs::kNtpDisabledModules);
    base::ListValue& list = update.Get();
    base::Value module_id_value(ntp_modules::kTabGroupsModuleId);
    list.Append(std::move(module_id_value));
  }
  InstallTestPromo(Eligibility::kEligible);
  RunTestSequence(InstrumentTab(kNtpElementId),
                  NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
                  WaitForAndScrollToElement(kNtpElementId, kPathToPromo),
                  EnsureNotVisible(kNtpElementId, kPathToModules),
                  CheckShowMetrics(ShowNtpPromosResult::kShown));
}

IN_PROC_BROWSER_TEST_F(NtpPromoUiTest, ExtensionsPromoAppearsAndIsClickable) {
  ClearRegisteredPromosExcept(kNtpExtensionsPromoId);
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
      WaitForPromoVisible(kExtensionsIconName),

      // Since bots cannot navigate to actual pages, we can't use
      // WaitForWebContentsNavigation() or the like. Instead, verify that the
      // browser *tries* to navigate to the account login page.
      PollViewProperty(kLocationBarTextValue, kOmniboxElementId),
      // Click the promo button; this should navigate the current page.
      ClickPromo(),
      // Note that the URL here may not match what users see, due to redirects.
      WaitForState(
          kLocationBarTextValue,
          ::testing::AllOf(OptionalStringContains(u"webstore"),
                           OptionalStringContains(base::UTF8ToUTF16(
                               extension_urls::kNtpPromo1pUtmSource)))),
      // The NTP tab should navigate, rather than opening a new tab.
      CheckOneTabOpen());

  // TODD(https://crbug.com/433607240): Check model, histograms.
}

IN_PROC_BROWSER_TEST_F(NtpPromoUiTest,
                       CustomizationPromoAppearsAndIsClickable) {
  ClearRegisteredPromosExcept(kNtpCustomizationPromoId);
  RunTestSequence(InstrumentTab(kNtpElementId),
                  NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
                  WaitForPromoVisible(kCustomizationIconName), ClickPromo(),
                  WaitForShow(kSidePanelElementId));

  // TODD(https://crbug.com/433607240): Check model, histograms.
}

// Regression test for crbug.com/485875459. With a second browser window open,
// ensure that a second-window promo click opens the customization side panel
// in the correct window. This test fails without the associated fix.
IN_PROC_BROWSER_TEST_F(NtpPromoUiTest,
                       CustomizationPromoOpensInCorrectBrowser) {
  ClearRegisteredPromosExcept(kNtpCustomizationPromoId);

  // Create a second browser window.
  Browser* browser2 = CreateBrowser(browser()->profile());

  RunTestSequence(
      // Set up the first browser.
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
      WaitForPromoVisible(kCustomizationIconName, kNtpElementId),

      // Set up the second browser.
      InstrumentTab(kBrowser2NtpElementId, 0, browser2),
      NavigateWebContents(kBrowser2NtpElementId, GURL(kNtpURL)),

      // Target the interactions to the second browser's context.
      InContext(
          BrowserView::GetBrowserViewForBrowser(browser2)->GetElementContext(),
          Steps(WaitForPromoVisible(kCustomizationIconName,
                                    kBrowser2NtpElementId),
                ClickPromo(kBrowser2NtpElementId),
                // Wait for the side panel to show in the second browser.
                WaitForShow(kSidePanelElementId))),

      // Verify the side panel is showing in the second browser.
      Check([browser2]() {
        return browser2->GetFeatures().side_panel_ui()->IsSidePanelEntryShowing(
            SidePanelEntry::Key(SidePanelEntry::Id::kCustomizeChrome));
      }),

      // Verify the side panel is NOT showing in the first browser.
      Check([this]() {
        return !browser()
                    ->GetFeatures()
                    .side_panel_ui()
                    ->IsSidePanelEntryShowing(SidePanelEntry::Key(
                        SidePanelEntry::Id::kCustomizeChrome));
      }));
}

class NtpPromoVisualUiTest
    : public NtpPromoUiTest,
      public testing::WithParamInterface<NtpPromoUiTestParams> {
 public:
  NtpPromoVisualUiTest() {
    // TODO(crbug.com/453086432): Fix test to work with Compose enabled.
    feature_list_.InitAndDisableFeature(ntp_composebox::kNtpComposebox);
  }

 protected:
  ui::MockOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }

 private:
  ui::MockOsSettingsProvider os_settings_provider_;
  base::test::ScopedFeatureList feature_list_;
};

// Screenshot the promo UI across the available presentation styles, along
// with other variables that warrant pixel-style validation.
INSTANTIATE_TEST_SUITE_P(
    ,
    NtpPromoVisualUiTest,
    ValuesIn(std::vector<NtpPromoUiTestParams>{
        {},
        {
            .color_scheme = ui::NativeTheme::PreferredColorScheme::kDark,
        },
        {
            .rtl = true,
        },
        {
            .long_text = true,
        }}),
    [](const testing::TestParamInfo<NtpPromoUiTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(NtpPromoVisualUiTest, Screenshots) {
  // Force a consistent window size to exercise promo layout within New Tab
  // Page bounds.
  auto screen_size = gfx::Size(1000, 1200);
  BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetSize(
      screen_size);

  os_settings_provider().SetPreferredColorScheme(GetParam().color_scheme);

  if (GetParam().rtl) {
    base::i18n::SetRTLForTesting(true);
  }

  if (GetParam().long_text) {
    // Override promo text to very long (and short) strings, to exercise the
    // promos growing to fit (nor not shrinking unexpectedly).
    auto& bundle = ui::ResourceBundle::GetSharedInstance();
    bundle.OverrideLocaleStringResource(IDS_NTP_CUSTOMIZATION_PROMO,
                                        kLongPromoText);
  }

  // Use fake promos for control over exactly what is shown.
  ClearRegisteredPromos();
  RegisterTestPromo("1", Eligibility::kEligible, IDS_NTP_CUSTOMIZATION_PROMO);

  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
      WaitForAndScrollToElement(kNtpElementId, kPathToPromo),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshots not captured on this platform."),
      ScreenshotWebUi(kNtpElementId, kPathToPromo,
                      /*screenshot_name=*/std::string(),
                      /*baseline_cl=*/"7718763"));
}

class NtpPromoDisabledUiTest : public NtpPromoUiTest {
 public:
  NtpPromoDisabledUiTest() {
    feature_list_.InitAndDisableFeature(
        user_education::features::kEnableNtpBrowserPromos);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NtpPromoDisabledUiTest, NotShownMetric) {
  RunTestSequence(InstrumentTab(kNtpElementId),
                  NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
                  CheckShowMetrics(ShowNtpPromosResult::kNotShownDueToPolicy));
}
