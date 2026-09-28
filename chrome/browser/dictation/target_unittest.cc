// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/target.h"

#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/editable_level.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"

namespace dictation {

namespace {

class TestTarget : public Target {
 public:
  using Target::Target;

  const std::u16string& last_sent_composition() const {
    return last_sent_composition_;
  }
  const std::u16string& last_sent_commit() const { return last_sent_commit_; }
  const std::u16string& last_sent_paste() const { return last_sent_paste_; }

  void ManuallyCompleteCompositionCallbacks() { auto_run_callbacks_ = false; }

  void RunPendingCallback() {
    ASSERT_TRUE(pending_callback_);
    std::move(pending_callback_).Run();
  }

  void set_text_preceding_selection(
      std::optional<std::u16string> text_preceding_selection) {
    text_preceding_selection_ = std::move(text_preceding_selection);
  }

  // Simulates whether the widget has native focus, e.g. false while the user
  // is interacting with the Dictation UI. The test RenderWidgetHostView always
  // reports that it has focus, so this can't be simulated directly.
  void set_has_native_focus(bool has_native_focus) {
    set_has_native_focus_for_testing(has_native_focus);
  }

 private:
  void SetExternallySourcedComposition(
      const std::u16string& text,
      const std::vector<ui::ImeTextSpan>& spans,
      base::OnceClosure on_complete) override {
    last_sent_composition_ = text;

    SetCompletionCallback(std::move(on_complete));
  }

  void CommitExternallySourcedComposition(
      const std::u16string& text,
      base::OnceClosure on_complete) override {
    last_sent_commit_ = text;

    SetCompletionCallback(std::move(on_complete));
  }

  void PasteIntoNode(const std::u16string& text) override {
    last_sent_paste_ = text;
  }

  std::optional<std::u16string_view> GetTextPrecedingSelection() override {
    return text_preceding_selection_;
  }

  void SetCompletionCallback(base::OnceClosure on_complete) {
    if (!on_complete) {
      return;
    }

    ASSERT_FALSE(pending_callback_);
    pending_callback_ = std::move(on_complete);
    if (auto_run_callbacks_) {
      RunPendingCallback();
    }
  }

  // Whether the composition completion callbacks are run automatically. Can be
  // set to false to have precise control of ordering.
  bool auto_run_callbacks_ = true;
  base::OnceClosure pending_callback_;

  std::u16string last_sent_composition_;
  std::u16string last_sent_commit_;
  std::u16string last_sent_paste_;
  std::optional<std::u16string> text_preceding_selection_;
};

class DictationTargetTest : public ChromeRenderViewHostTestHarness {
 public:
  DictationTargetTest() = default;
  ~DictationTargetTest() override = default;

  content::GlobalDOMNodeId MockTargetInMainFrame(int dom_node_id) {
    return content::GlobalDOMNodeId{main_rfh()->GetWeakDocumentPtr(),
                                    blink::DOMNodeIdType(dom_node_id)};
  }

  content::FocusedNodeDetails MakeFocusChange(int dom_node_id) {
    content::FocusedNodeDetails details;
    details.focus_type = blink::mojom::FocusType::kMouse;
    details.editable_level = content::EditableLevel::kPlaintextEditable;
    details.global_dom_node_id = MockTargetInMainFrame(dom_node_id);
    return details;
  }
};

TEST_F(DictationTargetTest, ComposeThenCommit) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));

  EXPECT_EQ(target.last_sent_composition(), u"");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.SetComposition(u"hello", true);
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.SetComposition(u"hello world", true);
  EXPECT_EQ(target.last_sent_composition(), u"hello world");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.CommitComposition(u"hello world", base::NullCallback());
  EXPECT_EQ(target.last_sent_composition(), u"hello world");
  EXPECT_EQ(target.last_sent_commit(), u"hello world");
}

TEST_F(DictationTargetTest, FocusChangeDuringComposition) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));

  target.SetComposition(u"A", true);
  EXPECT_EQ(target.last_sent_composition(), u"A");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.OnFocusChanged(MakeFocusChange(2));

  // Focus was lost, so we don't update the composition.
  target.SetComposition(u"AB", true);
  EXPECT_EQ(target.last_sent_composition(), u"A");
  EXPECT_EQ(target.last_sent_commit(), u"");

  // When we commit, the previous composition would have been committed, so we
  // don't commit the same text again.
  target.CommitComposition(u"ABC", base::NullCallback());
  EXPECT_EQ(target.last_sent_composition(), u"A");
  EXPECT_EQ(target.last_sent_commit(), u"BC");
}

