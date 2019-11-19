// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/native_desktop_media_list.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <utility>
#include <vector>

#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

using content::DesktopMediaID;
using testing::_;
using testing::DoAll;

namespace {

// Aura window capture unit tests are not stable. crbug.com/602494 and
// crbug.com/603823.
// #define ENABLE_AURA_WINDOW_TESTS

static const int kDefaultWindowCount = 2;
#if defined(ENABLE_AURA_WINDOW_TESTS)
static const int kDefaultAuraCount = 1;
#else
static const int kDefaultAuraCount = 0;
#endif

class MockObserver : public DesktopMediaListObserver {
 public:
  MOCK_METHOD2(OnSourceAdded, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceRemoved, void(DesktopMediaList* list, int index));
  MOCK_METHOD3(OnSourceMoved,
               void(DesktopMediaList* list, int old_index, int new_index));
  MOCK_METHOD2(OnSourceNameChanged, void(DesktopMediaList* list, int index));
  MOCK_METHOD2(OnSourceThumbnailChanged,
               void(DesktopMediaList* list, int index));
  MOCK_METHOD1(OnAllSourcesFound, void(DesktopMediaList* list));
};

class FakeScreenCapturer : public webrtc::DesktopCapturer {
 public:
  FakeScreenCapturer() {}
  ~FakeScreenCapturer() override {}

  // webrtc::ScreenCapturer implementation.
  void Start(Callback* callback) override { callback_ = callback; }

  void CaptureFrame() override {
    DCHECK(callback_);
    std::unique_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(10, 10)));
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                               std::move(frame));
  }

  bool GetSourceList(SourceList* screens) override {
    screens->push_back({0});
    return true;
  }

  bool SelectSource(SourceId id) override {
    EXPECT_EQ(0, id);
    return true;
  }

 protected:
  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeScreenCapturer);
};

class FakeWindowCapturer : public webrtc::DesktopCapturer {
 public:
  FakeWindowCapturer() : callback_(nullptr) {}
  ~FakeWindowCapturer() override {}

  void SetWindowList(const SourceList& list) {
    base::AutoLock lock(window_list_lock_);
    window_list_ = list;
  }

  // Sets |value| thats going to be used to memset() content of the frames
  // generated for |window_id|. By default generated frames are set to zeros.
  void SetNextFrameValue(SourceId window_id, int8_t value) {
    base::AutoLock lock(frame_values_lock_);
    frame_values_[window_id] = value;
  }

  // webrtc::WindowCapturer implementation.
  void Start(Callback* callback) override { callback_ = callback; }

  void CaptureFrame() override {
    DCHECK(callback_);

    base::AutoLock lock(frame_values_lock_);

    auto it = frame_values_.find(selected_window_id_);
    int8_t value = (it != frame_values_.end()) ? it->second : 0;
    std::unique_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(10, 10)));
    memset(frame->data(), value, frame->stride() * frame->size().height());
    callback_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                               std::move(frame));
  }

  bool GetSourceList(SourceList* windows) override {
    base::AutoLock lock(window_list_lock_);
    *windows = window_list_;
    return true;
  }

  bool SelectSource(SourceId id) override {
    selected_window_id_ = id;
    return true;
  }

  bool FocusOnSelectedSource() override { return true; }

 private:
  Callback* callback_;
  SourceList window_list_;
  base::Lock window_list_lock_;

  SourceId selected_window_id_;

  // Frames to be captured per window.
  std::map<SourceId, int8_t> frame_values_;
  base::Lock frame_values_lock_;

  DISALLOW_COPY_AND_ASSIGN(FakeWindowCapturer);
};

}  // namespace

ACTION_P2(CheckListSize, model, expected_list_size) {
  EXPECT_EQ(expected_list_size, model->GetSourceCount());
}

ACTION_P2(QuitRunLoop, task_runner, run_loop) {
  task_runner->PostTask(FROM_HERE, run_loop->QuitWhenIdleClosure());
}

class NativeDesktopMediaListTest : public ChromeViewsTestBase {
 public:
  NativeDesktopMediaListTest() = default;

