// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_list_ash.h"

#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_list_observer.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

int kThumbnailSize = 100;

using testing::AtLeast;
using testing::DoDefault;

class MockDesktopMediaListObserver : public DesktopMediaListObserver {
 public:
  MOCK_METHOD2(OnSourceAdded, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceRemoved, void(DesktopMediaList* list, int index));
  MOCK_METHOD3(OnSourceMoved,
               void(DesktopMediaList* list, int old_index, int new_index));
  MOCK_METHOD2(OnSourceNameChanged, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceThumbnailChanged,
               void(DesktopMediaList* list, int index));
};

class DesktopMediaListAshTest : public ChromeAshTestBase {
 public:
  DesktopMediaListAshTest() {}
  ~DesktopMediaListAshTest() override {}

  void TearDown() override {
    // Reset the unique_ptr so the list stops refreshing.
    list_.reset();
    ChromeAshTestBase::TearDown();
  }

  void CreateList(content::DesktopMediaID::Type type) {
    list_.reset(new DesktopMediaListAsh(type));
    list_->SetThumbnailSize(gfx::Size(kThumbnailSize, kThumbnailSize));

    // Set update period to reduce the time it takes to run tests.
    list_->SetUpdatePeriod(base::TimeDelta::FromMilliseconds(1));
  }

 protected:
  MockDesktopMediaListObserver observer_;
  std::unique_ptr<DesktopMediaListAsh> list_;
  DISALLOW_COPY_AND_ASSIGN(DesktopMediaListAshTest);
};

ACTION(QuitMessageLoop) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
}

TEST_F(DesktopMediaListAshTest, ScreenOnly) {
  CreateList(content::DesktopMediaID::TYPE_SCREEN);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  EXPECT_CALL(observer_, OnSourceAdded(list_.get(), 0));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(list_.get(), 0))
      .WillOnce(QuitMessageLoop())
      .WillRepeatedly(DoDefault());

  list_->StartUpdating(&observer_);
  base::RunLoop().Run();
}

TEST_F(DesktopMediaListAshTest, WindowOnly) {
  CreateList(content::DesktopMediaID::TYPE_WINDOW);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  EXPECT_CALL(observer_, OnSourceAdded(list_.get(), 0));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(list_.get(), 0))
      .WillOnce(QuitMessageLoop())
      .WillRepeatedly(DoDefault());
  EXPECT_CALL(observer_, OnSourceRemoved(list_.get(), 0))
      .WillOnce(QuitMessageLoop());

  list_->StartUpdating(&observer_);
  base::RunLoop().Run();
  window.reset();
  base::RunLoop().Run();
}