TEST_F(DictationTargetTest, FocusChangeBeforeComposition) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));

  // If we haven't started composing yet, focus changes don't disrupt our
  // ability to compose later.
  target.OnFocusChanged(MakeFocusChange(2));

  target.SetComposition(u"A", true);
  EXPECT_EQ(target.last_sent_composition(), u"A");
  EXPECT_EQ(target.last_sent_commit(), u"");
}

TEST_F(DictationTargetTest, RichlyEditableSingleLineCommitsViaIme) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/true));

  target.SetComposition(u"This is a test", true);
  EXPECT_EQ(target.last_sent_composition(), u"This is a test");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"");

  target.CommitComposition(u"This is a test", base::NullCallback());
  EXPECT_EQ(target.last_sent_commit(), u"This is a test");
  EXPECT_EQ(target.last_sent_paste(), u"");
}

TEST_F(DictationTargetTest, RichlyEditableNewlinePasteFallback) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails{target_id, /*richly_editable=*/true});

  target.SetComposition(u"hello", true);
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"");

  target.SetComposition(u"hello\nworld", true);
  // When a newline is encountered, stop composition.
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"");

  target.SetComposition(u"hello\nworld again", true);
  // Do not compose additional text in the fallback state.
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"");

  target.CommitComposition(u"hello\nworld again.", base::NullCallback());
  // The entire text should be pasted.
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"hello\nworld again.");
}

TEST_F(DictationTargetTest, RichlyEditableNewlinePasteFallbackWithFocusLoss) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails{target_id, /*richly_editable=*/true});

  target.SetComposition(u"hello", true);
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"");

  target.SetComposition(u"hello\nworld", true);
  // When a newline is encountered, stop composition.
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"");

  target.OnFocusChanged(MakeFocusChange(2));

  // As the composition was cleared before the focus change committed anything,
  // the entire text should be pasted.
  target.CommitComposition(u"hello\nworld again.", base::NullCallback());
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"hello\nworld again.");
}

TEST_F(DictationTargetTest, RichlyEditableFocusLossBeforeNewline) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails{target_id, /*richly_editable=*/true});

  target.SetComposition(u"hello", true);
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"");

  target.OnFocusChanged(MakeFocusChange(2));

  // As the composition was committed by the focus change, only the remaining
  // text should be pasted.
  target.CommitComposition(u"hello\nworld again.", base::NullCallback());
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"\nworld again.");
}

TEST_F(DictationTargetTest, NonRichlyEditableNewlineNoPasteFallback) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails{target_id, /*richly_editable=*/false});

  target.SetComposition(u"hello\nworld", true);
  EXPECT_EQ(target.last_sent_composition(), u"hello\nworld");
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_EQ(target.last_sent_paste(), u"");

  target.CommitComposition(u"hello\nworld", base::NullCallback());
  EXPECT_EQ(target.last_sent_composition(), u"hello\nworld");
  EXPECT_EQ(target.last_sent_commit(), u"hello\nworld");
  EXPECT_EQ(target.last_sent_paste(), u"");
}

TEST_F(DictationTargetTest, AsyncCompositionQueuesMostRecentPartial) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kDictation, {{"show_partials", "true"}});

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));
  target.ManuallyCompleteCompositionCallbacks();

  target.SetComposition(u"A", false);
  EXPECT_EQ(target.last_sent_composition(), u"A");

  // Intermediate updates while "A" is in progress.
  target.SetComposition(u"AB", false);
  target.SetComposition(u"ABC", false);
  EXPECT_EQ(target.last_sent_composition(), u"A");

  // "A" completes. Target should skip "AB" and proceed directly to "ABC".
  target.RunPendingCallback();
  EXPECT_EQ(target.last_sent_composition(), u"ABC");

  // "ABC" completes.
  target.RunPendingCallback();
}

TEST_F(DictationTargetTest, AsyncCompositionQueuesCommit) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));
  target.ManuallyCompleteCompositionCallbacks();

  target.SetComposition(u"A", true);
  EXPECT_EQ(target.last_sent_composition(), u"A");

  target.SetComposition(u"AB", true);

  base::test::TestFuture<void> commit_future;
  target.CommitComposition(u"ABC", commit_future.GetCallback());
  EXPECT_EQ(target.last_sent_commit(), u"");
  EXPECT_FALSE(commit_future.IsReady());

  // "A" completes. Target should proceed directly to the commit.
  target.RunPendingCallback();
  EXPECT_EQ(target.last_sent_commit(), u"ABC");
  EXPECT_FALSE(commit_future.IsReady());

  // Commit completes.
  target.RunPendingCallback();
  EXPECT_TRUE(commit_future.Wait());
}

