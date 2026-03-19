// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

class TestGlicSelectionObserver : public GlicSelectionObserver {
 public:
  explicit TestGlicSelectionObserver(content::WebContents* web_contents)
      : GlicSelectionObserver(web_contents) {}

  void UpdateSelectionState(const std::u16string& text) override {
    last_processed_text_ = text;
    update_count_++;
  }

  const std::optional<std::u16string>& last_processed_text() const {
    return last_processed_text_;
  }

  int update_count() const { return update_count_; }

  void Reset() {
    last_processed_text_.reset();
    update_count_ = 0;
  }

  // Expose OnInputEvent for testing.
  using GlicSelectionObserver::OnInputEvent;

 private:
  std::optional<std::u16string> last_processed_text_;
  int update_count_ = 0;
};

}  // namespace

class GlicSelectionObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  GlicSelectionObserverTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicSelectionPrompt);
    ChromeRenderViewHostTestHarness::SetUp();

    // Create our test observer.
    observer_ = std::make_unique<TestGlicSelectionObserver>(web_contents());
  }

  void TearDown() override {
    observer_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestGlicSelectionObserver> observer_;

  TestGlicSelectionObserver* GetObserver() { return observer_.get(); }
};

TEST_F(GlicSelectionObserverTest, SelectionUpdatesImmediatelyIfIdle) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  std::u16string selected_text = u"Hello World";
  observer->OnTextSelectionChanged(nullptr, selected_text);

  // Should be immediate.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(selected_text, *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectionUpdatesDebouncedWhenActive) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // First update immediate.
  observer->OnTextSelectionChanged(nullptr, u"First");
  EXPECT_EQ(1, observer->update_count());

  // Second update immediately after.
  observer->OnTextSelectionChanged(nullptr, u"Second");
  EXPECT_EQ(1, observer->update_count());  // Still 1.

  // Fast forward.
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Verify updated.
  EXPECT_EQ(2, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Second", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectionClearsDebounced) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // Set initial selection
  observer->OnTextSelectionChanged(nullptr, u"Initial");
  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(1, observer->update_count());
  observer->Reset();

  // Clear selection
  observer->OnTextSelectionChanged(nullptr, u"");

  // Clearing also debounces.
  EXPECT_EQ(0, observer->update_count());

  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be called with empty string.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, TooLongSelectionIgnored) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // kMaxSelectionLength is 1000.
  std::u16string huge_text(1001, 'a');
  observer->OnTextSelectionChanged(nullptr, huge_text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be treated as clearing (empty text).
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, DebounceRestarted) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // First update immediate.
  observer->OnTextSelectionChanged(nullptr, u"First");
  EXPECT_EQ(1, observer->update_count());

  // Advance time partially (150ms).
  task_environment()->FastForwardBy(base::Milliseconds(150));

  // Second update. Should be debounced because only 150ms passed since first.
  // Delay needed: 300 - 150 = 150ms.
  observer->OnTextSelectionChanged(nullptr, u"Second");
  EXPECT_EQ(1, observer->update_count());

  // Wait 100ms. Total since second update = 100ms.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(1, observer->update_count());

  // Wait 60ms. Total since second update = 160ms ( > 150ms).
  task_environment()->FastForwardBy(base::Milliseconds(60));
  EXPECT_EQ(2, observer->update_count());
  EXPECT_EQ(u"Second", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, KeyboardSelectionIgnored) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // Simulate a keyboard event.
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());

  // We need a dummy RenderWidgetHost, but we can pass a nullptr since it's not
  // used by OnInputEvent in this context except for signature. Wait,
  // OnInputEvent takes `const content::RenderWidgetHost&`, so passing nullptr
  // is UB. Let's just create a dummy WebMouseEvent. Wait, I can use the
  // web_contents's RWH.
  content::RenderWidgetHost* rwh =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);

  // Send a text selection event
  observer->OnTextSelectionChanged(nullptr, u"Keyboard Selection");

  // It should clear the selection immediately
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());

  observer->Reset();

  // Now simulate a mouse down event to clear the keyboard state.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, mouse_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);

  observer->OnTextSelectionChanged(nullptr, u"Mouse Selection");
  // Should update immediately
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Mouse Selection", *observer->last_processed_text());
}

}  // namespace glic
