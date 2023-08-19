// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
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
#include "base/test/scoped_feature_list.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/test/sk_color_eq.h"
#include "ui/message_center/message_center.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/widget/widget.h"

namespace ash::personalization_app {

namespace {

constexpr SkColor kDebugBackgroundColor = SK_ColorYELLOW;
constexpr int kDebugImageWidthPx = 400;
constexpr int kDebugImageHeightPx = 300;

gfx::ImageSkia CreateSolidImageSkia(const gfx::Size& size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height(), /*isOpaque=*/true);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(std::move(bitmap));
}

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
  raw_ptr<aura::Window, ExperimentalAsh> window_;
  const raw_ptr<const ui::ClassProperty<T>> key_;
};

void CallMakeTransparentAndWaitForOpacityChange(
    content::WebContents* web_contents) {
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"personalizationTestApi.makeTransparent()", base::DoNothing());

  auto* window = web_contents->GetTopLevelNativeWindow();
  auto* const opacityKey = chromeos::kWindowManagerManagesOpacityKey;

  if (window->GetProperty(opacityKey)) {
    // Wait for opacity key to change to false.
    WindowPropertyWaiter<bool> window_property_waiter(window, opacityKey);
    window_property_waiter.Wait();
  }

  // Wait for a round trip through renderer and compositor for transparency to
  // take effect.
  base::RunLoop loop;
  web_contents->GetPrimaryMainFrame()->InsertVisualStateCallback(
      base::BindLambdaForTesting([&loop](bool visual_state_updated) {
        ASSERT_TRUE(visual_state_updated);
        loop.Quit();
      }));
  loop.Run();
}

class WallpaperChangeWaiter : public ash::WallpaperControllerObserver {
 public:
  WallpaperChangeWaiter() = default;

  WallpaperChangeWaiter(const WallpaperChangeWaiter&) = delete;
  WallpaperChangeWaiter& operator=(const WallpaperChangeWaiter&) = delete;

  ~WallpaperChangeWaiter() override = default;

  void SetTestWallpaperAndWaitForColorsChanged() {
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();

    gfx::Size display_size =
        display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
    auto image = CreateSolidImageSkia(display_size, kDebugBackgroundColor);

    wallpaper_controller_observation_.Observe(ash::WallpaperController::Get());

    ash::WallpaperController::Get()->SetDecodedCustomWallpaper(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
        /*file_name=*/"fakename", ash::WALLPAPER_LAYOUT_CENTER_CROPPED,
        /*preview_mode=*/true, base::DoNothing(), /*file_path=*/"", image);

    loop.Run();
  }

  // Use OnWallpaperColorsChanged instead of OnWallpaperChanged.
  // OnWallpaperChanged fires way before the wallpaper is actually displayed.
  void OnWallpaperColorsChanged() override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
      wallpaper_controller_observation_.Reset();
    }
  }

 private:
  base::ScopedObservation<ash::WallpaperController,
                          ash::WallpaperControllerObserver>
      wallpaper_controller_observation_{this};
  base::OnceClosure quit_closure_;
};

class CompositorAnimationWaiter : public ui::CompositorObserver {
 public:
  // Wait for the first non animated frame to complete.
  void BlockUntilNonAnimatedFrameEnds() {
    DCHECK(!first_non_animated_frame_callback_);
    base::RunLoop loop;
    first_non_animated_frame_callback_ = loop.QuitClosure();
    loop.Run();
  }

  void OnCompositingEnded(ui::Compositor* compositor) override {
    if (compositing_ended_callback_) {
      std::move(compositing_ended_callback_).Run();
    }
  }

  void OnFirstNonAnimatedFrameStarted(ui::Compositor* compositor) override {
    if (first_non_animated_frame_callback_) {
      // Continue blocking until the next `OnCompositingEnded` event.
      compositing_ended_callback_ =
          std::move(first_non_animated_frame_callback_);
    }
  }

 private:
  base::OnceClosure first_non_animated_frame_callback_;
  base::OnceClosure compositing_ended_callback_;
};

}  // namespace