TEST_F(DictationTargetTest, FocusChangeDuringInFlightComposition) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));
  target.ManuallyCompleteCompositionCallbacks();

  target.SetComposition(u"hello", true);
  EXPECT_EQ(target.last_sent_composition(), u"hello");

  target.CommitComposition(u"hello world", base::NullCallback());
  EXPECT_EQ(target.last_sent_commit(), u"");

  // Focus changes while "hello" is in progress.
  target.OnFocusChanged(MakeFocusChange(2));

  // "hello" completes. Target proceeds to commit the remaining text.
  target.RunPendingCallback();
  EXPECT_EQ(target.last_sent_commit(), u" world");

  target.RunPendingCallback();
}

TEST_F(DictationTargetTest, SuccessiveDictationPrependsWhitespace) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));
  target.set_text_preceding_selection(u"This is a test.");

  target.SetComposition(u"This is another test.", true);
  EXPECT_EQ(target.last_sent_composition(), u" This is another test");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.CommitComposition(u"This is another test.", base::NullCallback());
  EXPECT_EQ(target.last_sent_composition(), u" This is another test");
  EXPECT_EQ(target.last_sent_commit(), u" This is another test");
}

TEST_F(DictationTargetTest, WhitespacePrependedWithPasteFallback) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/true));
  target.set_text_preceding_selection(u"Hello.");

  // Multiline composition triggers paste fallback in richly editable targets.
  target.SetComposition(u"world\nagain", true);
  target.CommitComposition(u"world\nagain", base::NullCallback());
  EXPECT_EQ(target.last_sent_paste(), u" world\nagain");
}

TEST_F(DictationTargetTest, TargetTrimsTrailingPeriodForSingleSentence) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));

  target.SetComposition(u"Jane Doe.", true);
  EXPECT_EQ(target.last_sent_composition(), u"Jane Doe");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.CommitComposition(u"Jane Doe.", base::NullCallback());
  EXPECT_EQ(target.last_sent_composition(), u"Jane Doe");
  EXPECT_EQ(target.last_sent_commit(), u"Jane Doe");
}

TEST_F(DictationTargetTest, TargetPreservesTrailingPeriodForMultipleSentences) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));

  target.SetComposition(u"First sentence. Second sentence.", true);
  EXPECT_EQ(target.last_sent_composition(),
            u"First sentence. Second sentence.");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.CommitComposition(u"First sentence. Second sentence.",
                           base::NullCallback());
  EXPECT_EQ(target.last_sent_composition(),
            u"First sentence. Second sentence.");
  EXPECT_EQ(target.last_sent_commit(), u"First sentence. Second sentence.");
}

TEST_F(DictationTargetTest, TakesPageFocusToCommitAndRestoresItAfterwards) {
  content::RenderWidgetHost* rwh = main_rfh()->GetRenderWidgetHost();
  rwh->Blur();
  ASSERT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/true));
  target.ManuallyCompleteCompositionCallbacks();
  // The user is interacting with the Dictation UI, so the widget doesn't
  // have native focus.
  target.set_has_native_focus(false);

  // The page must be focused while we write to it, otherwise sites which
  // disable editability on blur will reject the commit.
  target.CommitComposition(u"hello world", base::NullCallback());
  EXPECT_TRUE(content::IsRenderWidgetHostFocused(rwh));
  EXPECT_EQ(target.last_sent_commit(), u"hello world");

  // Once the operation completes, the page is returned to its real (unfocused)
  // state.
  target.RunPendingCallback();
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));
}

TEST_F(DictationTargetTest, TakesPageFocusToCommitAfterFocusedNodeChanged) {
  content::RenderWidgetHost* rwh = main_rfh()->GetRenderWidgetHost();
  rwh->Blur();
  ASSERT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/true));
  target.ManuallyCompleteCompositionCallbacks();
  // The user is interacting with the Dictation UI, so the widget doesn't
  // have native focus.
  target.set_has_native_focus(false);

  target.SetComposition(u"hello", true);
  target.RunPendingCallback();
  target.OnFocusChanged(MakeFocusChange(2));

  // Only the text that wasn't already committed by the focus change is written,
  // and we still take page focus to write it.
  target.CommitComposition(u"hello world", base::NullCallback());
  EXPECT_TRUE(content::IsRenderWidgetHostFocused(rwh));
  EXPECT_EQ(target.last_sent_commit(), u" world");
  EXPECT_EQ(target.last_sent_paste(), u"");

  target.RunPendingCallback();
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));
}

