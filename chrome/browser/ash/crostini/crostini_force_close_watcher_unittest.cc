// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_force_close_watcher.h"

#include <memory>
#include <type_traits>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace crostini {
namespace {

using ::testing::Return;

class CrostiniForceCloseWatcherTest : public views::test::WidgetTest {};

class MockDelegate : public ForceCloseWatcher::Delegate {
 public:
  MOCK_METHOD(views::Widget*, GetClosableWidget, (), (override));
  MOCK_METHOD(void, Watched, (ForceCloseWatcher * watcher), (override));
  MOCK_METHOD(void, Prompt, (), (override));
  MOCK_METHOD(void, Hide, (), (override));

  ~MockDelegate() override {
    if (delete_flag) {
      EXPECT_FALSE(*delete_flag);
      *delete_flag = true;
    }
  }
  raw_ptr<bool> delete_flag = nullptr;
};

TEST_F(CrostiniForceCloseWatcherTest, CallsHideWhenWidgetIsDestroyed) {
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate& delegate_ref = *delegate;

  WidgetAutoclosePtr widget(CreateTopLevelNativeWidget());

  EXPECT_CALL(delegate_ref, GetClosableWidget).WillOnce(Return(widget.get()));
  ForceCloseWatcher::Watch(std::move(delegate));

  EXPECT_CALL(delegate_ref, Hide).Times(1);
  widget.reset();
}

TEST_F(CrostiniForceCloseWatcherTest, DeletesSelfOnWidgetDeletion) {
  bool deleted = false;
  auto delegate = std::make_unique<testing::NiceMock<MockDelegate>>();
  MockDelegate& delegate_ref = *delegate;

  WidgetAutoclosePtr widget(CreateTopLevelNativeWidget());

  EXPECT_CALL(delegate_ref, GetClosableWidget).WillOnce(Return(widget.get()));
  ForceCloseWatcher::Watch(std::move(delegate));

  delegate_ref.delete_flag = &deleted;
  widget.reset();
  EXPECT_TRUE(deleted);
}

TEST_F(CrostiniForceCloseWatcherTest, CallsForceCloseAfterSecondCloseAttempt) {
  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate& delegate_ref = *delegate;

  WidgetAutoclosePtr widget(CreateTopLevelNativeWidget());

  EXPECT_CALL(delegate_ref, GetClosableWidget).WillOnce(Return(widget.get()));
  EXPECT_CALL(delegate_ref, Watched)
      .WillOnce(testing::Invoke([](ForceCloseWatcher* watcher) {
        watcher->OverrideDelayForTesting(base::Seconds(0));
        watcher->OnCloseRequested();
        watcher->OnCloseRequested();
      }));
  EXPECT_CALL(delegate_ref, Prompt).Times(1);
  EXPECT_CALL(delegate_ref, Hide).Times(1);

  ForceCloseWatcher::Watch(std::move(delegate));
}

TEST_F(CrostiniForceCloseWatcherTest, NoForceCloseUntilDelayPassed) {
  auto delegate = std::make_unique<testing::StrictMock<MockDelegate>>();
  MockDelegate& delegate_ref = *delegate;

  WidgetAutoclosePtr widget(CreateTopLevelNativeWidget());

  EXPECT_CALL(delegate_ref, GetClosableWidget).WillOnce(Return(widget.get()));
  EXPECT_CALL(delegate_ref, Watched)
      .WillOnce(testing::Invoke([](ForceCloseWatcher* watcher) {
        watcher->OnCloseRequested();
        watcher->OnCloseRequested();
      }));
  EXPECT_CALL(delegate_ref, Hide).Times(1);

  ForceCloseWatcher::Watch(std::move(delegate));
}

}  // namespace
}  // namespace crostini
