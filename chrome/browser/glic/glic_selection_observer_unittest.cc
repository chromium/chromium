// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/test/test_clipboard.h"

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

  void DismissUI(bool keep_nudge) override {
    dismiss_ui_called_ = true;
    dismiss_ui_kept_nudge_ = keep_nudge;
    GlicSelectionObserver::DismissUI(keep_nudge);
  }

  const std::optional<std::u16string>& last_processed_text() const {
    return last_processed_text_;
  }

  int update_count() const { return update_count_; }

  bool dismiss_ui_called() const { return dismiss_ui_called_; }
  bool dismiss_ui_kept_nudge() const { return dismiss_ui_kept_nudge_; }

  void Reset() {
    last_processed_text_.reset();
    update_count_ = 0;
    dismiss_ui_called_ = false;
    dismiss_ui_kept_nudge_ = false;
  }

  // Expose methods for testing.
  using GlicSelectionObserver::OnInputEvent;
  using GlicSelectionObserver::RenderFrameCreated;
  using GlicSelectionObserver::RenderFrameDeleted;

 protected:
  bool IsSelectionPromptEnabled() const override { return true; }

 private:
  std::optional<std::u16string> last_processed_text_;
  int update_count_ = 0;
  bool dismiss_ui_called_ = false;
  bool dismiss_ui_kept_nudge_ = false;
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

  bool ShouldShowSelectionWidget() {
    return static_cast<GlicSelectionObserver*>(observer_.get())
        ->ShouldShowSelectionWidget();
  }

  void OnWidgetDismissed() {
    static_cast<GlicSelectionObserver*>(observer_.get())->OnWidgetDismissed();
  }

  void CallOnLinkGenerated(
      const GURL& fallback_url,
      const std::string& selector,
      shared_highlighting::LinkGenerationError error,
      shared_highlighting::LinkGenerationReadyStatus ready_status) {
    observer_->OnLinkGenerated(fallback_url, selector, error, ready_status);
  }

  void CallCopyLinkToHighlight(content::WeakDocumentPtr weak_document_ptr) {
    observer_->CopyLinkToHighlight(weak_document_ptr);
  }

  std::optional<GURL> GetGeneratedLink() const {
    return observer_->generated_link_;
  }

  content::RenderWidgetHost* GetRenderWidgetHost() {
    return web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  }

  size_t GetObservedFramesCount() const {
    return observer_->observed_frames_.size();
  }
};

TEST_F(GlicSelectionObserverTest, ObserverDeduplicatesRenderWidgetHosts) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  NavigateAndCommit(GURL("http://example.com"));
  content::RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();

  // Create a child frame. It should share the RenderWidgetHost with the main
  // frame.
  content::RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh)->AppendChild("child");
  ASSERT_TRUE(child_rfh);
  ASSERT_EQ(main_rfh->GetRenderWidgetHost(), child_rfh->GetRenderWidgetHost());

  // Trigger RenderFrameCreated for both.
  observer->RenderFrameCreated(main_rfh);
  observer->RenderFrameCreated(child_rfh);

  EXPECT_EQ(2u, GetObservedFramesCount());

  // Removing the child frame should remove it from the map, but not the main
  // frame.
  observer->RenderFrameDeleted(child_rfh);
  EXPECT_EQ(1u, GetObservedFramesCount());

  // We can't directly check the internal observer list of RenderWidgetHost
  // without exposing it in test headers, but we can verify our observer's map
  // handles the duplicate RenderWidgetHost correctly.

  observer->RenderFrameDeleted(main_rfh);
  EXPECT_EQ(0u, GetObservedFramesCount());
}

TEST_F(GlicSelectionObserverTest, SelectionUpdatesDebounced) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  std::u16string selected_text = u"Hello World";
  observer->OnTextSelectionChanged(nullptr, selected_text);

  // Should be debounced.
  EXPECT_EQ(0, observer->update_count());

  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(selected_text, *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, MultipleSelectionUpdatesDebounced) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // First update.
  observer->OnTextSelectionChanged(nullptr, u"First");
  EXPECT_EQ(0, observer->update_count());

  // Second update before the timer fires.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  observer->OnTextSelectionChanged(nullptr, u"Second");
  EXPECT_EQ(0, observer->update_count());

  // Fast forward to fire the timer for the second update.
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Verify updated with the second text.
  EXPECT_EQ(1, observer->update_count());
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

TEST_F(GlicSelectionObserverTest, TooShortSelectionIgnored) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // kMinSelectionLength is 3.
  std::u16string short_text(2, 'a');
  observer->OnTextSelectionChanged(nullptr, short_text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be treated as clearing (empty text).
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, WhitespaceIgnoredInLengthCheck) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // "a b " has 2 non-whitespace characters.
  std::u16string text = u"a b ";
  observer->OnTextSelectionChanged(nullptr, text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be treated as clearing (empty text) because it has < 3
  // non-whitespace chars.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"", *observer->last_processed_text());

  observer->Reset();

  // "a b c" has 3 non-whitespace characters.
  text = u"a b c";
  observer->OnTextSelectionChanged(nullptr, text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be accepted.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(text, *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, SelectionTrimmed) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // "  abc  " should be trimmed to "abc".
  std::u16string text = u"  abc  ";
  observer->OnTextSelectionChanged(nullptr, text);
  task_environment()->FastForwardBy(base::Milliseconds(300));

  // Should be accepted and trimmed.
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"abc", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, DebounceRestarted) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  // First update.
  observer->OnTextSelectionChanged(nullptr, u"First");
  EXPECT_EQ(0, observer->update_count());

  // Advance time partially (150ms).
  task_environment()->FastForwardBy(base::Milliseconds(150));
  EXPECT_EQ(0, observer->update_count());

  // Second update. Timer should be restarted.
  observer->OnTextSelectionChanged(nullptr, u"Second");
  EXPECT_EQ(0, observer->update_count());

  // Wait 100ms. Total since second update = 100ms.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(0, observer->update_count());

  // Wait 150ms. Total since second update = 250ms ( > 200ms).
  task_environment()->FastForwardBy(base::Milliseconds(150));
  EXPECT_EQ(1, observer->update_count());
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
  // Should be debounced.
  EXPECT_EQ(0, observer->update_count());

  task_environment()->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(1, observer->update_count());
  ASSERT_TRUE(observer->last_processed_text().has_value());
  EXPECT_EQ(u"Mouse Selection", *observer->last_processed_text());
}