  void TearDown() override {
    for (size_t i = 0; i < desktop_widgets_.size(); i++)
      desktop_widgets_[i].reset();

    ChromeViewsTestBase::TearDown();
  }

  void AddNativeWindow(int id) {
    webrtc::DesktopCapturer::Source window;
    window.id = id;
    window.title = "Test window";
    window_list_.push_back(window);
  }

#if defined(USE_AURA)
  std::unique_ptr<views::Widget> CreateDesktopWidget() {
    std::unique_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params;
    params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
    params.accept_events = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.native_widget = new views::DesktopNativeWidgetAura(widget.get());
    params.bounds = gfx::Rect(0, 0, 20, 20);
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

  void AddAuraWindow() {
    webrtc::DesktopCapturer::Source window;
    window.title = "Test window";

    // Create a aura native widow through a widget.
    desktop_widgets_.push_back(CreateDesktopWidget());
    aura::WindowTreeHost* const host =
        desktop_widgets_.back()->GetNativeWindow()->GetHost();
    aura::Window* const aura_window = host->window();

    // Get the native window's id.
    gfx::AcceleratedWidget widget = host->GetAcceleratedWidget();
#if defined(OS_WIN)
    window.id = reinterpret_cast<DesktopMediaID::Id>(widget);
#else
    window.id = widget;
#endif

    // Get the aura window's id.
    DesktopMediaID aura_id = DesktopMediaID::RegisterNativeWindow(
        DesktopMediaID::TYPE_WINDOW, aura_window);
    native_aura_id_map_[window.id] = aura_id.window_id;

    window_list_.push_back(window);
  }

  void RemoveAuraWindow(int index) {
    DCHECK_LT(index, static_cast<int>(desktop_widgets_.size()));

    // Get the native window's id.
    aura::Window* aura_window = desktop_widgets_[index]->GetNativeWindow();
    gfx::AcceleratedWidget widget =
        aura_window->GetHost()->GetAcceleratedWidget();
#if defined(OS_WIN)
    int native_id = reinterpret_cast<DesktopMediaID::Id>(widget);
#else
    int native_id = widget;
#endif
    // Remove the widget and associated aura window.
    desktop_widgets_.erase(desktop_widgets_.begin() + index);
    // Remove the aura window from the window list.
    size_t i;
    for (i = 0; i < window_list_.size(); i++) {
      if (window_list_[i].id == native_id)
        break;
    }
    DCHECK_LT(i, window_list_.size());
    window_list_.erase(window_list_.begin() + i);
    native_aura_id_map_.erase(native_id);
  }

#endif  // defined(USE_AURA)

  void AddWindowsAndVerify(bool has_view_dialog) {
    window_capturer_ = new FakeWindowCapturer();
    model_ = std::make_unique<NativeDesktopMediaList>(
        DesktopMediaID::TYPE_WINDOW, base::WrapUnique(window_capturer_));

    // Set update period to reduce the time it takes to run tests.
    model_->SetUpdatePeriod(base::TimeDelta::FromMilliseconds(20));

    // Set up widows.
    size_t aura_window_first_index = kDefaultWindowCount - kDefaultAuraCount;
    for (size_t i = 0; i < kDefaultWindowCount; ++i) {
      if (i < aura_window_first_index) {
        AddNativeWindow(i);
      } else {
#if defined(USE_AURA)
        AddAuraWindow();
#endif
      }
    }

    if (window_capturer_)
      window_capturer_->SetWindowList(window_list_);

    size_t window_count = kDefaultWindowCount;

    // Set view dialog window ID as the first window id.
    if (has_view_dialog) {
      DesktopMediaID dialog_window_id(DesktopMediaID::TYPE_WINDOW,
                                      window_list_[0].id);
      model_->SetViewDialogWindowId(dialog_window_id);
      window_count--;
      aura_window_first_index--;
    }

    base::RunLoop run_loop;

    {
      testing::InSequence dummy;
      for (size_t i = 0; i < window_count; ++i) {
        EXPECT_CALL(observer_, OnSourceAdded(model_.get(), i))
            .WillOnce(CheckListSize(model_.get(), static_cast<int>(i + 1)));
      }
      for (size_t i = 0; i < window_count - 1; ++i) {
        EXPECT_CALL(observer_, OnSourceThumbnailChanged(model_.get(), i));
      }
      EXPECT_CALL(observer_,
                  OnSourceThumbnailChanged(model_.get(), window_count - 1))
          .WillOnce(
              QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop));
    }
    model_->StartUpdating(&observer_);
    run_loop.Run();

    for (size_t i = 0; i < window_count; ++i) {
      EXPECT_EQ(model_->GetSource(i).id.type, DesktopMediaID::TYPE_WINDOW);
      EXPECT_EQ(model_->GetSource(i).name, base::UTF8ToUTF16("Test window"));
      int index = has_view_dialog ? i + 1 : i;
      int native_id = window_list_[index].id;
      EXPECT_EQ(model_->GetSource(i).id.id, native_id);
#if defined(USE_AURA)
      if (i >= aura_window_first_index)
        EXPECT_EQ(model_->GetSource(i).id.window_id,
                  native_aura_id_map_[native_id]);
#endif
    }
    testing::Mock::VerifyAndClearExpectations(&observer_);
  }

