// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/current_thread.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_iwa_runtime_data_provider_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "url/origin.h"

namespace ash {

namespace {

web_app::IsolatedWebAppUrlInfo InstallStandaloneIsolatedWebAppAndReturnUrlInfo(
    Profile* profile) {
  web_app::IsolatedWebAppUrlInfo url_info =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().SetName("SetShape Test IWA (standalone)"))
          .BuildBundle()
          ->InstallChecked(profile);
  return url_info;
}

web_app::IsolatedWebAppUrlInfo InstallUnframedIsolatedWebApp(Profile* profile) {
  web_app::IsolatedWebAppUrlInfo url_info =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder()
              .SetName("SetShape Test IWA (unframed)")
              .SetDisplayMode(blink::mojom::DisplayMode::kStandalone)
              .SetDisplayModeOverride(
                  {web_app::DisplayOverride::CreateUnframed({})})
              .AddPermissionsPolicy(
                  network::mojom::PermissionsPolicyFeature::kWindowManagement,
                  /*self=*/true, /*origins=*/{}))
          .BuildBundle()
          ->InstallChecked(profile);
  return url_info;
}

void SetWindowManagementPermission(Profile* profile,
                                   const url::Origin& origin,
                                   ContentSetting setting) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  map->SetContentSettingDefaultScope(origin.GetURL(), origin.GetURL(),
                                     ContentSettingsType::WINDOW_MANAGEMENT,
                                     setting);
}

// Returns the shape rectangles of the given `frame`, or `nullptr` if `frame`
// has no custom shape.
const std::vector<gfx::Rect>* GetShape(content::RenderFrameHost* frame) {
  return views::Widget::GetTopLevelWidgetForNativeView(
             content::WebContents::FromRenderFrameHost(frame)->GetNativeView())
      ->GetNativeWindow()
      ->layer()
      ->alpha_shape();
}

// Helper to match custom shapes in a `RenderFrameHost` given the expected
// `rects`. When `rects` is empty, expect the shape to be null.
testing::Matcher<content::RenderFrameHost*> ShapeRectanglesAre(
    const std::vector<testing::Matcher<gfx::Rect>>& rects) {
  if (rects.empty()) {
    return testing::ResultOf(GetShape, testing::IsNull());
  }
  return testing::ResultOf(
      GetShape,
      testing::AllOf(testing::NotNull(),
                     testing::Pointee(testing::ElementsAreArray(rects))));
}

// Waits until the display mode of `frame` becomes the given `display_mode`.
bool WaitDisplayModeIs(content::RenderFrameHost* frame,
                       blink::mojom::DisplayMode display_mode) {
  return base::test::RunUntil([frame, display_mode]() {
    content::WebContents& web_contents =
        CHECK_DEREF(content::WebContents::FromRenderFrameHost(frame));
    content::WebContentsDelegate& delegate =
        CHECK_DEREF(web_contents.GetDelegate());
    return delegate.GetDisplayMode(&web_contents) == display_mode;
  });
}

}  // namespace

class SetShapeTest : public web_app::IsolatedWebAppBrowserTestHarness {
 protected:
  void SetUpOnMainThread() override {
    web_app::IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();

    app_url_info_ = InstallUnframedIsolatedWebApp(profile());

    SetWindowManagementPermission(profile(), app_url_info_->origin(),
                                  CONTENT_SETTING_ALLOW);

    AllowSetShapeFor(app_url_info_->web_bundle_id());
  }

  void AllowSetShapeFor(const web_package::SignedWebBundleId& web_bundle_id) {
    data_provider_->Update([&](auto& update) {
      update.AddToSpecialPermissions(web_bundle_id, {.allow_set_shape = true});
    });
  }

  std::optional<web_app::IsolatedWebAppUrlInfo> app_url_info_;

