// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/shell.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_switches.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center.h"
#include "ui/snapshot/snapshot_aura.h"
#include "ui/views/widget/widget.h"

namespace ash::personalization_app {

namespace {

constexpr SkColor kDebugBackgroundColor = SK_ColorYELLOW;
constexpr int kDebugImageWidthPx = 600;
constexpr int kDebugImageHeightPx = 400;
constexpr int kButtonInsetPx = 20;
constexpr int kButtonHeightPx = 32;

// Allowed percentage of errors when checking pixels for the full screen preview
// image test. This allows some slack for differences in various renderers
// during tests.
constexpr int kAllowedPixelErrorPercent = 1.f;

gfx::ImageSkia CreateSolidImageSkia(const gfx::Size& size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height(), /*isOpaque=*/true);
  SkCanvas canvas(bitmap);
  canvas.drawColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(std::move(bitmap));
}

bool IsClose(SkColor expected, SkColor color, int threshold = 3) {
  int a_err = SkColorGetA(expected) - SkColorGetA(color);
  int r_err = SkColorGetR(expected) - SkColorGetR(color);
  int g_err = SkColorGetG(expected) - SkColorGetG(color);
  int b_err = SkColorGetB(expected) - SkColorGetB(color);
  return std::abs(a_err) < threshold && std::abs(r_err) < threshold &&
         std::abs(g_err) < threshold && std::abs(b_err) < threshold;
}

// Tests if the color is a shade of gray or white seen in the buttons. These
// colors are A = 255, R ~= G.
bool IsOpaqueGray(SkColor color, int threshold = 3) {
  int rg_diff = SkColorGetR(color) - SkColorGetG(color);
  return std::abs(rg_diff) < threshold && SkColorGetA(color) == SK_AlphaOPAQUE;
}

// Assert that the image is either pure yellow, or gray/white buttons in a
// specific region.
void AssertExpectedDebugImage(const SkBitmap& bitmap) {
  EXPECT_EQ(kDebugImageWidthPx, bitmap.width());
  EXPECT_EQ(kDebugImageHeightPx, bitmap.height());
  gfx::Rect buttons_rect(kButtonInsetPx, kButtonInsetPx,
                         kDebugImageWidthPx - 2 * kButtonInsetPx,
                         kButtonHeightPx);
  gfx::Rect error_bounding_rect;
  for (int x = 0; x < bitmap.width(); ++x) {
    for (int y = 0; y < bitmap.height(); ++y) {
      SkColor color = bitmap.getColor(x, y);
      bool is_yellow = IsClose(kDebugBackgroundColor, color);
      bool is_button_gray = buttons_rect.Contains(x, y) && IsOpaqueGray(color);
      if (!is_yellow && !is_button_gray)
        error_bounding_rect.Union(gfx::Rect(x, y, 1, 1));
    }
  }
  if (error_bounding_rect.IsEmpty())
    return;

  float error_percentage = 100.f *
                           static_cast<float>(error_bounding_rect.width() *
                                              error_bounding_rect.height()) /
                           static_cast<float>(bitmap.width() * bitmap.height());

  SkColor first_wrong_color =
      bitmap.getColor(error_bounding_rect.x(), error_bounding_rect.y());

  EXPECT_LT(error_percentage, kAllowedPixelErrorPercent)
      << "Expected either yellow background or a gray/white button but "
         "received ARGB("
      << SkColorGetA(first_wrong_color) << ", "
      << SkColorGetR(first_wrong_color) << ", "
      << SkColorGetG(first_wrong_color) << ", "
      << SkColorGetB(first_wrong_color) << ") within bounding box "
      << error_bounding_rect.ToString();
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

    make_transparent_observation_.Observe(window_);

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
  aura::Window* window_;
  const raw_ptr<const ui::ClassProperty<T>> key_;
};

void CallJavascriptAndWaitForPropertyChange(content::WebContents* web_contents,
                                            const std::u16string& javascript) {
  WindowPropertyWaiter<bool> window_property_waiter(
      web_contents->GetTopLevelNativeWindow(),
      chromeos::kWindowManagerManagesOpacityKey);
  web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      javascript, base::DoNothing());
  window_property_waiter.Wait();
}

class WallpaperChangeWaiter : public ash::WallpaperControllerObserver {
 public:
  WallpaperChangeWaiter() = default;

  WallpaperChangeWaiter(const WallpaperChangeWaiter&) = delete;
  WallpaperChangeWaiter& operator=(const WallpaperChangeWaiter&) = delete;

  ~WallpaperChangeWaiter() override = default;

  void SetWallpaperAndWait() {
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

}  // namespace

class PersonalizationAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  PersonalizationAppIntegrationTest() = default;