 protected:
  // Must be listed before |model_|, so it's destroyed last.
  MockObserver observer_;

  // Owned by |model_|;
  FakeWindowCapturer* window_capturer_;

  webrtc::DesktopCapturer::SourceList window_list_;
  std::vector<std::unique_ptr<views::Widget>> desktop_widgets_;
  std::map<DesktopMediaID::Id, DesktopMediaID::Id> native_aura_id_map_;
  std::unique_ptr<NativeDesktopMediaList> model_;

  DISALLOW_COPY_AND_ASSIGN(NativeDesktopMediaListTest);
};

TEST_F(NativeDesktopMediaListTest, Windows) {
  AddWindowsAndVerify(false);
}

TEST_F(NativeDesktopMediaListTest, ScreenOnly) {
  model_ = std::make_unique<NativeDesktopMediaList>(
      DesktopMediaID::TYPE_SCREEN, std::make_unique<FakeScreenCapturer>());

  // Set update period to reduce the time it takes to run tests.
  model_->SetUpdatePeriod(base::TimeDelta::FromMilliseconds(20));

  base::RunLoop run_loop;

  {
    testing::InSequence dummy;
    EXPECT_CALL(observer_, OnSourceAdded(model_.get(), 0))
        .WillOnce(CheckListSize(model_.get(), 1));
    EXPECT_CALL(observer_, OnSourceThumbnailChanged(model_.get(), 0))
        .WillOnce(QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop));
  }
  model_->StartUpdating(&observer_);
  run_loop.Run();

  EXPECT_EQ(model_->GetSource(0).id.type, DesktopMediaID::TYPE_SCREEN);
  EXPECT_EQ(model_->GetSource(0).id.id, 0);
}

// Verifies that the window specified with SetViewDialogWindowId() is filtered
// from the results.
TEST_F(NativeDesktopMediaListTest, WindowFiltering) {
  AddWindowsAndVerify(true);
}

TEST_F(NativeDesktopMediaListTest, AddNativeWindow) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  const int index = kDefaultWindowCount;
  EXPECT_CALL(observer_, OnSourceAdded(model_.get(), index))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), kDefaultWindowCount + 1),
                QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop)));

  AddNativeWindow(index);
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();

  EXPECT_EQ(model_->GetSource(index).id.type, DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(model_->GetSource(index).id.id, index);
}

#if defined(ENABLE_AURA_WINDOW_TESTS)
TEST_F(NativeDesktopMediaListTest, AddAuraWindow) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  const int index = kDefaultWindowCount;
  EXPECT_CALL(observer_, OnSourceAdded(model_.get(), index))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), kDefaultWindowCount + 1),
                QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop)));

  AddAuraWindow();
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();

  int native_id = window_list_.back().id;
  EXPECT_EQ(model_->GetSource(index).id.type, DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(model_->GetSource(index).id.id, native_id);
  EXPECT_EQ(model_->GetSource(index).id.window_id,
            native_aura_id_map_[native_id]);
}
#endif  // defined(ENABLE_AURA_WINDOW_TESTS)

