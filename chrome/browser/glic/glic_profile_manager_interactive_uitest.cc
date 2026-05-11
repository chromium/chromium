// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

namespace glic {

// TODO(b/445924847): This test does not work with multi-instance, which is now
// launched. It needs updated if we want to pursue launching the GlicWarming
// feature.
class DISABLED_GlicProfileManagerUiTest : public test::InteractiveGlicTest {
 public:
  DISABLED_GlicProfileManagerUiTest() {
    std::vector<base::test::FeatureRefAndParams> enabled = {
        {features::kGlicWarming,
         {{features::kGlicWarmingDelayMs.name, "0"},
          {features::kGlicWarmingJitterMs.name, "0"}}}};
    feature_list_.InitWithFeaturesAndParameters(enabled, {});
  }

  void SetUp() override {
    // This will temporarily disable preloading to ensure that we don't load the
    // web client before we've initialized the embedded test server and can set
    // the correct URL.
    GlicProfileManager::SetPrewarmingEnabledForTesting(false);
    GlicProfileManager::ForceConnectionTypeForTesting(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET);
    fre_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));
    ASSERT_TRUE(fre_server_.Start());
    fre_url_ = fre_server_.GetURL("/glic/test_client/fre.html");
    test::InteractiveGlicTest::SetUp();
  }

  void TearDown() override {
    test::InteractiveGlicTest::TearDown();
    GlicProfileManager::SetPrewarmingEnabledForTesting(true);
    GlicProfileManager::ForceConnectionTypeForTesting(std::nullopt);
  }

  void SetUpOnMainThread() override {
    test::InteractiveGlicTest::SetUpOnMainThread();
    auto* profile_manager = g_browser_process->profile_manager();
    second_profile_path_ = profile_manager->GenerateNextProfileDirectoryPath();
    profiles::testing::CreateProfileSync(profile_manager, second_profile_path_);
    SetFRECompletion(GetSecondProfile(), prefs::FreStatus::kCompleted);
  }

  Profile* GetSecondProfile() {
    return g_browser_process->profile_manager()->GetProfile(
        second_profile_path_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kGlicFreURL, fre_url_.spec());
  }

  void TearDownOnMainThread() override {
    test::InteractiveGlicTest::TearDownOnMainThread();
  }

  GlicKeyedService* GetService(bool primary) {
    return GlicKeyedServiceFactory::GetGlicKeyedService(
        primary ? browser()->profile() : GetSecondProfile());
  }

  auto CreateAndWarmGlic(bool primary_profile) {
    return Do([primary_profile, this]() {
      GetService(primary_profile)->TryPreload();
    });
  }

  std::string WarmedAndSizedStatus(GlicKeyedService* service) {
    const bool warmed = GetInstanceCoordinator()
                            .GetWebContentsWarmingPoolForTesting()
                            .HasWarmedContainerForTesting() ||
                        service->instance_coordinator().IsAnyPanelShowing();
    if (!warmed) {
      return "Not warmed";
    }
    if (GetInstanceCoordinator()
            .GetWebContentsWarmingPoolForTesting()
            .HasWarmedContainerForTesting()) {
      return "warmed and sized";
    }
    auto* instance = service->GetInstanceForActiveTab(nullptr);
    if (!instance) {
      return "No instance";
    }
    if (instance->GetPanelSize().IsEmpty()) {
      return "Size is empty";
    }
    return "warmed and sized";
  }

  auto CheckWarmedAndSized(bool primary_warmed, bool secondary_warmed) {
    return Steps(
        WaitUntil(
            [this, primary_warmed]() {
              bool actual =
                  WarmedAndSizedStatus(GetService(true)) == "warmed and sized";
              return actual == primary_warmed ? "ok" : "waiting for primary";
            },
            "ok", "Wait for primary warmed state"),
        WaitUntil(
            [this, secondary_warmed]() {
              bool actual =
                  WarmedAndSizedStatus(GetService(false)) == "warmed and sized";
              return actual == secondary_warmed ? "ok"
                                                : "waiting for secondary";
            },
            "ok", "Wait for secondary warmed state"));
  }

  auto ResetPreloading() {
    return Do(
        []() { GlicProfileManager::SetPrewarmingEnabledForTesting(true); });
  }

  auto CacheClientContents(bool) {
    return Do([this]() { web_client_contents_ = GetHost()->webui_contents(); });
  }

  auto CheckCachedClientContents(bool) {
    return Do([this]() {
      EXPECT_EQ(web_client_contents_, GetHost()->webui_contents());
      EXPECT_NE(nullptr, GetHost()->webui_contents());
      web_client_contents_ = nullptr;
    });
  }

  auto SendMemoryPressureSignal() {
    return Do([]() {
      base::MemoryPressureListener::SimulatePressureNotification(
          base::MEMORY_PRESSURE_LEVEL_CRITICAL);
    });
  }

  GlicFreController* GetFreController(bool primary_profile) {
    return &GetService(primary_profile)->fre_controller();
  }
  GlicTestEnvironmentService& GetTestEnvForSecondProfile() {
    return *glic::GlicTestEnvironment::GetService(GetSecondProfile());
  }

 private:
  base::FilePath second_profile_path_;
  net::EmbeddedTestServer fre_server_;
  raw_ptr<content::WebContents> web_client_contents_ = nullptr;
  GURL fre_url_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DISABLED_GlicProfileManagerUiTest, ConsistentPreload) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've disabled preloading, nothing should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Enable preloading again.
      ResetPreloading(), Do([this]() {
        SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
      }),
      // Attempt to preload for the primary profile.
      CreateAndWarmGlic(/*primary_profile=*/true),
      // Since there is no contention, this should have succeeded
      // (and we should not have attempted to warm the other web
      // client, so it should not yet be warmed).
      CheckWarmedAndSized(true, false),
      // This stores a pointer to the web client contents so that we can check
      // that the shown contents match (otherwise, we've warmed for no
      // reason).
      CacheClientContents(/*primary_profile=*/true),
      DeprecatedOpenGlicWindow(GlicWindowMode::kAttached),
      // Check that the client contents are the same as when warmed.
      CheckCachedClientContents(/*primary_profile=*/true));
}

