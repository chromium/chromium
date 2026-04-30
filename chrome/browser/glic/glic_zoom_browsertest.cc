// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace glic {

class GlicZoomBrowserTest : public GlicBrowserTest {
 public:
  GlicZoomBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicClientZoomControl);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicZoomBrowserTest, ZoomHotkey) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(instance);

  // Wait for WebUI to be ready.
  ASSERT_TRUE(WaitForWebUiState(mojom::WebUiState::kReady).has_value());

  // Get the focus manager for triggering accelerators.
  views::View* view = instance->GetActiveEmbedderGlicViewForTesting();
  ASSERT_TRUE(view);
  views::FocusManager* focus_manager = view->GetWidget()->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  // Helper to get zoom level from webview.
  auto get_zoom_level = [&]() -> double {
    content::WebContents* webui_contents = instance->host().webui_contents();
    std::string script = R"(
      (async () => {
        const webview = document.querySelector('#webviewContainer webview');
        return new Promise(resolve => webview.getZoom(resolve));
      })()
    )";
    return content::EvalJs(webui_contents, script).ExtractDouble();
  };

  // Initial zoom should be 1.0.
  EXPECT_DOUBLE_EQ(get_zoom_level(), 1.0);

  // Trigger accelerator for zoom-in.
  base::span<const ui::Accelerator> zoom_in_accels =
      LocalHotkeyManager::GetStaticAccelerators(
          LocalHotkeyManager::Hotkey::kZoomIn);
  ASSERT_FALSE(zoom_in_accels.empty());
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in_accels[0]));

  // Verify zoom level increased.
  ASSERT_OK(RunUntilEqual<double>([&]() { return get_zoom_level(); }, 1.1,
                                  "Zoom level did not increase to 1.1"));

  // Trigger accelerator for zoom-out.
  base::span<const ui::Accelerator> zoom_out_accels =
      LocalHotkeyManager::GetStaticAccelerators(
          LocalHotkeyManager::Hotkey::kZoomOut);
  ASSERT_FALSE(zoom_out_accels.empty());
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_out_accels[0]));
  // Verify zoom level decreased.
  ASSERT_OK(RunUntilEqual<double>([&]() { return get_zoom_level(); }, 1.0,
                                  "Zoom level did not decrease to 1.0"));

  // Trigger accelerator for zoom-reset.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_in_accels[0]));
  ASSERT_OK(RunUntilEqual<double>([&]() { return get_zoom_level(); }, 1.1,
                                  "Zoom level did not increase to 1.1"));

  base::span<const ui::Accelerator> zoom_reset_accels =
      LocalHotkeyManager::GetStaticAccelerators(
          LocalHotkeyManager::Hotkey::kZoomReset);
  ASSERT_FALSE(zoom_reset_accels.empty());
  EXPECT_TRUE(focus_manager->ProcessAccelerator(zoom_reset_accels[0]));

  // Verify zoom level reset to 1.0.
  ASSERT_OK(RunUntilEqual<double>([&]() { return get_zoom_level(); }, 1.0,
                                  "Zoom level did not reset to 1.0"));
}

}  // namespace glic
