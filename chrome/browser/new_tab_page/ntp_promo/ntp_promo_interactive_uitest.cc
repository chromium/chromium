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
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
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
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/typed_identifier.h"
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
const InteractiveBrowserTestApi::DeepQuery kPathToIndividualPromos = {
    "ntp-app", "individual-promos", "#promos"};
const InteractiveBrowserTestApi::DeepQuery kPathToFirstIndividualPromo =
    kPathToIndividualPromos + "#promo";

const InteractiveBrowserTestApi::DeepQuery kPathToSetupList = {
    "ntp-app", "setup-list-module-wrapper", "setup-list"};
const InteractiveBrowserTestApi::DeepQuery kPathToSetupListFirstItem =
    kPathToSetupList + "setup-list-item";
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
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTestPromoShownEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTestPromoClickedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMetricsRecordedEvent);

// Contains variables on which these tests may be parameterized. This approach
// makes it easy to build sets of relevant tests, vs. the brute-force
// testing::Combine() approach.
struct NtpPromoUiTestParams {
  NtpBrowserPromoType promo_type = NtpBrowserPromoType::kNone;
  std::optional<int> individual_promos = std::nullopt;
  bool long_text = false;
  ui::NativeTheme::PreferredColorScheme color_scheme =
      ui::NativeTheme::PreferredColorScheme::kLight;
  bool wide_screen = false;
  bool rtl = false;

  std::string ToString() const {
    std::ostringstream oss;
    oss << promo_type;
    if (individual_promos.has_value()) {
      oss << "_" << individual_promos.value();
    }
    if (long_text) {
      oss << "_long_text";
    }
    if (color_scheme == ui::NativeTheme::PreferredColorScheme::kDark) {
      oss << "_dark";
    }
    if (wide_screen) {
      oss << "_wide";
    }
    if (rtl) {
      oss << "_rtl";
    }

    return oss.str();
  }
};

using ObserverType =
    views::test::PollingViewPropertyObserver<std::u16string, OmniboxViewViews>;
DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(ObserverType, kLocationBarTextValue);
MATCHER_P(OptionalStringContains, text, "Optional string contains") {
  return arg.has_value() && arg.value().find(text) != std::u16string::npos;
}

}  // namespace

class NtpPromoUiTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest>,
      public testing::WithParamInterface<NtpPromoUiTestParams> {
 public:
  NtpPromoUiTest() {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
  }
  ~NtpPromoUiTest() override = default;

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    base::FieldTrialParams params;
    params[user_education::features::kNtpBrowserPromoType.name] = [=]() {
      switch (GetParam().promo_type) {
        case NtpBrowserPromoType::kSimple:
          return "simple";
        case NtpBrowserPromoType::kSetupList:
          return "setuplist";
        case NtpBrowserPromoType::kNone:
          return "none";
        default:
          NOTREACHED();
      }
    }();
    if (GetParam().individual_promos.has_value()) {
      params[user_education::features::kNtpBrowserPromoIndividualPromoLimit
                 .name] =
          base::NumberToString(GetParam().individual_promos.value());
    }
    return {{user_education::features::kEnableNtpBrowserPromos, params}};
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    // Sanity check that the flag setup actually took; if it didn't, then we
    // can't accurately perform the test.
    ASSERT_EQ(user_education::features::GetNtpBrowserPromoType(),
              GetParam().promo_type);
  }

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

    if (eligibility == Eligibility::kCompleted) {
      // Need to configure the data so that the completed promo can show.
      user_education::UserEducationStorageService& storage_service =
          service->user_education_storage_service();
      user_education::NtpPromoData data =
          storage_service.ReadNtpPromoData(id).value_or(
              user_education::NtpPromoData());
      data.last_clicked = storage_service.GetCurrentTime();
      data.completed = storage_service.GetCurrentTime();
      storage_service.SaveNtpPromoData(id, data);
    }
  }

  void InstallTestPromo(Eligibility eligibility) {
    ClearRegisteredPromos();
    RegisterTestPromo(kTestPromoName, eligibility,
                      IDS_NTP_SIGN_IN_PROMO_WITH_BOOKMARKS);
  }

  auto GetFirstPromoPath() const {
    switch (GetParam().promo_type) {
      case NtpBrowserPromoType::kSimple:
        return kPathToFirstIndividualPromo;
      case NtpBrowserPromoType::kSetupList:
        return kPathToSetupListFirstItem;
      default:
        NOTREACHED();
    }
  }

  // Returns the element containing either the individual promos, or the
  // setup list.
  auto GetPromosPath() const {
    switch (GetParam().promo_type) {
      case NtpBrowserPromoType::kSimple:
        return kPathToIndividualPromos;
      case NtpBrowserPromoType::kSetupList:
        return kPathToSetupList;
      default:
        NOTREACHED();
    }
  }

  auto WaitForAndScrollToElement(
      const ui::ElementIdentifier& ntp_id,
      const WebContentsInteractionTestUtil::DeepQuery& query) {
    auto steps = Steps(WaitForElementToRender(ntp_id, query),
                       ScrollIntoView(ntp_id, query));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto GetPromoIconPath() const { return GetFirstPromoPath() + kPromoIconId; }

  auto WaitForPromoIcon(std::string_view expected_icon) {
    const auto path = GetPromoIconPath();
    auto steps = Steps(
        WaitForAndScrollToElement(kNtpElementId, path),
        // Verify the icon shows the correct image.
        CheckJsResultAt(kNtpElementId, path, "el => el.icon", expected_icon));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto WaitForPromoVisible(Eligibility eligibility,
                           std::string_view expected_icon) {
    MultiStep steps;
    switch (eligibility) {
      case Eligibility::kEligible:
        steps += WaitForPromoIcon(std::string("ntp-promo:") +
                                  std::string(expected_icon));
        break;
      case Eligibility::kCompleted:
        steps += WaitForPromoIcon("cr:check");
        break;
      case Eligibility::kIneligible:
        NOTREACHED();
    }
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto VerifyTestPromoText() {
    return CheckJsResultAt(
               kNtpElementId, GetFirstPromoPath() + kPromoTextId,
               "el => el.innerText",
               l10n_util::GetStringUTF8(IDS_NTP_SIGN_IN_PROMO_WITH_BOOKMARKS))
        .AddDescriptionPrefix(__func__);
  }

  auto ClickPromo() {
    auto steps = ClickElement(kNtpElementId, GetPromoIconPath());
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

  // Returns the expected UTM extension that should be supplied with the web
  // store URL when invoking the extensions promo.
  std::string_view ExpectedExtensionUtmSource() {
    switch (GetParam().promo_type) {
      case user_education::features::NtpBrowserPromoType::kSimple:
        if (user_education::features::GetNtpBrowserPromoIndividualPromoLimit() >
            1) {
          return extension_urls::kNtpPromo2pUtmSource;
        } else {
          return extension_urls::kNtpPromo1pUtmSource;
        }
      case user_education::features::NtpBrowserPromoType::kSetupList:
        return extension_urls::kNtpPromoSlUtmSource;
      default:
        NOTREACHED();
    }
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

INSTANTIATE_TEST_SUITE_P(
    ,
    NtpPromoUiTest,
    ValuesIn(std::vector<NtpPromoUiTestParams>{
        {.promo_type = NtpBrowserPromoType::kSimple},
        {.promo_type = NtpBrowserPromoType::kSetupList}}),
    [](const testing::TestParamInfo<NtpPromoUiTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest, TestPromoEligible) {
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
      WaitForPromoVisible(Eligibility::kEligible, kSignInIconName),
      VerifyTestPromoText(),
      // As before, because the click and the event are sent asynchronously,
      // run these in parallel.
      InParallel(RunSubsequence(ClickPromo()),
                 RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                             kTestPromoClickedEvent))),
      // Verify that the correct histogram was recorded.
      CheckShowMetrics(ShowNtpPromosResult::kShown));
}

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest, TestPromoCompleted) {
  InstallTestPromo(Eligibility::kCompleted);
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
      If(
          []() {
            return GetParam().promo_type == NtpBrowserPromoType::kSetupList;
          },
          Then(WaitForPromoVisible(Eligibility::kCompleted, kSignInIconName),
               VerifyTestPromoText(),
               CheckShowMetrics(ShowNtpPromosResult::kShown)),
          Else(EnsureNotPresent(kNtpElementId, GetFirstPromoPath()),
               CheckShowMetrics(ShowNtpPromosResult::kNotShownNoPromos))));
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

INSTANTIATE_TEST_SUITE_P(
    ,
    NtpPromoWithModuleUiTest,
    ValuesIn(std::vector<NtpPromoUiTestParams>{
        {.promo_type = NtpBrowserPromoType::kSimple},
        {.promo_type = NtpBrowserPromoType::kSetupList}}),
    [](const testing::TestParamInfo<NtpPromoUiTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(NtpPromoWithModuleUiTest, ModuleEnabled) {
  InstallTestPromo(Eligibility::kEligible);
  RunTestSequence(InstrumentTab(kNtpElementId),
                  NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
                  WaitForAndScrollToElement(kNtpElementId, kPathToModules),
                  EnsureNotPresent(kNtpElementId, GetFirstPromoPath()),
                  CheckShowMetrics(ShowNtpPromosResult::kNotShownDueToPolicy));
}

IN_PROC_BROWSER_TEST_P(NtpPromoWithModuleUiTest, ModuleDisabled) {
  // Disable the Tab Groups module in prefs.
  {
    ScopedListPrefUpdate update(browser()->profile()->GetPrefs(),
                                prefs::kNtpDisabledModules);
    base::Value::List& list = update.Get();
    base::Value module_id_value(ntp_modules::kTabGroupsModuleId);
    list.Append(std::move(module_id_value));
  }
  InstallTestPromo(Eligibility::kEligible);
  RunTestSequence(InstrumentTab(kNtpElementId),
                  NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
                  WaitForAndScrollToElement(kNtpElementId, GetFirstPromoPath()),
                  EnsureNotVisible(kNtpElementId, kPathToModules),
                  CheckShowMetrics(ShowNtpPromosResult::kShown));
}

// Tests in this block rely on the fact that the top priority promotion is
// signin - except on ChromeOS, where there is no signin flow. So do not build
// or run these tests on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest, SigninPromoAppearsAndIsClickable) {
  ClearRegisteredPromosExcept(kNtpSignInPromoId);
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
      WaitForPromoVisible(Eligibility::kEligible, kSignInIconName),

      // Since bots cannot navigate to actual pages, we can't use
      // WaitForWebContentsNavigation() or the like. Instead, verify that the
      // browser *tries* to navigate to the account login page.
      PollViewProperty(kLocationBarTextValue, kOmniboxElementId,
                       &OmniboxViewViews::GetText),
      // Click the promo button; this should navigate the current page.
      ClickPromo(),
      WaitForState(kLocationBarTextValue,
                   OptionalStringContains(u"accounts.google.com")),
      // The NTP tab should navigate, rather than opening a new tab.
      CheckOneTabOpen());

  // TODD(https://crbug.com/433607240): Check model, histograms.
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest, ExtensionsPromoAppearsAndIsClickable) {
  ClearRegisteredPromosExcept(kNtpExtensionsPromoId);
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
      WaitForPromoVisible(Eligibility::kEligible, kExtensionsIconName),

      // Since bots cannot navigate to actual pages, we can't use
      // WaitForWebContentsNavigation() or the like. Instead, verify that the
      // browser *tries* to navigate to the account login page.
      PollViewProperty(kLocationBarTextValue, kOmniboxElementId,
                       &OmniboxViewViews::GetText),
      // Click the promo button; this should navigate the current page.
      ClickPromo(),
      // Note that the URL here may not match what users see, due to redirects.
      WaitForState(kLocationBarTextValue,
                   ::testing::AllOf(OptionalStringContains(u"webstore"),
                                    OptionalStringContains(base::UTF8ToUTF16(
                                        ExpectedExtensionUtmSource())))),
      // The NTP tab should navigate, rather than opening a new tab.
      CheckOneTabOpen());

  // TODD(https://crbug.com/433607240): Check model, histograms.
}

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest,
                       CustomizationPromoAppearsAndIsClickable) {
  ClearRegisteredPromosExcept(kNtpCustomizationPromoId);
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
      WaitForPromoVisible(Eligibility::kEligible, kCustomizationIconName),
      ClickPromo(), WaitForShow(kSidePanelElementId));

  // TODD(https://crbug.com/433607240): Check model, histograms.
}