IN_PROC_BROWSER_TEST_F(DISABLED_GlicProfileManagerUiTest, PreloadMutex) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've disabled preloading, nothing should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Re-enable preloading.
      ResetPreloading(), Do([this]() {
        SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
      }),
      // Attempt to preload for the primary profile.
      CreateAndWarmGlic(/*primary_profile=*/true),
      // Since there is no contention, this should have succeeded
      // (and we should not have attempted to warm the other web
      // client, so it should not yet be warmed).
      CheckWarmedAndSized(true, false),
      // Warming the secondary profile will cause another web client
      // to come into existence.
      CreateAndWarmGlic(/*primary_profile=*/false),
      // The first service should remain warmed.
      CheckWarmedAndSized(true, true));
}

IN_PROC_BROWSER_TEST_F(DISABLED_GlicProfileManagerUiTest, ShowMutex) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've disabled preloading, nothing should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Re-enable preloading.
      ResetPreloading(),
      // Attempt to preload for the secondary profile.
      CreateAndWarmGlic(/*primary_profile=*/false),
      // Since there is no contention, this should have succeeded
      // (and we should not have attempted to warm the other web
      // client, so it should not yet be warmed).
      CheckWarmedAndSized(false, true),
      // Set primary profile to completed so it can be warmed.
      Do([this]() {
        SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
      }),
      DeprecatedOpenGlicWindow(GlicWindowMode::kAttached),
      // The first service should remain warmed.
      CheckWarmedAndSized(true, true));
}

IN_PROC_BROWSER_TEST_F(DISABLED_GlicProfileManagerUiTest, FreMutex) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've disabled preloading, nothing should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Re-enable preloading.
      ResetPreloading(),
      // Attempt to preload for the secondary profile.
      CreateAndWarmGlic(/*primary_profile=*/false),
      // Since there is no contention, this should have succeeded
      // (and we should not have attempted to warm the other web
      // client, so it should not yet be warmed).
      CheckWarmedAndSized(false, true),
      // Set primary profile to completed so it can be warmed.
      Do([this]() {
        SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
      }),
      ToggleGlicWindow(GlicWindowMode::kAttached),
      // The first service should remain warmed.
      CheckWarmedAndSized(true, true));
}

IN_PROC_BROWSER_TEST_F(DISABLED_GlicProfileManagerUiTest,
                       DoNotWarmWhenShowing) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've disabled preloading, nothing should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Re-enable preloading.
      ResetPreloading(),
      // Set primary profile to completed so it can be warmed.
      Do([this]() {
        SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
      }),
      DeprecatedOpenGlicWindow(GlicWindowMode::kAttached),
      CheckWarmedAndSized(true, false),
      // Attempt to preload for the secondary profile.
      CreateAndWarmGlic(/*primary_profile=*/false),
      // We should warm the secondary profile even if the primary one is
      // showing.
      CheckWarmedAndSized(true, true));
}

IN_PROC_BROWSER_TEST_F(DISABLED_GlicProfileManagerUiTest,
                       MemPressureClearsCache) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've disabled preloading, nothing should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Re-enable preloading.
      ResetPreloading(), Do([this]() {
        SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
      }),
      // Attempt to preload for the primary profile.
      CreateAndWarmGlic(/*primary_profile=*/true),
      // Since there is no contention, this should have succeeded
      // (and we should not have attempted to warm the other web
      // client, so it should not yet be warmed).
      CheckWarmedAndSized(true, false), SendMemoryPressureSignal(),
      CheckWarmedAndSized(false, false));
}

IN_PROC_BROWSER_TEST_F(DISABLED_GlicProfileManagerUiTest,
                       MemPressureDoesNotClearShown) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've disabled preloading, nothing should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Re-enable preloading.
      ResetPreloading(), Do([this]() {
        SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
      }),
      DeprecatedOpenGlicWindow(GlicWindowMode::kAttached),
      CheckWarmedAndSized(true, false), SendMemoryPressureSignal(),
      // Since the window is showing, we shouldn't close it.
      CheckWarmedAndSized(true, false),
      // This should close the window.
      ToggleGlicWindow(GlicWindowMode::kAttached),
      // Since the window was shown, it is the last active glic and
      // should not be cleared.
      SendMemoryPressureSignal(), CheckWarmedAndSized(true, false));
}

}  // namespace glic