  // SystemWebAppIntegrationTest:
  void SetUp() override {
    EnablePixelOutput();
    SystemWebAppIntegrationTest::SetUp();
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

    // After the full screen change is observed, there is a significant delay
    // until rendering catches up. To make sure that the wallpaper is visible
    // through the transparent frame, block until the compositor updates. This
    // allows shelf to hide, app list to hide, and wallpaper to change.
    for (int i = 0; i < 3; i++) {
      base::RunLoop loop;
      web_contents->GetPrimaryMainFrame()->InsertVisualStateCallback(
          base::BindLambdaForTesting([&loop](bool visual_state_updated) {
            ASSERT_TRUE(visual_state_updated);
            loop.Quit();
          }));
      loop.Run();
    }

    EXPECT_TRUE(widget->IsFullscreen());
  }
};

// Test that the Personalization App installs correctly.
IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationTest,
                       PersonalizationAppInstalls) {
  const GURL url(kChromeUIPersonalizationAppURL);
  std::string appTitle = "Wallpaper & style";
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::PERSONALIZATION, url, appTitle));
}

// Test that the widget is modified to be transparent.
IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationTest,
                       PersonalizationAppWidgetIsTransparent) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  content::WebContents* web_contents = LaunchAppAtWallpaperSubpage(&browser);

  CallJavascriptAndWaitForPropertyChange(
      web_contents, u"personalizationTestApi.makeTransparent();");

  EXPECT_TRUE(web_contents->GetTopLevelNativeWindow()->GetTransparent());
  EXPECT_FALSE(web_contents->GetTopLevelNativeWindow()->GetProperty(
      chromeos::kWindowManagerManagesOpacityKey));
}

IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationTest,
                       PersonalizationAppDisablesWindowBackdrop) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  content::WebContents* web_contents = LaunchAppAtWallpaperSubpage(&browser);
  aura::Window* window = web_contents->GetTopLevelNativeWindow();

  CallJavascriptAndWaitForPropertyChange(
      web_contents, u"personalizationTestApi.makeTransparent();");

  ash::WindowBackdrop* window_backdrop = ash::WindowBackdrop::Get(window);
  EXPECT_EQ(ash::WindowBackdrop::BackdropMode::kDisabled,
            window_backdrop->mode());
}

// Test that the background color is forced to be transparent.
// Disabled due to flakiness. crbug.com/1294458
IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationTest,
                       DISABLED_SetsTransparentBackgroundColor) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  content::WebContents* web_contents = LaunchAppAtWallpaperSubpage(&browser);

  CallJavascriptAndWaitForPropertyChange(
      web_contents, u"personalizationTestApi.makeTransparent();");

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  EXPECT_EQ(SK_ColorTRANSPARENT,
            browser_view->contents_web_view()->GetBackground()->get_color());

  // Personalization app by default has an opaque content background color.
  // Trigger full screen mode, which sets a transparent background color.
  SetAppFullscreenAndWait(browser, web_contents);

  EXPECT_EQ(
      SK_ColorTRANSPARENT,
      web_contents->GetRenderWidgetHostView()->GetBackgroundColor().value());
}

// Screenshot the entire system UI while in fullscreen preview mode. Should see
// a bright yellow wallpaper with the full screen controls overlay. Note should
// not see crop option buttons even though wallpaper type is custom.
// TODO(crbug/1268795) fix this flaky test.
IN_PROC_BROWSER_TEST_P(PersonalizationAppIntegrationTest,
                       DISABLED_ScreenshotShowsWallpaperUnderSWA) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
      .UpdateDisplay(
          base::StringPrintf("%dx%d", kDebugImageWidthPx, kDebugImageHeightPx));

  // Minimize all windows.
  for (Browser* browser : *BrowserList::GetInstance())
    browser->window()->Minimize();

  // Dismiss any notifications that interfere with taking a screenshot.
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/false, message_center::MessageCenter::RemoveType::ALL);

  WaitForTestSystemAppInstall();
  Browser* browser;
  content::WebContents* web_contents = LaunchAppAtWallpaperSubpage(&browser);

  CallJavascriptAndWaitForPropertyChange(
      web_contents, u"personalizationTestApi.makeTransparent();");

  WallpaperChangeWaiter wallpaper_changer;
  wallpaper_changer.SetWallpaperAndWait();

  SetAppFullscreenAndWait(browser, web_contents);

  base::RunLoop loop;
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  ui::GrabLayerSnapshotAsync(
      root_window->layer(), gfx::Rect(root_window->bounds().size()),
      base::BindLambdaForTesting([&loop](gfx::Image snapshot) {
        AssertExpectedDebugImage(snapshot.AsBitmap());
        loop.Quit();
      }));
  loop.Run();
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    PersonalizationAppIntegrationTest);

}  // namespace ash::personalization_app
