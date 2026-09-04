// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/privileged_web_contents.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace pwc {
namespace {

std::unique_ptr<FixedPwcPolicyDelegate> MakeTestDelegate() {
  return std::make_unique<FixedPwcPolicyDelegate>(
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))},
      std::vector<url::Origin>{
          url::Origin::Create(GURL("https://pwc-test.example.com"))});
}

class PrivilegedWebContentsTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        mojom::features::kPrivilegedWebContents);
    ChromeRenderViewHostTestHarness::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrivilegedWebContentsTest, CreateOwnsAWebContents) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  ASSERT_TRUE(pwc);
  ASSERT_TRUE(pwc->web_contents());
  EXPECT_EQ(pwc->web_contents()->GetBrowserContext(), profile());
  EXPECT_EQ(pwc->component(), PrivilegedComponent::kTestComponent);
  EXPECT_EQ(pwc->policy().component(), PrivilegedComponent::kTestComponent);
  EXPECT_EQ(pwc->web_contents()->GetDelegate(), pwc.get());
}

TEST_F(PrivilegedWebContentsTest, UsesDefaultStoragePartition) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  content::RenderProcessHost* process =
      pwc->web_contents()->GetPrimaryMainFrame()->GetProcess();
  EXPECT_EQ(process->GetStoragePartition(),
            profile()->GetDefaultStoragePartition());
}

TEST_F(PrivilegedWebContentsTest, FromWebContentsRoundTrips) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(pwc->web_contents()),
            pwc.get());
}

TEST_F(PrivilegedWebContentsTest, FromWebContentsIsNullForOrdinaryContents) {
  // The harness's own WebContents is not owned by a PrivilegedWebContents.
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(web_contents()), nullptr);
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(nullptr), nullptr);
}

TEST_F(PrivilegedWebContentsTest, DisablesPrerendering) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  content::WebContents* web_contents = pwc->web_contents();
  // A privileged WebContents never supports prerendering: a prerendered page is
  // activated into the primary main frame without running navigation throttles,
  // which would let an off-allowlist page bypass PwcNavigationThrottle.
  EXPECT_EQ(
      content::PreloadingEligibility::kPreloadingUnsupportedByWebContents,
      web_contents->GetDelegate()->IsPrerender2Supported(
          *web_contents, content::PreloadingTriggerType::kSpeculationRule));
}

TEST_F(PrivilegedWebContentsTest, DestructionIsClean) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  pwc.reset();
  // No crash, and unrelated WebContents are unaffected.
  EXPECT_EQ(PrivilegedWebContents::FromWebContents(web_contents()), nullptr);
}

class TestEmbedderDelegate : public PrivilegedWebContents::EmbedderDelegate {
 public:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    last_keyboard_source_ = source;
    last_event_type_ = event.GetType();
    keyboard_event_count_++;
    return handle_keyboard_return_value_;
  }

  void ContentsZoomChange(bool zoom_in) override {
    last_zoom_in_ = zoom_in;
    zoom_change_count_++;
  }

  raw_ptr<content::WebContents, DisableDanglingPtrDetection>
      last_keyboard_source_ = nullptr;
  std::optional<blink::WebInputEvent::Type> last_event_type_;
  int keyboard_event_count_ = 0;
  bool handle_keyboard_return_value_ = true;
  std::optional<bool> last_zoom_in_;
  int zoom_change_count_ = 0;
};

TEST_F(PrivilegedWebContentsTest, EmbedderDelegateDefaultsToNull) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  EXPECT_EQ(pwc->embedder_delegate(), nullptr);
}

TEST_F(PrivilegedWebContentsTest, SetEmbedderDelegateUpdatesDelegate) {
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  TestEmbedderDelegate delegate;

  pwc->SetEmbedderDelegate(&delegate);
  EXPECT_EQ(pwc->embedder_delegate(), &delegate);

  pwc->SetEmbedderDelegate(nullptr);
  EXPECT_EQ(pwc->embedder_delegate(), nullptr);
}

TEST_F(PrivilegedWebContentsTest, ForwardsKeyboardEventToEmbedderDelegate) {
  TestEmbedderDelegate delegate;
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());
  content::WebContents* web_contents = pwc->web_contents();
  input::NativeWebKeyboardEvent event(blink::WebInputEvent::Type::kRawKeyDown,
                                      blink::WebInputEvent::kNoModifiers,
                                      base::TimeTicks::Now());

  // Returns false when no embedder delegate is set.
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents, event));

  pwc->SetEmbedderDelegate(&delegate);

  delegate.handle_keyboard_return_value_ = true;
  EXPECT_TRUE(pwc->web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents, event));
  EXPECT_EQ(delegate.last_keyboard_source_, web_contents);
  EXPECT_EQ(delegate.last_event_type_, blink::WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(delegate.keyboard_event_count_, 1);

  delegate.handle_keyboard_return_value_ = false;
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents, event));
  EXPECT_EQ(delegate.keyboard_event_count_, 2);

  // Clearing the delegate stops forwarding.
  pwc->SetEmbedderDelegate(nullptr);
  EXPECT_FALSE(pwc->web_contents()->GetDelegate()->HandleKeyboardEvent(
      web_contents, event));
  EXPECT_EQ(delegate.keyboard_event_count_, 2);
  delegate.last_keyboard_source_ = nullptr;
}

TEST_F(PrivilegedWebContentsTest,
       ForwardsContentsZoomChangeToEmbedderDelegate) {
  TestEmbedderDelegate delegate;
  std::unique_ptr<PrivilegedWebContents> pwc = PrivilegedWebContents::Create(
      PrivilegedComponent::kTestComponent, profile(), MakeTestDelegate());

  // Does not crash when no embedder delegate is set.
  pwc->web_contents()->GetDelegate()->ContentsZoomChange(/*zoom_in=*/true);

  pwc->SetEmbedderDelegate(&delegate);

  pwc->web_contents()->GetDelegate()->ContentsZoomChange(/*zoom_in=*/true);
  EXPECT_EQ(delegate.zoom_change_count_, 1);
  EXPECT_EQ(delegate.last_zoom_in_, true);

  pwc->web_contents()->GetDelegate()->ContentsZoomChange(/*zoom_in=*/false);
  EXPECT_EQ(delegate.zoom_change_count_, 2);
  EXPECT_EQ(delegate.last_zoom_in_, false);

  // Clearing the delegate stops forwarding.
  pwc->SetEmbedderDelegate(nullptr);
  pwc->web_contents()->GetDelegate()->ContentsZoomChange(/*zoom_in=*/true);
  EXPECT_EQ(delegate.zoom_change_count_, 2);
}

TEST_F(PrivilegedWebContentsTest, DefaultEmbedderDelegateMethods) {
  PrivilegedWebContents::EmbedderDelegate default_delegate;
  input::NativeWebKeyboardEvent event(blink::WebInputEvent::Type::kRawKeyDown,
                                      blink::WebInputEvent::kNoModifiers,
                                      base::TimeTicks::Now());

  EXPECT_FALSE(default_delegate.HandleKeyboardEvent(web_contents(), event));
  EXPECT_FALSE(default_delegate.HandleKeyboardEvent(/*source=*/nullptr, event));

  // Default ContentsZoomChange is a no-op that does not crash.
  default_delegate.ContentsZoomChange(/*zoom_in=*/true);
  default_delegate.ContentsZoomChange(/*zoom_in=*/false);
}

}  // namespace
}  // namespace pwc
