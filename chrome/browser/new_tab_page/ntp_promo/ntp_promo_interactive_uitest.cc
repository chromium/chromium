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
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"
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
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/typed_identifier.h"
#include "ui/base/l10n/l10n_util.h"
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
constexpr char kActionIconId[] = "#actionIcon";
constexpr char kPromoTextId[] = "#bodyText";
constexpr char kPromoIconId[] = "#bodyIcon";
constexpr char kIconName[] = "account_circle";
constexpr int kLongSampleTextIds = IDS_NTP_SIGN_IN_PROMO;
constexpr int kShortSampleTextIds = IDS_NTP_SIGN_IN_PROMO_ACTION_BUTTON;

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

}  // namespace

class NtpPromoUiTest
    : public InteractiveBrowserTest,
      public testing::WithParamInterface<NtpPromoUiTestParams> {
 public:
  NtpPromoUiTest() = default;
  ~NtpPromoUiTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(GetFeatures(), {});
    InteractiveBrowserTest::SetUp();
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetFeatures() {
    base::FieldTrialParams params;
    params[user_education::features::kNtpBrowserPromoType.name] = [=]() {
      switch (GetParam().promo_type) {
        case NtpBrowserPromoType::kSimple:
          return "simple";
        case NtpBrowserPromoType::kSetupList:
          return "setuplist";
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

  void RegisterTestPromo(const user_education::NtpPromoIdentifier& id,
                         Eligibility eligibility,
                         int text_id) {
    UserEducationService* const service =
        UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
    user_education::NtpPromoRegistry* registry = service->ntp_promo_registry();
    user_education::NtpPromoSpecification spec(
        id,
        user_education::NtpPromoContent(kIconName, text_id,
                                        IDS_NTP_SIGN_IN_PROMO_ACTION_BUTTON),
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
    RegisterTestPromo(kTestPromoName, eligibility, IDS_NTP_SIGN_IN_PROMO);
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

  auto GetActionButtonPath() const {
    return GetFirstPromoPath() + kActionIconId;
  }

  auto GetPromoIconPath() const { return GetFirstPromoPath() + kPromoIconId; }

  auto WaitForPromoIcon(std::string_view expected_icon) {
    const auto path = GetPromoIconPath();
    auto steps = Steps(
        WaitForElementVisible(kNtpElementId, path),
        // Verify the icon shows the correct image.
        CheckJsResultAt(kNtpElementId, path, "el => el.icon", expected_icon));
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto WaitForPromoVisible(Eligibility eligibility) {
    MultiStep steps;
    switch (eligibility) {
      case Eligibility::kEligible:
        steps += WaitForPromoIcon(std::string("ntp-promo:") + kIconName);
        steps += WaitForElementVisible(kNtpElementId, GetActionButtonPath());
        break;
      case Eligibility::kCompleted:
        steps += WaitForPromoIcon("cr:check");
        if (GetParam().promo_type == NtpBrowserPromoType::kSimple) {
          steps += EnsureNotVisible(kNtpElementId, GetActionButtonPath());
        } else {
          steps += WaitForElementVisible(kNtpElementId, GetActionButtonPath());
        }
        break;
      case Eligibility::kIneligible:
        NOTREACHED();
    }
    AddDescriptionPrefix(steps, __func__);
    return steps;
  }

  auto VerifyTestPromoText() {
    return CheckJsResultAt(kNtpElementId, GetFirstPromoPath() + kPromoTextId,
                           "el => el.innerText",
                           l10n_util::GetStringUTF8(IDS_NTP_SIGN_IN_PROMO))
        .AddDescriptionPrefix(__func__);
  }

  auto PressActionButton() {
    return ClickElement(kNtpElementId, GetActionButtonPath())
        .AddDescriptionPrefix(__func__);
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
      InParallel(RunSubsequence(NavigateWebContents(
                     kNtpElementId, GURL(chrome::kChromeUINewTabPageURL))),
                 RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                             kTestPromoShownEvent))),
      // Should already be visible at this point, but confirm it is and that it
      // is in the correct state.
      WaitForPromoVisible(Eligibility::kEligible), VerifyTestPromoText(),
      // As before, because the click and the event are sent asynchronously,
      // run these in parallel.
      InParallel(RunSubsequence(PressActionButton()),
                 RunSubsequence(WaitForEvent(kBrowserViewElementId,
                                             kTestPromoClickedEvent))),
      // Verify that the correct histogram was recorded.
      CheckShowMetrics(ShowNtpPromosResult::kShown));
}

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest, TestPromoCompleted) {
  InstallTestPromo(Eligibility::kCompleted);
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(chrome::kChromeUINewTabPageURL)),
      If(
          []() {
            return GetParam().promo_type == NtpBrowserPromoType::kSetupList;
          },
          Then(WaitForPromoVisible(Eligibility::kCompleted),
               VerifyTestPromoText(),
               CheckShowMetrics(ShowNtpPromosResult::kShown)),
          Else(EnsureNotVisible(kNtpElementId, GetFirstPromoPath()),
               CheckShowMetrics(ShowNtpPromosResult::kNotShownNoPromos))));
}

namespace {
const InteractiveBrowserTestApi::DeepQuery kPathToModules = {"ntp-app",
                                                             "ntp-modules"};
}  // namespace

class NtpPromoWithModuleUiTest : public NtpPromoUiTest {
 protected:
  std::vector<base::test::FeatureRefAndParams> GetFeatures() override {
    auto result = NtpPromoUiTest::GetFeatures();
    result.push_back(
        {ntp_features::kNtpTabGroupsModule,
         {{ntp_features::kNtpTabGroupsModuleDataParam, "Fake Data"}}});
    return result;
  }
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
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(chrome::kChromeUINewTabPageURL)),
      WaitForElementVisible(kNtpElementId, kPathToModules),
      EnsureNotVisible(kNtpElementId, GetFirstPromoPath()),
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
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(chrome::kChromeUINewTabPageURL)),
      WaitForElementVisible(kNtpElementId, GetFirstPromoPath()),
      EnsureNotVisible(kNtpElementId, kPathToModules),
      CheckShowMetrics(ShowNtpPromosResult::kShown));
}

