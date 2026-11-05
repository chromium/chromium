// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_prewarmer.h"

#include <memory>

#include "base/process/kill.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_utils.h"
#include "url/gurl.h"

namespace glic {
namespace {

class FakeColorProviderSource : public ui::ColorProviderSource {
 public:
  const ui::ColorProvider* GetColorProvider() const override {
    return &color_provider_;
  }
  ui::ColorProviderKey GetColorProviderKey() const override { return {}; }
  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    return {};
  }

 private:
  ui::ColorProvider color_provider_;
};

selection::InteractionOptionsPtr Options(bool hide_handles) {
  return selection::InteractionOptions::New(hide_handles,
                                            /*disable_multi_select=*/true,
                                            /*skip_intro_animation=*/true);
}

class SelectionOverlayPrewarmerTest : public ChromeRenderViewHostTestHarness {
 public:
  explicit SelectionOverlayPrewarmerTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME)
      : ChromeRenderViewHostTestHarness(time_source) {}

  void TearDown() override {
    // The harness checks for leaked renderers before members are destroyed.
    prewarmer_.Cancel();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  // Starts a prewarm for `options` and returns its WebContents.
  content::WebContents* StartPrewarm(selection::InteractionOptionsPtr options) {
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    content::WebContents* raw_contents = contents.get();
    prewarmer_.Start(std::move(contents), color_source_, std::move(options));
    return raw_contents;
  }

  FakeColorProviderSource color_source_;
  SelectionOverlayPrewarmer prewarmer_;
};

TEST_F(SelectionOverlayPrewarmerTest, UsableOnlyForSameOptions) {
  content::WebContents* contents = StartPrewarm(Options(true));
  EXPECT_TRUE(prewarmer_.Contains(contents));
  EXPECT_TRUE(prewarmer_.IsUsable(Options(true)));
  EXPECT_FALSE(prewarmer_.IsUsable(Options(false)));
}

TEST_F(SelectionOverlayPrewarmerTest, DroppedWhenRendererGoesAway) {
  content::WebContents* contents = StartPrewarm(Options(true));
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents, GURL("https://example.com/"));
  ASSERT_TRUE(prewarmer_.Contains(contents));

  static_cast<content::MockRenderProcessHost*>(
      contents->GetPrimaryMainFrame()->GetProcess())
      ->SimulateCrash();
  EXPECT_FALSE(prewarmer_.Contains(contents));
  EXPECT_FALSE(prewarmer_.IsUsable(Options(true)));
}

TEST_F(SelectionOverlayPrewarmerTest, NotUsableOnErrorPage) {
  content::WebContents* contents = StartPrewarm(Options(true));
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      contents, GURL("https://example.com/"), net::ERR_FAILED);
  EXPECT_FALSE(prewarmer_.IsUsable(Options(true)));
}

// Mock time only where needed: the simulated navigations in the other tests
// wait in run loops, which mock time can fast-forward.
class SelectionOverlayPrewarmerTimeoutTest
    : public SelectionOverlayPrewarmerTest {
 public:
  SelectionOverlayPrewarmerTimeoutTest()
      : SelectionOverlayPrewarmerTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(SelectionOverlayPrewarmerTimeoutTest,
       DroppedAfterTimeoutUnlessExtended) {
  content::WebContents* contents = StartPrewarm(Options(true));
  task_environment()->FastForwardBy(base::Seconds(9));
  prewarmer_.Extend();
  task_environment()->FastForwardBy(base::Seconds(9));
  EXPECT_TRUE(prewarmer_.Contains(contents));

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(prewarmer_.IsUsable(Options(true)));
  EXPECT_FALSE(prewarmer_.GetInteractionOptions());
}

TEST_F(SelectionOverlayPrewarmerTest, TakeAndCancelEmptyIt) {
  content::WebContents* contents = StartPrewarm(Options(true));
  std::unique_ptr<content::WebContents> taken = prewarmer_.Take();
  EXPECT_EQ(taken.get(), contents);
  EXPECT_FALSE(prewarmer_.Contains(contents));
  EXPECT_FALSE(prewarmer_.GetInteractionOptions());

  StartPrewarm(Options(true));
  prewarmer_.Cancel();
  EXPECT_FALSE(prewarmer_.IsUsable(Options(true)));
  EXPECT_FALSE(prewarmer_.GetInteractionOptions());
}

}  // namespace
}  // namespace glic