class PersonalizationAppIntegrationBrowserTest
    : public SystemWebAppIntegrationTest {
 public:
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

  void SetAppFullscreenAndWait(Browser* browser,
                               content::WebContents* web_contents) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
        web_contents->GetTopLevelNativeWindow());
    DCHECK(widget);
    EXPECT_FALSE(widget->IsFullscreen());

    FullscreenNotificationObserver waiter(browser);
    web_contents->GetPrimaryMainFrame()
        ->ExecuteJavaScriptWithUserGestureForTests(
            u"personalizationTestApi.enterFullscreen();", base::NullCallback());
    waiter.Wait();
    EXPECT_TRUE(widget->IsFullscreen());

    // After the full screen change is observed, there is a significant delay
    // until rendering catches up. To make sure that the wallpaper is visible
    // through the transparent frame, block until the compositor finishes
    // animating. This allows shelf to hide, app list to hide, and window
    // transparency to flush to the "gpu".
    auto* compositor =
        web_contents->GetTopLevelNativeWindow()->GetHost()->compositor();
    CompositorAnimationWaiter compositor_animation_waiter;
    compositor->AddObserver(&compositor_animation_waiter);
    compositor_animation_waiter.BlockUntilNonAnimatedFrameEnds();
    compositor->RemoveObserver(&compositor_animation_waiter);
  }
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

  CallMakeTransparentAndWaitForOpacityChange(web_contents);

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

  CallMakeTransparentAndWaitForOpacityChange(web_contents);

  ash::WindowBackdrop* window_backdrop = ash::WindowBackdrop::Get(window);
  EXPECT_EQ(ash::WindowBackdrop::BackdropMode::kDisabled,
            window_backdrop->mode());
}

// Test that the background color is forced to be transparent.
IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationBrowserTest,
                       SetsTransparentBackgroundColor) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  content::WebContents* web_contents = LaunchAppAtWallpaperSubpage(&browser);
  DCHECK(web_contents);

  CallMakeTransparentAndWaitForOpacityChange(web_contents);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  EXPECT_FALSE(browser_view->contents_web_view()->GetBackground())
      << "No background set for personalization app contents web view";

  EXPECT_NE(
      SK_ColorTRANSPARENT,
      web_contents->GetRenderWidgetHostView()->GetBackgroundColor().value())
      << "personalization app starts with opaque background color";

  // Trigger full screen mode, which sets a transparent background color.
  SetAppFullscreenAndWait(browser, web_contents);

  EXPECT_SKCOLOR_EQ(
      SK_ColorTRANSPARENT,
      web_contents->GetRenderWidgetHostView()->GetBackgroundColor().value());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    PersonalizationAppIntegrationBrowserTest);

class PersonalizationAppIntegrationPixelBrowserTest
    : public PersonalizationAppIntegrationBrowserTest {
 public:
  PersonalizationAppIntegrationPixelBrowserTest() {
    // Disable jelly until button colors are stable.
    scoped_feature_list_.InitAndDisableFeature(chromeos::features::kJelly);
  }

  void SetUp() override {
    if (IsExperimentalBrowserPixelTestEnabled()) {
      view_skia_gold_pixel_diff_ =
          std::make_unique<views::ViewSkiaGoldPixelDiff>(
              GetScreenshotPrefixForCurrentTestInfo());
    }
    PersonalizationAppIntegrationBrowserTest::SetUp();
  }

  bool IsExperimentalBrowserPixelTestEnabled() {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kVerifyPixels);
  }

  void PrepareUi() {
    // Dark/light changes button color.
    DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);

    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay(base::StringPrintf("%dx%d", kDebugImageWidthPx,
                                          kDebugImageHeightPx));

    // Minimize all windows.
    for (Browser* browser : *BrowserList::GetInstance()) {
      browser->window()->Minimize();
    }

    // Dismiss any notifications that interfere with taking a screenshot.
    message_center::MessageCenter::Get()->RemoveAllNotifications(
        /*by_user=*/false, message_center::MessageCenter::RemoveType::ALL);

    WaitForTestSystemAppInstall();
  }

  void VerifyRootWindowPixels(const std::string& screenshot_name) {
    if (!view_skia_gold_pixel_diff_) {
      DVLOG(3) << "Skipping pixel test verification";
      return;
    }
    auto* root_window = ash::Shell::GetPrimaryRootWindow();
    view_skia_gold_pixel_diff_->CompareNativeWindowScreenshot(
        screenshot_name, root_window, root_window->bounds());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<views::ViewSkiaGoldPixelDiff> view_skia_gold_pixel_diff_;
};

// Do not run on very slow builds.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
#define MAYBE_PixelTestFullscreenPreview DISABLED_PixelTestFullscreenPreview
#else
#define MAYBE_PixelTestFullscreenPreview PixelTestFullscreenPreview
#endif

// Screenshot the entire system UI while in fullscreen preview mode. Should see
// a bright yellow wallpaper with the full screen controls overlay.
IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationPixelBrowserTest,
                       MAYBE_PixelTestFullscreenPreview) {
  // Full screen preview flow only active in tablet mode.
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  PrepareUi();
  Browser* browser;
  content::WebContents* web_contents = LaunchAppAtWallpaperSubpage(&browser);

  CallMakeTransparentAndWaitForOpacityChange(web_contents);

  WallpaperChangeWaiter wallpaper_changer;
  wallpaper_changer.SetTestWallpaperAndWaitForColorsChanged();

  SetAppFullscreenAndWait(browser, web_contents);

  VerifyRootWindowPixels("wallpaper_fullscreen_preview");
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    PersonalizationAppIntegrationPixelBrowserTest);

}  // namespace ash::personalization_app