// Tests in this block rely on the fact that the top priority promotion is
// signin - except on ChromeOS, where there is no signin flow. So do not build
// or run these tests on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)

namespace {

using ObserverType =
    views::test::PollingViewPropertyObserver<std::u16string, OmniboxViewViews>;
DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(ObserverType, kLocationBarTextValue);
MATCHER_P(OptionalStringContains, text, "Optional string contains") {
  return arg.has_value() && arg.value().find(text) != std::u16string::npos;
}

}  // namespace

IN_PROC_BROWSER_TEST_P(NtpPromoUiTest, SigninPromoAppearsAndIsClickable) {
  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(chrome::kChromeUINewTabPageURL)),
      WaitForPromoVisible(Eligibility::kEligible),

      // Since bots cannot navigate to actual pages, we can't use
      // WaitForWebContentsNavigation() or the like. Instead, verify that the
      // browser *tries* to navigate to the account login page.
      PollViewProperty(kLocationBarTextValue, kOmniboxElementId,
                       &OmniboxViewViews::GetText),
      // Click the promo button; this should navigate the current page.
      PressActionButton(),
      WaitForState(kLocationBarTextValue,
                   OptionalStringContains(u"accounts.google.com")));

  // TODD(https://crbug.com/433607240): Check model, histograms.
}

#endif

class NtpPromoVisualUiTest : public NtpPromoUiTest {};

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

  ui::NativeTheme::GetInstanceForNativeUi()->set_preferred_color_scheme(
      GetParam().color_scheme);

  if (GetParam().rtl) {
    base::i18n::SetRTLForTesting(true);
  }

  // Use fake promos to exercise pending/completed state and short/long text.
  ClearRegisteredPromos();
  RegisterTestPromo("1", Eligibility::kEligible, kLongSampleTextIds);
  RegisterTestPromo("2", Eligibility::kEligible, kShortSampleTextIds);
  RegisterTestPromo("3", Eligibility::kCompleted, kLongSampleTextIds);

  RunTestSequence(
      InstrumentTab(kNtpElementId),
      NavigateWebContents(kNtpElementId, GURL(chrome::kChromeUINewTabPageURL)),
      WaitForElementVisible(kNtpElementId, GetFirstPromoPath()),
      ScrollIntoView(kNtpElementId, GetPromosPath()),
      SetOnIncompatibleAction(
        OnIncompatibleAction::kSkipTest,
                              "Screenshots not captured on this platform."),
      ScreenshotWebUi(kNtpElementId, GetPromosPath(),
                      /*screenshot_name=*/std::string(),
                      /*baseline_cl=*/"6896001"));
}
