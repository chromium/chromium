// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/target.h"

#include <string>
#include <vector>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/global_dom_node_id.h"
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

 private:
  void SetExternallySourcedComposition(
      const std::u16string& text,
      const std::vector<ui::ImeTextSpan>& spans) override {
    last_sent_composition_ = text;
  }

  void CommitExternallySourcedComposition(const std::u16string& text) override {
    last_sent_commit_ = text;
  }

  std::u16string last_sent_composition_;
  std::u16string last_sent_commit_;
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
    details.is_editable_node = true;
    details.global_dom_node_id = MockTargetInMainFrame(dom_node_id);
    return details;
  }
};

TEST_F(DictationTargetTest, ComposeThenCommit) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(target_id);

  EXPECT_EQ(target.last_sent_composition(), u"");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.SetComposition(u"hello", true);
  EXPECT_EQ(target.last_sent_composition(), u"hello");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.SetComposition(u"hello world", true);
  EXPECT_EQ(target.last_sent_composition(), u"hello world");
  EXPECT_EQ(target.last_sent_commit(), u"");

  target.CommitComposition(u"hello world");
  EXPECT_EQ(target.last_sent_composition(), u"hello world");
  EXPECT_EQ(target.last_sent_commit(), u"hello world");
}

TEST_F(DictationTargetTest, FocusChangeDuringComposition) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(target_id);

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
  target.CommitComposition(u"ABC");
  EXPECT_EQ(target.last_sent_composition(), u"A");
  EXPECT_EQ(target.last_sent_commit(), u"BC");
}

TEST_F(DictationTargetTest, FocusChangeBeforeComposition) {
  content::GlobalDOMNodeId target_id = MockTargetInMainFrame(1);
  TestTarget target(target_id);

  // If we haven't started composing yet, focus changes don't disrupt our
  // ability to compose later.
  target.OnFocusChanged(MakeFocusChange(2));

  target.SetComposition(u"A", true);
  EXPECT_EQ(target.last_sent_composition(), u"A");
  EXPECT_EQ(target.last_sent_commit(), u"");
}

}  // namespace

}  // namespace dictation