class NtpPromoVisualUiTest : public NtpPromoUiTest {
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
        {
            .promo_type = NtpBrowserPromoType::kSimple,
        },
        {
            .promo_type = NtpBrowserPromoType::kSimple,
            .color_scheme = ui::NativeTheme::PreferredColorScheme::kDark,
        },
        {
            .promo_type = NtpBrowserPromoType::kSimple,
            .rtl = true,
        },
        {
            .promo_type = NtpBrowserPromoType::kSimple,
            .individual_promos = 2,
        },
        {
            // Tests that the individual promos match in height, despite
            // lengthy text in one of the promos.
            .promo_type = NtpBrowserPromoType::kSimple,
            .individual_promos = 2,
            .long_text = true,
        },
        {
            // Tests that the promos sit side-by-side.
            .promo_type = NtpBrowserPromoType::kSimple,
            .individual_promos = 2,
            .wide_screen = true,
        },
        {
            .promo_type = NtpBrowserPromoType::kSetupList,
        },
        {
            .promo_type = NtpBrowserPromoType::kSetupList,
            .long_text = true,
        },
        {
            .promo_type = NtpBrowserPromoType::kSetupList,
            .color_scheme = ui::NativeTheme::PreferredColorScheme::kDark,
        },
        {
            .promo_type = NtpBrowserPromoType::kSetupList,
            .rtl = true,
        }}),
    [](const testing::TestParamInfo<NtpPromoUiTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(NtpPromoVisualUiTest, Screenshots) {
  // Force a consistent window size to exercise promo layout within New Tab
  // Page bounds.
  auto screen_size = gfx::Size(1000, 1200);
  if (GetParam().wide_screen) {
    // Grow the screen wide enough that individual promos can sit side-by-side.
    screen_size = gfx::Size(1500, 1200);
  }
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
    bundle.OverrideLocaleStringResource(IDS_NTP_EXTENSIONS_PROMO,
                                        kShortPromoText);
  }

  // Use fake promos to exercise pending/completed state and short/long text.
  ClearRegisteredPromos();
  RegisterTestPromo("1", Eligibility::kEligible,
                    IDS_NTP_SIGN_IN_PROMO_WITH_BOOKMARKS);
  RegisterTestPromo("2", Eligibility::kEligible, IDS_NTP_CUSTOMIZATION_PROMO);
  RegisterTestPromo("3", Eligibility::kCompleted, IDS_NTP_EXTENSIONS_PROMO);

  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
      WaitForAndScrollToElement(kNtpElementId, GetFirstPromoPath()),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Screenshots not captured on this platform."),
      ScreenshotWebUi(kNtpElementId, GetPromosPath(),
                      /*screenshot_name=*/std::string(),
                      /*baseline_cl=*/"6998053"));
}

class NtpPromosDisabledUiTest : public NtpPromoUiTest {};

INSTANTIATE_TEST_SUITE_P(
    ,
    NtpPromosDisabledUiTest,
    ValuesIn(std::vector<NtpPromoUiTestParams>{
        {.promo_type = NtpBrowserPromoType::kNone}}),
    [](const testing::TestParamInfo<NtpPromoUiTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(NtpPromosDisabledUiTest, NotShownMetric) {
  RunTestSequence(InstrumentTab(kNtpElementId),
                  NavigateWebContents(kNtpElementId, GURL(kNtpURL)),
                  CheckShowMetrics(ShowNtpPromosResult::kNotShownDueToPolicy));
}
