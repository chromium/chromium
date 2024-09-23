// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/prerewritten_event_forwarder.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"
#include "ui/events/test/test_event_rewriter_continuation.h"

namespace ash {

namespace {

class TestEventRewriterContinuation
    : public ui::test::TestEventRewriterContinuation {
 public:
  TestEventRewriterContinuation() = default;

  TestEventRewriterContinuation(const TestEventRewriterContinuation&) = delete;
  TestEventRewriterContinuation& operator=(
      const TestEventRewriterContinuation&) = delete;

  ~TestEventRewriterContinuation() override = default;

  ui::EventDispatchDetails SendEvent(const ui::Event* event) override {
    passthrough_event_ = event->Clone();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails SendEventFinally(const ui::Event* event) override {
    rewritten_event_ = event->Clone();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails DiscardEvent() override {
    return ui::EventDispatchDetails();
  }

  void Reset() {
    passthrough_event_.reset();
    rewritten_event_.reset();
  }

  bool discarded() { return !(passthrough_event_ || rewritten_event_); }

  std::unique_ptr<ui::Event> passthrough_event_;
  std::unique_ptr<ui::Event> rewritten_event_;

  base::WeakPtrFactory<TestEventRewriterContinuation> weak_ptr_factory_{this};
};

class TestKeyObserver : public PrerewrittenEventForwarder::Observer {
 public:
  TestKeyObserver()
      : key_event_(ui::EventType::kUnknown, ui::VKEY_UNKNOWN, ui::EF_NONE) {}

  void OnPrerewriteKeyInputEvent(const ui::KeyEvent& key_event) override {
    key_event_ = key_event;
  }

  ui::KeyEvent last_observed_key_event() { return key_event_; }

 private:
  ui::KeyEvent key_event_;
};

bool AreKeyEventsEqual(const ui::KeyEvent& lhs, const ui::KeyEvent& rhs) {
  return lhs.key_code() == rhs.key_code() && lhs.flags() == rhs.flags() &&
         lhs.type() == rhs.type();
}

}  // namespace

class PrerewrittenEventForwarderTest : public AshTestBase {
 public:
  PrerewrittenEventForwarderTest() = default;
  PrerewrittenEventForwarderTest(const PrerewrittenEventForwarderTest&) =
      delete;
  PrerewrittenEventForwarderTest& operator=(
      const PrerewrittenEventForwarderTest&) = delete;
  ~PrerewrittenEventForwarderTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    scoped_feature_list_.InitWithFeatures({features::kShortcutCustomization},
                                          {});
    observer_ = std::make_unique<TestKeyObserver>();
    rewriter_ = std::make_unique<PrerewrittenEventForwarder>();
    rewriter_->AddObserver(observer_.get());
  }

  void TearDown() override {
    rewriter_->RemoveObserver(observer_.get());
    rewriter_.reset();
    observer_.reset();
    scoped_feature_list_.Reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestKeyObserver> observer_;
  std::unique_ptr<PrerewrittenEventForwarder> rewriter_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PrerewrittenEventForwarderTest, InitializationTest) {
  EXPECT_NE(rewriter_.get(), nullptr);
}

TEST_F(PrerewrittenEventForwarderTest, RawKeyEventObserverCalled) {
  TestEventRewriterContinuation continuation;
  const ui::KeyEvent expected_key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                                        /*flags=*/0);
  rewriter_->RewriteEvent(expected_key_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  EXPECT_FALSE(continuation.discarded());
  EXPECT_TRUE(AreKeyEventsEqual(expected_key_event,
                                observer_->last_observed_key_event()));

  continuation.Reset();

  const ui::KeyEvent expected_key_event_2(ui::EventType::kKeyReleased,
                                          ui::VKEY_B, ui::EF_COMMAND_DOWN);
  rewriter_->RewriteEvent(expected_key_event_2,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  EXPECT_FALSE(continuation.discarded());
  EXPECT_TRUE(AreKeyEventsEqual(expected_key_event_2,
                                observer_->last_observed_key_event()));
}

TEST_F(PrerewrittenEventForwarderTest, IgnoreRepeatKeyEvents) {
  TestEventRewriterContinuation continuation;
  const ui::KeyEvent expected_key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                                        ui::EF_IS_REPEAT);
  rewriter_->RewriteEvent(expected_key_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  EXPECT_FALSE(continuation.discarded());
  // Expect the event to not have been fired and therefore the last observed
  // key event is empty.
  const ui::KeyEvent empty_key_event(ui::EventType::kUnknown, ui::VKEY_UNKNOWN,
                                     ui::EF_NONE);
  EXPECT_TRUE(
      AreKeyEventsEqual(empty_key_event, observer_->last_observed_key_event()));
}

}  // namespace ash