 private:
  base::test::ScopedFeatureList feature_list_{blink::features::kUnframedIwa};
  web_app::FakeIwaRuntimeDataProviderMixin data_provider_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(SetShapeTest, ValidatesInput) {
  content::RenderFrameHost* frame = OpenApp(app_url_info_->app_id());

  constexpr auto invalid_inputs = base::MakeFixedFlatSet<std::string_view>({
      // Negative dimension.
      "[new DOMRect(0, 0, -1, 200)]",
      "[new DOMRect(0, 0, 200, -1)]",
      // Infinite location or dimension.
      "[new DOMRect(Infinity, 0, 200, 200)]",
      "[new DOMRect(0, Infinity, 200, 200)]",
      "[new DOMRect(0, 0, Infinity, 200)]",
      "[new DOMRect(0, 0, 200, Infinity)]",
      // NaN location or dimension.
      "[new DOMRect(NaN, 0, 200, 200)]",
      "[new DOMRect(0, NaN, 200, 200)]",
      "[new DOMRect(0, 0, NaN, 200)]",
      "[new DOMRect(0, 0, 200, NaN)]",
      // Too many rectangles.
      "Array(10001).fill(new DOMRect(0, 0, 200, 200))",
      // Rectangles too small.
      "[new DOMRect(0, 0, 9, 9)]",
      "[new DOMRect(0, 0, 9, 10)]",
      "[new DOMRect(0, 0, 10, 9)]",
      "[new DOMRect(0, 0, 9, 9), new DOMRect(10, 10, 5, 5)]",
      // Rectangles outside window bounds.
      "[new DOMRect(-100000, -100000, 10, 10)]",
      "[new DOMRect(window.innerWidth, 0, 10, 10)]",
      "[new DOMRect(0, window.innerHeight, 10, 10)]",
      // Rectangles that intersect with the window bounds but are too small.
      "[new DOMRect(-5, -5, 10, 10)]",
      "[new DOMRect(window.innerWidth - 5, 0, 10, 10)]",
      "[new DOMRect(0, window.innerHeight - 5, 10, 10)]",
  });
  for (const auto& input : invalid_inputs) {
    std::string script = base::StrCat({
        "window.setShape(",
        input,
        ").catch(error => error.name)",
    });
    EXPECT_EQ("TypeError", content::EvalJs(frame, script))
        << "Failed on input: " << input;
    EXPECT_THAT(frame, ShapeRectanglesAre({}));
  }
}

IN_PROC_BROWSER_TEST_F(SetShapeTest, WorksInUnframedMode) {
  content::RenderFrameHost* frame = OpenApp(app_url_info_->app_id());

  auto result = content::EvalJs(frame, R"(
      window.setShape([
        new DOMRect(0, 0, 200, 200),
        new DOMRect(180, 190, 100, 300)
      ])
    )");
  EXPECT_EQ(base::Value(), result);
  EXPECT_THAT(frame, ShapeRectanglesAre({gfx::Rect(0, 0, 200, 200),
                                         gfx::Rect(180, 190, 100, 300)}));
}

IN_PROC_BROWSER_TEST_F(SetShapeTest, DoesNotWorkOutsideUnframedMode) {
  web_app::IsolatedWebAppUrlInfo url_info =
      InstallStandaloneIsolatedWebAppAndReturnUrlInfo(profile());

  AllowSetShapeFor(url_info.web_bundle_id());

  content::RenderFrameHost* frame = OpenApp(url_info.app_id());

  auto result = content::EvalJs(frame, R"(
      window.setShape([
        new DOMRect(0, 0, 200, 200),
        new DOMRect(180, 190, 100, 300)
      ]).catch(error => error.name)
    )");
  EXPECT_EQ("InvalidStateError", result);
  EXPECT_THAT(frame, ShapeRectanglesAre({}));
}

IN_PROC_BROWSER_TEST_F(SetShapeTest, ClearsShapeOnTransitionFromUnframed) {
  content::RenderFrameHost* frame = OpenApp(app_url_info_->app_id());

  EXPECT_THAT(frame, ShapeRectanglesAre({}));

  auto result = content::EvalJs(frame, R"(
      window.setShape([
        new DOMRect(0, 0, 200, 200)
      ])
    )");
  EXPECT_EQ(base::Value(), result);
  EXPECT_THAT(frame, ShapeRectanglesAre({gfx::Rect(0, 0, 200, 200)}));

  SetWindowManagementPermission(profile(), app_url_info_->origin(),
                                CONTENT_SETTING_BLOCK);
  ASSERT_TRUE(WaitDisplayModeIs(frame, blink::mojom::DisplayMode::kStandalone));
  EXPECT_THAT(frame, ShapeRectanglesAre({}));

  auto set_shape_after_revoke = content::EvalJs(frame, R"(
      window.setShape([
        new DOMRect(0, 0, 200, 200)
      ]).catch(error => error.name)
    )");
  EXPECT_EQ("InvalidStateError", set_shape_after_revoke);
  EXPECT_THAT(frame, ShapeRectanglesAre({}));
}

IN_PROC_BROWSER_TEST_F(SetShapeTest, AllowsMixOfSmallAndLargeRects) {
  content::RenderFrameHost* frame = OpenApp(app_url_info_->app_id());

  auto result = content::EvalJs(frame, R"(
      window.setShape([
        new DOMRect(0, 0, 9, 9),
        new DOMRect(10, 10, 10, 10)
      ])
    )");
  EXPECT_EQ(base::Value(), result);
  EXPECT_THAT(frame, ShapeRectanglesAre({
                         gfx::Rect(0, 0, 9, 9),
                         gfx::Rect(10, 10, 10, 10),
                     }));
}

IN_PROC_BROWSER_TEST_F(SetShapeTest, AllowsPartiallyOutOfBoundsRectangles) {
  content::RenderFrameHost* frame = OpenApp(app_url_info_->app_id());

  auto result = content::EvalJs(frame, R"(
      window.setShape([
        new DOMRect(-10, 0, 100, 100)
      ])
    )");
  EXPECT_EQ(base::Value(), result);
  EXPECT_THAT(frame, ShapeRectanglesAre({gfx::Rect(-10, 0, 100, 100)}));
}

IN_PROC_BROWSER_TEST_F(SetShapeTest, EmptyListClearsShape) {
  content::RenderFrameHost* frame = OpenApp(app_url_info_->app_id());

  auto set_result = content::EvalJs(frame, R"(
      window.setShape([
        new DOMRect(0, 0, 200, 200)
      ])
    )");
  EXPECT_EQ(base::Value(), set_result);
  EXPECT_THAT(frame, ShapeRectanglesAre({gfx::Rect(0, 0, 200, 200)}));

  auto clear_result = content::EvalJs(frame, R"(
      window.setShape([])
    )");
  EXPECT_EQ(base::Value(), clear_result);
  EXPECT_THAT(frame, ShapeRectanglesAre({}));
}

}  // namespace ash
