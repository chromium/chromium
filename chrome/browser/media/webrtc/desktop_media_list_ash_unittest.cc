// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_list_ash.h"

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_list_observer.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list_mock_observer.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

int kThumbnailSize = 100;

using testing::AtLeast;
using testing::DoDefault;

class DesktopMediaListAshTest : public ChromeAshTestBase {
 public:
  DesktopMediaListAshTest() {}

  DesktopMediaListAshTest(const DesktopMediaListAshTest&) = delete;
  DesktopMediaListAshTest& operator=(const DesktopMediaListAshTest&) = delete;

  ~DesktopMediaListAshTest() override {}

  void TearDown() override {
    // Reset the unique_ptr so the list stops refreshing.
    list_.reset();
    ChromeAshTestBase::TearDown();
  }

  void CreateList(DesktopMediaList::Type type) {
    list_ = std::make_unique<DesktopMediaListAsh>(type);
    list_->SetThumbnailSize(gfx::Size(kThumbnailSize, kThumbnailSize));

    // Set update period to reduce the time it takes to run tests.
    list_->SetUpdatePeriod(base::Milliseconds(1));
  }

 protected:
  DesktopMediaListMockObserver observer_;
  std::unique_ptr<DesktopMediaListAsh> list_;
};

ACTION_P2(QuitMessageLoop, quit_closure) {
  std::move(quit_closure).Run();
}

TEST_F(DesktopMediaListAshTest, ScreenOnly) {
  CreateList(DesktopMediaList::Type::kScreen);
  base::RunLoop loop;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  EXPECT_CALL(observer_, OnSourceAdded(0));
  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(QuitMessageLoop(loop.QuitWhenIdleClosure()))
      .WillRepeatedly(DoDefault());

  list_->StartUpdating(&observer_);
  loop.Run();
}

TEST_F(DesktopMediaListAshTest, WindowOnly) {
  CreateList(DesktopMediaList::Type::kWindow);

  {
    base::RunLoop loop1;
    base::RunLoop loop2;
    std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

    EXPECT_CALL(observer_, OnSourceAdded(0));
    EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
        .WillOnce(QuitMessageLoop(loop1.QuitWhenIdleClosure()))
        .WillRepeatedly(DoDefault());
    EXPECT_CALL(observer_, OnSourceRemoved(0))
        .WillOnce(QuitMessageLoop(loop2.QuitWhenIdleClosure()));

    list_->StartUpdating(&observer_);
    loop1.Run();
    window.reset();
    loop2.Run();
  }
  // Tests that a floated window shows up on the list. Regression test for
  // crbug.com/1462516.
  {
    base::RunLoop loop1;
    base::RunLoop loop2;
    std::unique_ptr<aura::Window> float_window = CreateAppWindow();
    ui::test::EventGenerator event_generator(float_window->GetRootWindow());
    event_generator.PressAndReleaseKey(ui::VKEY_F,
                                       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);

    EXPECT_CALL(observer_, OnSourceAdded(0));
    EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
        .WillOnce(QuitMessageLoop(loop1.QuitWhenIdleClosure()))
        .WillRepeatedly(DoDefault());
    EXPECT_CALL(observer_, OnSourceRemoved(0))
        .WillOnce(QuitMessageLoop(loop2.QuitWhenIdleClosure()));

    loop1.Run();
    float_window.reset();
    loop2.Run();
  }
}