TEST_F(DictationTargetTest, KeepsPageFocusStateIfWidgetHasNativeFocus) {
  content::RenderWidgetHost* rwh = main_rfh()->GetRenderWidgetHost();
  rwh->Blur();
  ASSERT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/true));
  target.ManuallyCompleteCompositionCallbacks();
  target.set_has_native_focus(true);

  // The page is only blurred as a result of the widget losing native focus, so
  // a widget that still has native focus leaves the page's focus state
  // unchanged.
  target.CommitComposition(u"hello", base::NullCallback());
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  target.RunPendingCallback();
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));
}

TEST_F(DictationTargetTest, KeepsPageFocusIfWidgetRegainsNativeFocus) {
  content::RenderWidgetHost* rwh = main_rfh()->GetRenderWidgetHost();
  rwh->Blur();
  ASSERT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/true));
  target.ManuallyCompleteCompositionCallbacks();
  // The user is interacting with the Dictation UI, so the widget doesn't have
  // native focus.
  target.set_has_native_focus(false);

  target.CommitComposition(u"hello", base::NullCallback());
  EXPECT_TRUE(content::IsRenderWidgetHostFocused(rwh));

  // Simulates the widget regaining native focus while we're committing, e.g.
  // the user switching back to the tab.
  target.set_has_native_focus(true);

  // We shouldn't blur a widget that genuinely has focus.
  target.RunPendingCallback();
  EXPECT_TRUE(content::IsRenderWidgetHostFocused(rwh));
}

TEST_F(DictationTargetTest, TakesPageFocusForFinalComposition) {
  content::RenderWidgetHost* rwh = main_rfh()->GetRenderWidgetHost();
  rwh->Blur();
  ASSERT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/true));
  target.ManuallyCompleteCompositionCallbacks();
  // The user is interacting with the Dictation UI, so the widget doesn't
  // have native focus.
  target.set_has_native_focus(false);

  target.SetComposition(u"hello", true);
  EXPECT_TRUE(content::IsRenderWidgetHostFocused(rwh));
  EXPECT_EQ(target.last_sent_composition(), u"hello");

  target.RunPendingCallback();
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));
}

TEST_F(DictationTargetTest, DoesNotTakePageFocusForDroppedPartials) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kDictation, {{"show_partials", "false"}});

  content::RenderWidgetHost* rwh = main_rfh()->GetRenderWidgetHost();
  rwh->Blur();
  ASSERT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/true));

  // Partials that aren't shown never reach the renderer, so they must not churn
  // the page's focus state.
  target.SetComposition(u"hello", false);
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));
  EXPECT_EQ(target.last_sent_composition(), u"");
}

TEST_F(DictationTargetTest, HoldsPageFocusAcrossQueuedOperations) {
  content::RenderWidgetHost* rwh = main_rfh()->GetRenderWidgetHost();
  rwh->Blur();
  ASSERT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/false));
  target.ManuallyCompleteCompositionCallbacks();
  // The user is interacting with the Dictation UI, so the widget doesn't
  // have native focus.
  target.set_has_native_focus(false);

  target.SetComposition(u"hello", true);
  EXPECT_TRUE(content::IsRenderWidgetHostFocused(rwh));

  // Queue a commit while the composition is still in flight.
  target.CommitComposition(u"hello world", base::NullCallback());

  // Completing the composition starts the queued commit. Page focus is held
  // across both operations rather than being toggled in between.
  target.RunPendingCallback();
  EXPECT_TRUE(content::IsRenderWidgetHostFocused(rwh));
  EXPECT_EQ(target.last_sent_commit(), u"hello world");

  target.RunPendingCallback();
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));
}

TEST_F(DictationTargetTest, ReleasesPageFocusIfTargetDestroyed) {
  content::RenderWidgetHost* rwh = main_rfh()->GetRenderWidgetHost();
  rwh->Blur();
  ASSERT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  auto target = std::make_unique<TestTarget>(
      TargetDetails(target_id, /*richly_editable=*/true));
  target->ManuallyCompleteCompositionCallbacks();
  // The user is interacting with the Dictation UI, so the widget doesn't have
  // native focus.
  target->set_has_native_focus(false);

  target->CommitComposition(u"hello", base::NullCallback());
  EXPECT_TRUE(content::IsRenderWidgetHostFocused(rwh));

  // The target may be destroyed before the operation completes.
  target.reset();
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));
}

TEST_F(DictationTargetTest, DoesNotTakePageFocusWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kDictationRefocusBeforeCommit);

  content::RenderWidgetHost* rwh = main_rfh()->GetRenderWidgetHost();
  rwh->Blur();
  ASSERT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(TargetDetails(target_id, /*richly_editable=*/true));

  target.SetComposition(u"hello", true);
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));

  target.CommitComposition(u"hello world", base::NullCallback());
  EXPECT_FALSE(content::IsRenderWidgetHostFocused(rwh));
}

}  // namespace

}  // namespace dictation