TEST_F(GlicSelectionObserverTest, InputEventsDismissUI) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  ASSERT_TRUE(rwh);

  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);

  // Keyboard events should dismiss UI with keep_nudge = false.
  // The nudge should be dismissed.
  // DismissUI posts a task to update the nudge label. To verify the
  // expectations we must wait for this posted task to run. The posted task
  // calls GetBrowserWindowInterface() on the TabInterface. By hooking into this
  // mock call, we can quit the RunLoop, ensuring the test waits exactly until
  // the async task executes before proceeding to VerifyAndClearExpectations.
  base::RunLoop run_loop_keyboard;
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillOnce(testing::InvokeWithoutArgs(
          [&run_loop_keyboard]() -> BrowserWindowInterface* {
            run_loop_keyboard.Quit();
            return nullptr;
          }));
  blink::WebKeyboardEvent key_event(
      blink::WebInputEvent::Type::kKeyDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, key_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_FALSE(observer->dismiss_ui_kept_nudge());
  run_loop_keyboard.Run();
  testing::Mock::VerifyAndClearExpectations(&mock_tab);
  observer->Reset();

  // Mouse clicks should dismiss UI with keep_nudge = false.
  // The nudge should be dismissed.
  // We use the same RunLoop trick as above to wait for the posted task to run.
  base::RunLoop run_loop_mouse;
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillOnce(testing::InvokeWithoutArgs(
          [&run_loop_mouse]() -> BrowserWindowInterface* {
            run_loop_mouse.Quit();
            return nullptr;
          }));
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  observer->OnInputEvent(*rwh, mouse_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_FALSE(observer->dismiss_ui_kept_nudge());
  run_loop_mouse.Run();
  testing::Mock::VerifyAndClearExpectations(&mock_tab);
  observer->Reset();

  // Scroll events should dismiss UI with keep_nudge = true.
  // The nudge should NOT be dismissed.
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface()).Times(0);
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::Type::kMouseWheel,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  observer->OnInputEvent(*rwh, scroll_event,
                         content::RenderWidgetHost::InputEventObserver::
                             InputEventSource::kUnknown);
  EXPECT_TRUE(observer->dismiss_ui_called());
  EXPECT_TRUE(observer->dismiss_ui_kept_nudge());
  testing::Mock::VerifyAndClearExpectations(&mock_tab);
  observer->Reset();
}

TEST_F(GlicSelectionObserverTest, OnLinkGeneratedSuccess) {
  GURL fallback_url("https://example.com");
  std::string selector = "test-selector";

  CallOnLinkGenerated(
      fallback_url, selector, shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);

  EXPECT_TRUE(GetGeneratedLink().has_value());
  EXPECT_EQ(GetGeneratedLink().value().spec(),
            "https://example.com/#:~:text=test-selector");
}

TEST_F(GlicSelectionObserverTest, OnLinkGeneratedEmptySelector) {
  GURL fallback_url("https://example.com");
  std::string selector = "";

  CallOnLinkGenerated(
      fallback_url, selector,
      shared_highlighting::LinkGenerationError::kEmptySelection,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);

  EXPECT_FALSE(GetGeneratedLink().has_value());
}

TEST_F(GlicSelectionObserverTest, CopyLinkToHighlight) {
  ui::TestClipboard* clipboard = ui::TestClipboard::CreateForCurrentThread();

  NavigateAndCommit(GURL("https://example.com"));

  GURL fallback_url("https://example.com");
  std::string selector = "test-selector";

  CallOnLinkGenerated(
      fallback_url, selector, shared_highlighting::LinkGenerationError::kNone,
      shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);

  // Trigger copy to clipboard.
  CallCopyLinkToHighlight(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr());

  // Allow clipboard async operations to complete and verify the contents.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    base::test::TestFuture<std::u16string> future;
    clipboard->ReadText(ui::ClipboardBuffer::kCopyPaste, std::nullopt,
                        future.GetCallback());
    return base::UTF16ToUTF8(future.Get()) ==
           "https://example.com/#:~:text=test-selector";
  }));

  ui::Clipboard::DestroyClipboardForCurrentThread();
}

TEST_F(GlicSelectionObserverTest, WidgetFrequencyCapping) {
  auto* observer = GetObserver();
  ASSERT_TRUE(observer);

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

  // Initially we should be able to show the widget.
  EXPECT_TRUE(ShouldShowSelectionWidget());

  // Test total dismiss capping.
  prefs->SetInteger(
      prefs::kGlicSelectionWidgetDismissCount,
      features::kGlicSelectionPromptWidgetMaxTotalDismisses.Get());
  EXPECT_FALSE(ShouldShowSelectionWidget());

  // Reset total dismiss capping.
  prefs->SetInteger(prefs::kGlicSelectionWidgetDismissCount, 0);
  EXPECT_TRUE(ShouldShowSelectionWidget());
}

}  // namespace glic