TEST_F(NativeDesktopMediaListTest, RemoveNativeWindow) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), 0))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), kDefaultWindowCount - 1),
                QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop)));

  window_list_.erase(window_list_.begin());
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();
}

#if defined(ENABLE_AURA_WINDOW_TESTS)
TEST_F(NativeDesktopMediaListTest, RemoveAuraWindow) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  int aura_window_start_index = kDefaultWindowCount - kDefaultAuraCount;
  EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), aura_window_start_index))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), kDefaultWindowCount - 1),
                QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop)));

  RemoveAuraWindow(0);
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();
}
#endif  // defined(ENABLE_AURA_WINDOW_TESTS)

TEST_F(NativeDesktopMediaListTest, RemoveAllWindows) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  testing::InSequence seq;
  for (int i = 0; i < kDefaultWindowCount - 1; i++) {
    EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), 0))
        .WillOnce(CheckListSize(model_.get(), kDefaultWindowCount - i - 1));
  }
  EXPECT_CALL(observer_, OnSourceRemoved(model_.get(), 0))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), 0),
                QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop)));

  window_list_.clear();
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();
}

TEST_F(NativeDesktopMediaListTest, UpdateTitle) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  EXPECT_CALL(observer_, OnSourceNameChanged(model_.get(), 0))
      .WillOnce(QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop));

  const std::string kTestTitle = "New Title";
  window_list_[0].title = kTestTitle;
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();

  EXPECT_EQ(model_->GetSource(0).name, base::UTF8ToUTF16(kTestTitle));
}

TEST_F(NativeDesktopMediaListTest, UpdateThumbnail) {
  AddWindowsAndVerify(false);

  // Aura windows' thumbnails may unpredictably change over time.
  for (size_t i = kDefaultWindowCount - kDefaultAuraCount;
       i < kDefaultWindowCount; ++i) {
    EXPECT_CALL(observer_, OnSourceThumbnailChanged(model_.get(), i))
        .Times(testing::AnyNumber());
  }

  base::RunLoop run_loop;

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(model_.get(), 0))
      .WillOnce(QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop));

  // Update frame for the window and verify that we get notification about it.
  window_capturer_->SetNextFrameValue(0, 10);

  run_loop.Run();
}

TEST_F(NativeDesktopMediaListTest, MoveWindow) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  EXPECT_CALL(observer_, OnSourceMoved(model_.get(), 1, 0))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), kDefaultWindowCount),
                QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop)));

  std::swap(window_list_[0], window_list_[1]);
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();
}

// This test verifies that webrtc::DesktopCapturer::CaptureFrame() is not
// called when the thumbnail size is empty.
TEST_F(NativeDesktopMediaListTest, EmptyThumbnail) {
  window_capturer_ = new FakeWindowCapturer();
  model_ = std::make_unique<NativeDesktopMediaList>(
      DesktopMediaID::TYPE_WINDOW, base::WrapUnique(window_capturer_));
  model_->SetThumbnailSize(gfx::Size());

  // Set update period to reduce the time it takes to run tests.
  model_->SetUpdatePeriod(base::TimeDelta::FromMilliseconds(20));

  base::RunLoop run_loop;

  EXPECT_CALL(observer_, OnSourceAdded(model_.get(), 0))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), 1),
                QuitRunLoop(base::ThreadTaskRunnerHandle::Get(), &run_loop)));
  // Called upon webrtc::DesktopCapturer::CaptureFrame() call.
  ON_CALL(observer_, OnSourceThumbnailChanged(_, _))
      .WillByDefault(testing::InvokeWithoutArgs([]() { NOTREACHED(); }));

  model_->StartUpdating(&observer_);

  AddNativeWindow(0);
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();

  EXPECT_EQ(model_->GetSource(0).id.type, DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(model_->GetSource(0).id.id, 0);
  EXPECT_EQ(model_->GetSource(0).thumbnail.size(), gfx::Size());
}
