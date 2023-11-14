// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/accessibility/media_app/ax_media_app.h"
#include "chrome/browser/accessibility/media_app/test/fake_ax_media_app.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "components/services/screen_ai/public/test/fake_screen_ai_annotator.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/ax_platform_node.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ash::test {

namespace {

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
class TestScreenAIInstallState : public screen_ai::ScreenAIInstallState {
 public:
  TestScreenAIInstallState() = default;
  TestScreenAIInstallState(const TestScreenAIInstallState&) = delete;
  TestScreenAIInstallState& operator=(const TestScreenAIInstallState&) = delete;
  ~TestScreenAIInstallState() override = default;

  void SetLastUsageTime() override {}
  void DownloadComponentInternal() override {}
};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

class AXMediaAppUntrustedHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  AXMediaAppUntrustedHandlerTest() : feature_list_(features::kBacklightOcr) {}
  AXMediaAppUntrustedHandlerTest(
      const AXMediaAppUntrustedHandlerTest&) = delete;
  AXMediaAppUntrustedHandlerTest& operator=(
      const AXMediaAppUntrustedHandlerTest&) = delete;
  ~AXMediaAppUntrustedHandlerTest() override = default;

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ASSERT_NE(nullptr, screen_ai::ScreenAIInstallState::GetInstance());
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ASSERT_NE(nullptr, content::BrowserAccessibilityState::GetInstance());

    mojo::PendingRemote<ash::media_app_ui::mojom::OcrUntrustedPage> pageRemote;
    // TODO(b/309860428): Delete MediaApp interface - after we implement all
    // Mojo APIs, it should not be needed any more.
    handler_ = std::make_unique<AXMediaAppUntrustedHandler>(
        *web_contents()->GetBrowserContext(), std::move(pageRemote));

    handler_->SetMediaAppForTesting(&fake_media_app_);
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    handler_->SetScreenAIAnnotatorForTesting(
        fake_annotator_.BindNewPipeAndPassRemote());
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    ASSERT_NE(nullptr, handler_.get());
  }

  void TearDown() override {
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  TestScreenAIInstallState install_state_;
  screen_ai::test::FakeScreenAIAnnotator fake_annotator_{
      /*create_empty_result=*/true};
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  FakeAXMediaApp fake_media_app_;
  std::unique_ptr<AXMediaAppUntrustedHandler> handler_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
TEST_F(AXMediaAppUntrustedHandlerTest, IsOcrServiceEnabled) {
  EXPECT_FALSE(handler_->IsOcrServiceEnabled());
  EXPECT_FALSE(fake_media_app_.IsOcrServiceEnabled());

  screen_ai::ScreenAIInstallState::GetInstance()->SetState(
      screen_ai::ScreenAIInstallState::State::kReady);
  EXPECT_TRUE(handler_->IsOcrServiceEnabled());
  EXPECT_TRUE(fake_media_app_.IsOcrServiceEnabled());

  screen_ai::ScreenAIInstallState::GetInstance()->SetState(
      screen_ai::ScreenAIInstallState::State::kNotDownloaded);
  EXPECT_FALSE(handler_->IsOcrServiceEnabled());
  EXPECT_FALSE(fake_media_app_.IsOcrServiceEnabled());
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

TEST_F(AXMediaAppUntrustedHandlerTest, IsAccessibilityEnabled) {
  EXPECT_FALSE(handler_->IsAccessibilityEnabled());
  EXPECT_FALSE(fake_media_app_.IsAccessibilityEnabled());

  accessibility_state_utils::OverrideIsScreenReaderEnabledForTesting(true);
  ui::AXPlatformNode::NotifyAddAXModeFlags(ui::kAXModeComplete);
  EXPECT_TRUE(handler_->IsAccessibilityEnabled());
  EXPECT_TRUE(fake_media_app_.IsAccessibilityEnabled());
  // Once enabled, accessibility cannot be disabled.
  ui::AXPlatformNode::SetAXMode(ui::AXMode::kNone);
}

}  // namespace ash::test
