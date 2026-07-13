// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider_manager.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace glic {
namespace {

class GlicBrowserTest : public InProcessBrowserTest {
 public:
  GlicBrowserTest() = default;
  GlicBrowserTest(const GlicBrowserTest&) = delete;
  GlicBrowserTest& operator=(const GlicBrowserTest&) = delete;

  ~GlicBrowserTest() override = default;

  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Load blank page in glic guest view
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL, "about:blank");
  }

 protected:
  virtual void InitializeFeatureList() {
  }

 private:
  GlicTestEnvironment glic_test_environment_{{.fre_status = std::nullopt}};
};

class SharedGlicBrowserTest : public glic::GlicBrowserTest {
 public:
  // Set up the Glic UI for testing. This runs after the browser is launched.
  void SetUpOnMainThread() override {
    glic::GlicBrowserTest::SetUpOnMainThread();
    auto* profile = GetProfile();
    SetFRECompletion(profile, prefs::FreStatus::kCompleted);

    ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
    web_contents_ = instance->host().webui_contents();
    ASSERT_TRUE(web_contents_);
    ASSERT_TRUE(WaitForWebUiState(mojom::WebUiState::kReady).has_value());
  }
  // Clear the raw_ptr before the test environment destroys the WebContents,
  // preventing a dangling pointer error.
  void TearDownOnMainThread() override {
    web_contents_ = nullptr;
    glic::GlicBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

// Ensure basic incognito window doesn't cause a crash. Simply opens an
// incognito window and navigates, test passes if it doesn't crash.
IN_PROC_BROWSER_TEST_F(GlicBrowserTest, IncognitoModeCrash) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(incognito_browser, GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(GlicBrowserTest, PausedProfileIsNotReady) {
  // Signin and check that Glic is enabled.
  auto* profile = browser()->profile();
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));
  SetFRECompletion(profile, prefs::FreStatus::kCompleted);
  ASSERT_TRUE(GlicEnabling::IsReadyForProfile(profile));

  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));
  ASSERT_FALSE(GlicEnabling::IsReadyForProfile(profile));
}

IN_PROC_BROWSER_TEST_F(GlicBrowserTest, GlicEnablingDismissed) {
  // Signin and check that Glic is enabled.
  auto* profile = browser()->profile();

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));

  // False until FRE is shown.
  ASSERT_FALSE(GlicEnabling::DidDismissForProfile(profile));

  // Simulate user shown FRE and dismissed.
  SetFRECompletion(profile, prefs::FreStatus::kIncomplete);
  ASSERT_TRUE(GlicEnabling::DidDismissForProfile(profile));

  // Simulate user shown FRE again and accepted.
  SetFRECompletion(profile, prefs::FreStatus::kCompleted);
  ASSERT_FALSE(GlicEnabling::DidDismissForProfile(profile));
}

IN_PROC_BROWSER_TEST_F(GlicBrowserTest, InvokeFailsWhenProfileNotEnabled) {
  auto* profile = browser()->profile();
  ScopedGlicCapability scoped_glic_capability(profile, false);
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile));

  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  ASSERT_TRUE(glic_service);

  bool error_called = false;
  GlicInvokeError error_received;

  base::RunLoop run_loop;

  GlicInvokeOptions options(mojom::InvocationSource::kTopChromeButton);
  options.on_error = base::BindLambdaForTesting([&](GlicInvokeError error) {
    error_called = true;
    error_received = error;
    run_loop.Quit();
  });

  auto instance = glic_service->Invoke(std::move(options));
  EXPECT_FALSE(instance);

  // on_error is posted asynchronously, so we wait for it
  run_loop.Run();

  EXPECT_TRUE(error_called);
  EXPECT_EQ(error_received, GlicInvokeError::kProfileNotEnabled);
}

// This test verifies that focus is correctly trapped and forwarded to the
// guest panel's webview when the guest panel is visible.
// This simulates the behavior expected by the focus listener in
// glic_app_controller.ts.
IN_PROC_BROWSER_TEST_F(SharedGlicBrowserTest, FocusTrappingGuestPanel) {
  // Execute script to simulate focus trapping.
  std::string script = R"(
    (() => {
      // Simulate focus on body
      document.body.focus();
      // Trigger the focus event listener
      window.dispatchEvent(new Event('focus'));
      const webview = document.querySelector('#webviewContainer webview');
      if (document.activeElement === webview) {
        return 'success';
      }
      return 'activeElement is ' + document.activeElement.tagName;
    })()
  )";

  EXPECT_EQ("success", content::EvalJs(web_contents_, script));
}

