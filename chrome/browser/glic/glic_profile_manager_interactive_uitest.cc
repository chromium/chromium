// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_monitor.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"

namespace glic {

class FreWebUiStateObserver
    : public ui::test::StateObserver<mojom::FreWebUiState> {
 public:
  explicit FreWebUiStateObserver(GlicFreController* controller)
      : subscription_(controller->AddWebUiStateChangedCallback(
            base::BindRepeating(&FreWebUiStateObserver::OnWebUiStateChanged,
                                base::Unretained(this)))),
        controller_(controller) {}

  mojom::FreWebUiState GetStateObserverInitialState() const override {
    return controller_->GetWebUiState();
  }

  void OnWebUiStateChanged(mojom::FreWebUiState new_state) {
    OnStateObserverStateChanged(new_state);
  }

 private:
  base::CallbackListSubscription subscription_;
  raw_ptr<GlicFreController> controller_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(FreWebUiStateObserver, kFreWebUiState);

class GlicProfileManagerUiTest
    : public test::InteractiveGlicTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  GlicProfileManagerUiTest() {
    if (ShouldWarmMultiple()) {
      feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{features::kGlicWarmMultiple, {}},
                                {features::kGlicFreWarming, {}},
                                {features::kGlicWarming,
                                 {{features::kGlicWarmingDelayMs.name, "0"},
                                  {features::kGlicWarmingJitterMs.name, "0"}}}},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{features::kGlicFreWarming, {}},
                                {features::kGlicWarming,
                                 {
                                     {features::kGlicWarmingDelayMs.name, "0"},
                                     {features::kGlicWarmingJitterMs.name, "0"},
                                 }}},
          /*disabled_features=*/{features::kGlicWarmMultiple});
    }
  }
  ~GlicProfileManagerUiTest() override = default;

  void SetUp() override {
    // This will temporarily disable preloading to ensure that we don't load the
    // web client before we've initialized the embedded test server and can set
    // the correct URL.
    GlicProfileManager::ForceMemoryPressureForTesting(
        base::MemoryPressureMonitor::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_CRITICAL);
    GlicProfileManager::ForceConnectionTypeForTesting(
        network::mojom::ConnectionType::CONNECTION_ETHERNET);
    fre_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));
    ASSERT_TRUE(fre_server_.Start());
    fre_url_ = fre_server_.GetURL("/glic/test_client/fre.html");
    test::InteractiveGlicTest::SetUp();
  }

  void TearDown() override {
    test::InteractiveGlicTest::TearDown();
    GlicProfileManager::ForceMemoryPressureForTesting(std::nullopt);
    GlicProfileManager::ForceConnectionTypeForTesting(std::nullopt);
  }

  void SetUpOnMainThread() override {
    test::InteractiveGlicTest::SetUpOnMainThread();
    auto* profile_manager = g_browser_process->profile_manager();
    auto new_path = profile_manager->GenerateNextProfileDirectoryPath();
    profiles::testing::CreateProfileSync(profile_manager, new_path);
    auto* profile = profile_manager->GetProfile(new_path);
    // Build a test environment for the new profile to ensure that it can work
    // with glic.
    test_env_for_second_profile_ =
        std::make_unique<GlicTestEnvironment>(profile);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kGlicFreURL, fre_url_.spec());
  }

  void TearDownOnMainThread() override {
    test_env_for_second_profile_.reset();
    test::InteractiveGlicTest::TearDownOnMainThread();
  }

  bool ShouldWarmMultiple() const { return std::get<0>(GetParam()); }

  bool ShouldWarmFRE() const { return std::get<1>(GetParam()); }

  GlicKeyedService* GetService(bool primary) {
    Profile* profile =
        primary ? browser()->profile()
                : test_env_for_second_profile_->GetService()->profile();
    return GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  }

  auto CreateAndWarmGlic(bool primary_profile) {
    return Do([primary_profile, this]() {
      if (ShouldWarmFRE()) {
        if (primary_profile) {
          glic_test_environment().SetFRECompletion(
              prefs::FreStatus::kNotStarted);
        } else {
          test_env_for_second_profile_->SetFRECompletion(
              prefs::FreStatus::kNotStarted);
        }
        GetService(primary_profile)->TryPreloadFre();
      } else {
        GetService(primary_profile)->TryPreload();
      }
    });
  }

  auto CheckWarmedAndSized(bool primary_warmed, bool secondary_warmed) {
    return Do([primary_warmed, secondary_warmed, this]() {
      auto IsWarmedAndSized = [](GlicKeyedService* service) {
        const bool warmed =
            service->window_controller().IsWarmed() ||
            service->window_controller().fre_controller()->IsWarmed() ||
            service->window_controller().IsPanelOrFreShowing();
        if (!warmed) {
          return false;
        }
        auto* contents = service->host().webui_contents();
        if (!contents) {
          contents = service->window_controller().GetFreWebContents();
        }
        return contents && !contents->GetSize().IsEmpty();
      };

      EXPECT_EQ(primary_warmed, IsWarmedAndSized(GetService(true)));
      EXPECT_EQ(secondary_warmed, IsWarmedAndSized(GetService(false)));
    });
  }

  auto ResetMemoryPressure() {
    return Do([]() {
      GlicProfileManager::ForceMemoryPressureForTesting(
          base::MemoryPressureMonitor::MemoryPressureLevel::
              MEMORY_PRESSURE_LEVEL_NONE);
    });
  }

  auto CacheClientContents(bool primary_profile) {
    return Do([this, primary_profile]() {
      auto* service = GetService(primary_profile);
      if (ShouldWarmFRE()) {
        web_client_contents_ = service->window_controller().GetFreWebContents();
      } else {
        web_client_contents_ = service->host().webui_contents();
      }
    });
  }

  auto CheckCachedClientContents(bool primary_profile) {
    return Do([this, primary_profile]() {
      auto* service = GetService(primary_profile);
      auto& controller = GetService(primary_profile)->window_controller();
      if (ShouldWarmFRE()) {
        EXPECT_EQ(web_client_contents_, controller.GetFreWebContents());
        EXPECT_NE(nullptr, controller.GetFreWebContents());
      } else {
        EXPECT_EQ(web_client_contents_, service->host().webui_contents());
        EXPECT_NE(nullptr, service->host().webui_contents());
      }
      web_client_contents_ = nullptr;
    });
  }

  auto SetNeedsFRE() {
    return Do([this]() {
      glic_test_environment().SetFRECompletion(prefs::FreStatus::kNotStarted);
    });
  }

  auto SendMemoryPressureSignal(bool primary_profile) {
    return Do([this, primary_profile]() {
      GlicProfileManager::ForceMemoryPressureForTesting(
          base::MemoryPressureMonitor::MemoryPressureLevel::
              MEMORY_PRESSURE_LEVEL_CRITICAL);
      GetService(primary_profile)
          ->OnMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel::
                                 MEMORY_PRESSURE_LEVEL_CRITICAL);
    });
  }

  GlicFreController* GetFreController(bool primary_profile) {
    return GetService(true)->window_controller().fre_controller();
  }

 private:
  std::unique_ptr<GlicTestEnvironment> test_env_for_second_profile_;
  net::EmbeddedTestServer fre_server_;
  raw_ptr<content::WebContents> web_client_contents_ = nullptr;
  GURL fre_url_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicProfileManagerUiTest, ConsistentPreload) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've artificially set high memory pressure, nothing
      // should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Resetting the memory pressure will enable preloading again.
      ResetMemoryPressure(),
      // Attempt to preload for the primary profile.
      CreateAndWarmGlic(/*primary_profile=*/true),
      // Since there is no contention, this should have succeeded
      // (and we should not have attempted to warm the other web
      // client, so it should not yet be warmed).
      CheckWarmedAndSized(true, false),
      // This stores a pointer to the web client contents so that we can check
      // that the the shown contents match (otherwise, we've warmed for no
      // reason).
      CacheClientContents(/*primary_profile=*/true),
      If(base::BindOnce(&GlicProfileManagerUiTest::ShouldWarmFRE,
                        base::Unretained(this)),
         Then(SetNeedsFRE(),
              ObserveState(
                  kFreWebUiState,
                  base::BindOnce(&GlicProfileManagerUiTest::GetFreController,
                                 base::Unretained(this),
                                 /*primary_profile=*/true)),
              ToggleGlicWindow(GlicWindowMode::kAttached),
              WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady)),
         Else(OpenGlicWindow(GlicWindowMode::kAttached))),
      // Check that the client contents are the same as when warmed.
      CheckCachedClientContents(/*primary_profile=*/true));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerUiTest, PreloadMutex) {
  RunTestSequence(WaitForShow(kGlicButtonElementId),
                  // Since we've artificially set high memory pressure, nothing
                  // should be preloaded yet.
                  CheckWarmedAndSized(false, false),
                  // Resetting the memory pressure will enable preloading again.
                  ResetMemoryPressure(),
                  // Attempt to preload for the primary profile.
                  CreateAndWarmGlic(/*primary_profile=*/true),
                  // Since there is no contention, this should have succeeded
                  // (and we should not have attempted to warm the other web
                  // client, so it should not yet be warmed).
                  CheckWarmedAndSized(true, false),
                  // Warming the secondary profile will cause another web client
                  // to come into existence.
                  CreateAndWarmGlic(/*primary_profile=*/false),
                  // The first service should only remain warmed if we have the
                  // feature `kGlicWarmMultiple` enabled.
                  CheckWarmedAndSized(ShouldWarmMultiple(), true));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerUiTest, ShowMutex) {
  RunTestSequence(WaitForShow(kGlicButtonElementId),
                  // Since we've artificially set high memory pressure, nothing
                  // should be preloaded yet.
                  CheckWarmedAndSized(false, false),
                  // Resetting the memory pressure will enable preloading again.
                  ResetMemoryPressure(),
                  // Attempt to preload for the secondary profile.
                  CreateAndWarmGlic(/*primary_profile=*/false),
                  // Since there is no contention, this should have succeeded
                  // (and we should not have attempted to warm the other web
                  // client, so it should not yet be warmed).
                  CheckWarmedAndSized(false, true),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  // The first service should only remain warmed if we have the
                  // feature `kGlicWarmMultiple` enabled.
                  CheckWarmedAndSized(true, ShouldWarmMultiple()));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerUiTest, FreMutex) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've artificially set high memory pressure, nothing
      // should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Resetting the memory pressure will enable preloading again.
      ResetMemoryPressure(),
      // Attempt to preload for the secondary profile.
      CreateAndWarmGlic(/*primary_profile=*/false),
      // Since there is no contention, this should have succeeded
      // (and we should not have attempted to warm the other web
      // client, so it should not yet be warmed).
      CheckWarmedAndSized(false, true), SetNeedsFRE(),
      ObserveState(
          kFreWebUiState,
          base::BindOnce(&GlicProfileManagerUiTest::GetFreController,
                         base::Unretained(this), /*primary_profile=*/true)),
      ToggleGlicWindow(GlicWindowMode::kAttached),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      // The first service should only remain warmed if we have the
      // feature `kGlicWarmMultiple` enabled.
      CheckWarmedAndSized(true, ShouldWarmMultiple()));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerUiTest, DoNotWarmWhenShowing) {
  RunTestSequence(WaitForShow(kGlicButtonElementId),
                  // Since we've artificially set high memory pressure, nothing
                  // should be preloaded yet.
                  CheckWarmedAndSized(false, false),
                  // Resetting the memory pressure will enable preloading again.
                  ResetMemoryPressure(),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckWarmedAndSized(true, false),
                  // Attempt to preload for the secondary profile.
                  CreateAndWarmGlic(/*primary_profile=*/false),
                  CheckWarmedAndSized(true, ShouldWarmMultiple()));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerUiTest, DoNotWarmWhenShowingFre) {
  RunTestSequence(
      WaitForShow(kGlicButtonElementId),
      // Since we've artificially set high memory pressure, nothing
      // should be preloaded yet.
      CheckWarmedAndSized(false, false),
      // Resetting the memory pressure will enable preloading again.
      ResetMemoryPressure(), SetNeedsFRE(),
      ObserveState(
          kFreWebUiState,
          base::BindOnce(&GlicProfileManagerUiTest::GetFreController,
                         base::Unretained(this), /*primary_profile=*/true)),
      ToggleGlicWindow(GlicWindowMode::kAttached),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      CheckWarmedAndSized(true, false),
      // Attempt to preload for the secondary profile.
      CreateAndWarmGlic(/*primary_profile=*/false),
      CheckWarmedAndSized(true, ShouldWarmMultiple()));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerUiTest, MemPressureClearsCache) {
  RunTestSequence(WaitForShow(kGlicButtonElementId),
                  // Since we've artificially set high memory pressure, nothing
                  // should be preloaded yet.
                  CheckWarmedAndSized(false, false),
                  // Resetting the memory pressure will enable preloading again.
                  ResetMemoryPressure(),
                  // Attempt to preload for the primary profile.
                  CreateAndWarmGlic(/*primary_profile=*/true),
                  // Since there is no contention, this should have succeeded
                  // (and we should not have attempted to warm the other web
                  // client, so it should not yet be warmed).
                  CheckWarmedAndSized(true, false),
                  SendMemoryPressureSignal(/*primary_profile=*/true),
                  CheckWarmedAndSized(false, false));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerUiTest, MemPressureDoesNotClearShown) {
  RunTestSequence(WaitForShow(kGlicButtonElementId),
                  // Since we've artificially set high memory pressure, nothing
                  // should be preloaded yet.
                  CheckWarmedAndSized(false, false),
                  // Resetting the memory pressure will enable preloading again.
                  ResetMemoryPressure(),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckWarmedAndSized(true, false),
                  SendMemoryPressureSignal(/*primary_profile=*/true),
                  // Since the window is showing, we shouldn't close it.
                  CheckWarmedAndSized(true, false),
                  // This should close the window.
                  ToggleGlicWindow(GlicWindowMode::kAttached),
                  // Since the window was shown, it is the last active glic and
                  // should not be cleared.
                  SendMemoryPressureSignal(/*primary_profile=*/true),
                  CheckWarmedAndSized(true, false));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicProfileManagerUiTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      std::string name =
          std::get<0>(info.param) ? "WarmMultiple_" : "DoNotWarmMultiple_";
      name += std::get<1>(info.param) ? "WarmFre" : "WarmGlic";
      return name;
    });

}  // namespace glic
