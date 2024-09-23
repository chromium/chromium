// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/shell.h"
#include "ash/test/pixel/ash_pixel_diff_util.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/widget/widget.h"

namespace ash::personalization_app {

namespace {

template <typename T>
class WindowPropertyWaiter : public aura::WindowObserver {
 public:
  WindowPropertyWaiter(aura::Window* window, const ui::ClassProperty<T>* key)
      : window_(window), key_(key) {}
  ~WindowPropertyWaiter() override = default;

  void Wait() {
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();

    make_transparent_observation_.Observe(window_.get());

    loop.Run();
  }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != key_) {
      return;
    }

    if (quit_closure_) {
      std::move(quit_closure_).Run();
      make_transparent_observation_.Reset();
    }
  }

 private:
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      make_transparent_observation_{this};
  base::OnceClosure quit_closure_;
  raw_ptr<aura::Window> window_;
  const raw_ptr<const ui::ClassProperty<T>> key_;
};

// After the full screen change is observed, there is a significant delay
// until rendering catches up. To make sure that the wallpaper is visible
// through the transparent frame, force a roundtrip through the compositor.
// This allows shelf to hide, app list to hide, and window transparency to flush
// to the "gpu".
void WaitForCompositorFlush(content::WebContents* web_contents) {
  {
    base::RunLoop loop;
    web_contents->GetPrimaryMainFrame()->InsertVisualStateCallback(
        base::BindLambdaForTesting([&loop](bool visual_state_updated) {
          ASSERT_TRUE(visual_state_updated);
          loop.QuitWhenIdle();
        }));
    loop.Run();
  }

  {
    base::RunLoop loop;
    ash::Shell::GetPrimaryRootWindow()
        ->GetHost()
        ->compositor()
        ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
            [](base::OnceClosure quit_closure,
               const viz::FrameTimingDetails& frame_timing_details) {
              std::move(quit_closure).Run();
            },
            loop.QuitWhenIdleClosure()));
    loop.Run();
  }
}

void WaitForOpacityFalse(content::WebContents* web_contents) {
  auto* window = web_contents->GetTopLevelNativeWindow();
  auto* const opacityKey = chromeos::kWindowManagerManagesOpacityKey;

  if (window->GetProperty(opacityKey)) {
    // Wait for opacity key to change to false.
    WindowPropertyWaiter<bool> window_property_waiter(window, opacityKey);
    window_property_waiter.Wait();
  }
  ASSERT_FALSE(window->GetProperty(opacityKey));
}

void CallMakeTransparent(content::WebContents* web_contents) {
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"personalizationTestApi.makeTransparent()", base::DoNothing(),
      content::ISOLATED_WORLD_ID_GLOBAL);
}

}  // namespace

class PersonalizationAppIntegrationBrowserTest
    : public SystemWebAppIntegrationTest {
 public:
  void SetUpOnMainThread() override {
    SystemWebAppIntegrationTest::SetUpOnMainThread();
    ash::Shell::GetPrimaryRootWindow()
        ->GetHost()
        ->compositor()
        ->DisableAnimations();
  }

  // Launch the app at the wallpaper subpage to avoid a redirect while loading
  // the app.
  content::WebContents* LaunchAppAtWallpaperSubpage(Browser** browser) {
    apps::AppLaunchParams launch_params =
        LaunchParamsForApp(ash::SystemWebAppType::PERSONALIZATION);
    launch_params.override_url =
        GURL(std::string(kChromeUIPersonalizationAppURL) +
             kWallpaperSubpageRelativeUrl);
    return LaunchApp(std::move(launch_params), browser);
  }

 private:
  ui::ScopedAnimationDurationScaleMode animation_duration_scale_mode_{
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION};
};

// Test that the Personalization App installs correctly.
IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationBrowserTest,
                       PersonalizationAppInstalls) {
  const GURL url(kChromeUIPersonalizationAppURL);
  std::string appTitle = "Wallpaper & style";
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::PERSONALIZATION, url, appTitle));
}

// Test that the widget is modified to be transparent.
IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationBrowserTest,
                       PersonalizationAppWidgetIsTransparent) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  content::WebContents* web_contents = LaunchAppAtWallpaperSubpage(&browser);

  CallMakeTransparent(web_contents);
  WaitForOpacityFalse(web_contents);
  WaitForCompositorFlush(web_contents);

  EXPECT_TRUE(web_contents->GetTopLevelNativeWindow()->GetTransparent());
  EXPECT_FALSE(web_contents->GetTopLevelNativeWindow()->GetProperty(
      chromeos::kWindowManagerManagesOpacityKey));
}

IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationBrowserTest,
                       PersonalizationAppDisablesWindowBackdrop) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  content::WebContents* web_contents = LaunchAppAtWallpaperSubpage(&browser);
  aura::Window* window = web_contents->GetTopLevelNativeWindow();

  ash::WindowBackdrop* window_backdrop = ash::WindowBackdrop::Get(window);

  ASSERT_EQ(ash::WindowBackdrop::BackdropMode::kAuto, window_backdrop->mode());

  CallMakeTransparent(web_contents);
  WaitForOpacityFalse(web_contents);
  WaitForCompositorFlush(web_contents);

  EXPECT_EQ(ash::WindowBackdrop::BackdropMode::kDisabled,
            window_backdrop->mode());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    PersonalizationAppIntegrationBrowserTest);

}  // namespace ash::personalization_app