class GlicKeyedServiceSyncBrowserTest : public GlicBrowserTest {
 public:
  GlicKeyedServiceSyncBrowserTest() = default;
  ~GlicKeyedServiceSyncBrowserTest() override = default;

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    platform_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForPlatform(),
            policy::EnterpriseManagementAuthority::NONE);
    profile_management_override_ =
        std::make_unique<policy::ScopedManagementServiceOverrideForTesting>(
            policy::ManagementServiceFactory::GetForProfile(profile()),
            policy::EnterpriseManagementAuthority::NONE);
  }

  void TearDownOnMainThread() override {
    platform_management_override_.reset();
    profile_management_override_.reset();
    GlicBrowserTest::TearDownOnMainThread();
  }

  void InitializeFeatureList() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicExperimentalTriggering);
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&GlicKeyedServiceSyncBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
    GlicBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::FakeDeviceInfoSyncService>();
        }));
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }

  Profile* profile() { return browser()->profile(); }

  syncer::FakeDeviceInfoSyncService* fake_device_info_sync_service() {
    return static_cast<syncer::FakeDeviceInfoSyncService*>(
        DeviceInfoSyncServiceFactory::GetForProfile(profile()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      platform_management_override_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      profile_management_override_;
};

IN_PROC_BROWSER_TEST_F(GlicKeyedServiceSyncBrowserTest,
                       PrefChangesTriggerSyncRefresh) {
  // Ensure glic is enabled and allowed.
  SetGlicCapability(profile(), true);
  // Start with FRE incomplete (default).
  SetFRECompletion(profile(), prefs::FreStatus::kIncomplete);

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile());
  ASSERT_TRUE(glic_service);
  glic::GlicEnabling& enabling = glic_service->enabling();

  // Initially, state should be kNeedsOptIn. One refresh has been triggered
  // during startup due to the initial transition from kUnavailable.
  ASSERT_EQ(fake_device_info_sync_service()->RefreshLocalDeviceInfoCount(), 1);

  // 1. Set UserEnabledActuationOnWeb to true -> should NOT trigger refresh
  // (state remains kNeedsOptIn because FRE is incomplete and triggering is
  // false).
  enabling.SetUserEnabledActuationOnWeb(true);
  EXPECT_EQ(fake_device_info_sync_service()->RefreshLocalDeviceInfoCount(), 1);

  // 2. Set ExperimentalTriggeringEnabled to true -> should NOT trigger refresh
  // (state remains kNeedsOptIn because FRE is still incomplete).
  enabling.SetExperimentalTriggeringEnabled(true);
  EXPECT_EQ(fake_device_info_sync_service()->RefreshLocalDeviceInfoCount(), 1);

  // 3. Set FRE to completed -> should trigger refresh (state becomes kReady).
  SetFRECompletion(profile(), prefs::FreStatus::kCompleted);
  EXPECT_EQ(fake_device_info_sync_service()->RefreshLocalDeviceInfoCount(), 2);

  // 4. Toggle ExperimentalTriggeringEnabled to false -> should trigger refresh
  // (state becomes kNeedsOptIn).
  enabling.SetExperimentalTriggeringEnabled(false);
  EXPECT_EQ(fake_device_info_sync_service()->RefreshLocalDeviceInfoCount(), 3);

  // 5. Toggle it back to true -> should trigger refresh (state becomes kReady).
  enabling.SetExperimentalTriggeringEnabled(true);
  EXPECT_EQ(fake_device_info_sync_service()->RefreshLocalDeviceInfoCount(), 4);
}

class GlicWidgetThemeBrowserTest : public glic::GlicBrowserTest {
 public:
  GlicWidgetThemeBrowserTest() = default;
  ~GlicWidgetThemeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    glic::GlicBrowserTest::SetUpOnMainThread();
    SetFRECompletion(GetProfile(), prefs::FreStatus::kCompleted);
  }
};

IN_PROC_BROWSER_TEST_F(GlicWidgetThemeBrowserTest,
                       InitialThemePropagationOnCreation) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(GetProfile());

  auto initial_color_scheme = theme_service->GetBrowserColorScheme();

  // 1. Set the profile color scheme to Light.
  theme_service->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);

  // 2. Verify that GlicWidget created with Light color scheme is light.
  {
    auto glic_view =
        std::make_unique<GlicView>(GetProfile(), gfx::Size(100, 100), nullptr);
    auto delegate =
        GlicWidget::CreateWidgetDelegate(std::move(glic_view), false);
    std::unique_ptr<GlicWidget> widget = GlicWidget::Create(
        delegate.get(), GetProfile(), gfx::Rect(0, 0, 100, 100), false);
    ASSERT_TRUE(widget);

    GlicView* view = widget->GetGlicView();
    ASSERT_TRUE(view);
    ASSERT_TRUE(view->layer());

    ui::ColorProviderKey key = widget->GetColorProviderKeyForTesting();
    key.color_mode = ui::ColorProviderKey::ColorMode::kLight;
    const ui::ColorProvider* light_provider =
        ui::ColorProviderManager::Get().GetColorProviderFor(key);
    EXPECT_EQ(light_provider->GetColor(kColorGlicBackground),
              view->layer()->AsSolidColor()->background_color());
  }

  // 3. Set the profile color scheme to Dark.
  theme_service->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kDark);

  // 4. Create the GlicWidget directly without showing it.
  {
    auto glic_view =
        std::make_unique<GlicView>(GetProfile(), gfx::Size(100, 100), nullptr);
    auto delegate =
        GlicWidget::CreateWidgetDelegate(std::move(glic_view), false);
    std::unique_ptr<GlicWidget> widget = GlicWidget::Create(
        delegate.get(), GetProfile(), gfx::Rect(0, 0, 100, 100), false);
    ASSERT_TRUE(widget);

    // 5. Get GlicView.
    GlicView* view = widget->GetGlicView();
    ASSERT_TRUE(view);
    ASSERT_TRUE(view->layer());

    // 6. Verify that GlicView's background color is in dark mode.
    ui::ColorProviderKey key = widget->GetColorProviderKeyForTesting();
    key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
    const ui::ColorProvider* dark_provider =
        ui::ColorProviderManager::Get().GetColorProviderFor(key);
    EXPECT_EQ(dark_provider->GetColor(kColorGlicBackground),
              view->layer()->AsSolidColor()->background_color());
  }

  // Restore initial color scheme.
  theme_service->SetBrowserColorScheme(initial_color_scheme);
}

}  // namespace
}  // namespace glic
