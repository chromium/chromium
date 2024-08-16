// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/native_desktop_media_list.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/desktop_capture.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/string_util_win.h"
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

#if BUILDFLAG(IS_WIN)
constexpr char kWindowTitle[] = "NativeDesktopMediaList Test Window";
constexpr wchar_t kWideWindowTitle[] = L"NativeDesktopMediaList Test Window";
constexpr wchar_t kWindowClass[] = L"NativeDesktopMediaListTestWindowClass";

struct WindowInfo {
  HWND hwnd;
  HINSTANCE window_instance;
  ATOM window_class;
};

WindowInfo CreateTestWindow() {
  WindowInfo info;
  ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCWSTR>(&DefWindowProc),
                      &info.window_instance);

  WNDCLASS window_class = {};
  window_class.hInstance = info.window_instance;
  window_class.lpfnWndProc = &DefWindowProc;
  window_class.lpszClassName = kWindowClass;
  info.window_class = ::RegisterClass(&window_class);

  info.hwnd =
      ::CreateWindow(kWindowClass, kWideWindowTitle, WS_OVERLAPPEDWINDOW,
                     CW_USEDEFAULT, CW_USEDEFAULT, /*width=*/100,
                     /*height=*/100, /*parent_window=*/nullptr,
                     /*menu_bar=*/nullptr, info.window_instance,
                     /*additional_params=*/nullptr);

  ::ShowWindow(info.hwnd, SW_SHOWNORMAL);
  ::UpdateWindow(info.hwnd);
  return info;
}

void DestroyTestWindow(WindowInfo info) {
  ::DestroyWindow(info.hwnd);
  ::UnregisterClass(MAKEINTATOM(info.window_class), info.window_instance);
}
#endif  // BUILDFLAG(IS_WIN)

// Returns the given index, offset by a fixed value such that it does not
// collide with Aura window IDs. Intended for usage with indices that are passed
// to AddNativeWindow().
int WindowIndex(int index) {
  // During test setup, an Aura window is created. On some platforms, e.g.
  // Wayland, this window's ID starts at 1 for the first run test, then 2 for
  // the second test, etc. To avoid clashes between this Aura window's ID and
  // the ID of native windows, this offset is added to every index passed to
  // AddNativeWindow(). Its value has no significance, it is just an arbitrary,
  // large number.
  static constexpr int kWindowIndexOffset = 1 << 12;
  return index + kWindowIndexOffset;
}

class MockObserver : public DesktopMediaListObserver {
 public:
  MOCK_METHOD1(OnSourceAdded, void(int index));
  MOCK_METHOD1(OnSourceRemoved, void(int index));
  MOCK_METHOD2(OnSourceMoved, void(int old_index, int new_index));
  MOCK_METHOD1(OnSourceNameChanged, void(int index));
  MOCK_METHOD1(OnSourceThumbnailChanged, void(int index));
  MOCK_METHOD1(OnSourcePreviewChanged, void(size_t index));
  MOCK_METHOD0(OnDelegatedSourceListSelection, void());
  MOCK_METHOD0(OnDelegatedSourceListDismissed, void());
};

class FakeScreenCapturer : public ThumbnailCapturer {
 public:
  FakeScreenCapturer() {}

  FakeScreenCapturer(const FakeScreenCapturer&) = delete;
  FakeScreenCapturer& operator=(const FakeScreenCapturer&) = delete;

  ~FakeScreenCapturer() override {}

  // ThumbnailCapturer implementation.
  void Start(Consumer* consumer) override { consumer_ = consumer; }

  FrameDeliveryMethod GetFrameDeliveryMethod() const override {
    return FrameDeliveryMethod::kOnRequest;
  }

  void CaptureFrame() override {
    DCHECK(consumer_);
    std::unique_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(10, 10)));
    consumer_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
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
  raw_ptr<Consumer> consumer_;
};

class FakeWindowCapturer : public ThumbnailCapturer {
 public:
  FakeWindowCapturer() = default;
  explicit FakeWindowCapturer(const webrtc::DesktopCaptureOptions& options)
      : options_(options) {}

  FakeWindowCapturer(const FakeWindowCapturer&) = delete;
  FakeWindowCapturer& operator=(const FakeWindowCapturer&) = delete;

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

  // ThumbnailCapturer implementation.
  void Start(Consumer* consumer) override { consumer_ = consumer; }

  FrameDeliveryMethod GetFrameDeliveryMethod() const override {
    return FrameDeliveryMethod::kOnRequest;
  }

  void CaptureFrame() override {
    DCHECK(consumer_);

    base::AutoLock lock(frame_values_lock_);

    auto it = frame_values_.find(selected_window_id_);
    int8_t value = (it != frame_values_.end()) ? it->second : 0;
    std::unique_ptr<webrtc::DesktopFrame> frame(
        new webrtc::BasicDesktopFrame(webrtc::DesktopSize(10, 10)));
    memset(frame->data(), value, frame->stride() * frame->size().height());
    consumer_->OnCaptureResult(webrtc::DesktopCapturer::Result::SUCCESS,
                               std::move(frame));
  }

  bool GetSourceList(SourceList* windows) override {
#if BUILDFLAG(IS_WIN)
    // WebRTC calls `GetWindowTextLength` and `GetWindowText` to get the title
    // of every window. If the window is owned by the current process, these
    // functions will send a `WM_GETTEXT` message to the window. This can cause
    // a deadlock if the message loop is waiting on `GetSourceList`. To avoid
    // this issue, WebRTC exposes the `enumerate_current_process_windows` which,
    // when set to false, prevents these APIs from being called on windows from
    // the current process.
    if (options_.enumerate_current_process_windows()) {
      for (const Source& source : window_list_) {
        HWND hwnd = reinterpret_cast<HWND>(source.id);
        ::GetWindowTextLength(hwnd);  // Side-effect: Sends WM_GETTEXT message.
      }
    }
#endif  // BUILDFLAG(IS_WIN)

    base::AutoLock lock(window_list_lock_);
    *windows = window_list_;
    return true;
  }

  bool SelectSource(SourceId id) override {
    selected_window_id_ = id;
    return true;
  }

 private:
  raw_ptr<Consumer> consumer_;
  webrtc::DesktopCaptureOptions options_ =
      webrtc::DesktopCaptureOptions::CreateDefault();
  SourceList window_list_;
  base::Lock window_list_lock_;

  SourceId selected_window_id_;

  // Frames to be captured per window.
  std::map<SourceId, int8_t> frame_values_;
  base::Lock frame_values_lock_;
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

  NativeDesktopMediaListTest(const NativeDesktopMediaListTest&) = delete;
  NativeDesktopMediaListTest& operator=(const NativeDesktopMediaListTest&) =
      delete;

  void TearDown() override {
#if BUILDFLAG(IS_WIN)
    if (window_open_)
      DestroyTestWindow(window_info_);
#endif  // BUILDFLAG(IS_WIN

    for (auto& desktop_widget : desktop_widgets_)
      desktop_widget.reset();

    ChromeViewsTestBase::TearDown();
  }

  void AddNativeWindow(int id) {
    webrtc::DesktopCapturer::Source window;
    window.id = id;
    window.title = "Test window";
    window_list_.push_back(window);
  }

#if defined(USE_AURA)
  views::UniqueWidgetPtr CreateDesktopWidget() {
    views::UniqueWidgetPtr widget(std::make_unique<views::Widget>());
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.accept_events = false;
    params.native_widget = new views::DesktopNativeWidgetAura(widget.get());
    params.bounds = gfx::Rect(0, 0, 20, 20);
    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

  void AddAuraWindow() {
    webrtc::DesktopCapturer::Source window;
    window.title = "Test window";

    // Create a aura native window through a widget.
    desktop_widgets_.push_back(CreateDesktopWidget());
    aura::WindowTreeHost* const host =
        desktop_widgets_.back()->GetNativeWindow()->GetHost();
    aura::Window* const aura_window = host->window();

    // Get the native window's id.
    gfx::AcceleratedWidget widget = host->GetAcceleratedWidget();
#if BUILDFLAG(IS_WIN)
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
#if BUILDFLAG(IS_WIN)
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

  void CreateCapturerAndModel() {
    webrtc::DesktopCaptureOptions options =
        content::desktop_capture::CreateDesktopCaptureOptions();

#if BUILDFLAG(IS_WIN)
    // This option should always be false on Windows so we avoid a potential
    // deadlock.
    EXPECT_FALSE(options.enumerate_current_process_windows());
#endif  // BUILDFLAG(IS_WIN)

    window_capturer_ = new FakeWindowCapturer(options);

    // Only set `add_current_process_windows` if we're using real test windows.
    // The tests that use fake windows will have their expectations fail if
    // `model_` picks up other windows on the system.
    bool add_current_process_windows = false;
#if BUILDFLAG(IS_WIN)
    add_current_process_windows = window_open_;
#endif  // BUILDFLAG(IS_WIN)
    model_ = std::make_unique<NativeDesktopMediaList>(
        DesktopMediaList::Type::kWindow,
        base::WrapUnique(window_capturer_.get()), add_current_process_windows,
        /*auto_show_delegated_source_list=*/true);
  }

  void UpdateModel() {
    base::RunLoop run_loop;
    base::OnceClosure update_consumer =
        base::BindLambdaForTesting([&]() { run_loop.Quit(); });
    model_->Update(std::move(update_consumer));
    run_loop.Run();
  }

  DesktopMediaList::Source GetSourceFromModel(content::DesktopMediaID::Id id) {
    int source_count = model_->GetSourceCount();
    DesktopMediaList::Source source;
    for (int i = 0; i < source_count; i++) {
      source = model_->GetSource(i);
      if (source.id.id == id) {
        return source;
      }
    }

    return DesktopMediaList::Source();
  }

  void AddWindowsAndVerify(bool has_view_dialog) {
    CreateCapturerAndModel();

    // Set update period to reduce the time it takes to run tests.
    model_->SetUpdatePeriod(base::Milliseconds(20));

    // Set up windows.
    size_t aura_window_first_index = kDefaultWindowCount - kDefaultAuraCount;
    for (size_t i = 0; i < kDefaultWindowCount; ++i) {
      if (i < aura_window_first_index) {
        AddNativeWindow(WindowIndex(i));
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
        EXPECT_CALL(observer_, OnSourceAdded(i))
            .WillOnce(CheckListSize(model_.get(), static_cast<int>(i + 1)));
      }
      for (size_t i = 0; i < window_count - 1; ++i) {
        EXPECT_CALL(observer_, OnSourceThumbnailChanged(i));
      }
      EXPECT_CALL(observer_, OnSourceThumbnailChanged(window_count - 1))
          .WillOnce(QuitRunLoop(
              base::SingleThreadTaskRunner::GetCurrentDefault(), &run_loop));
    }
    model_->StartUpdating(&observer_);
    run_loop.Run();

    for (size_t i = 0; i < window_count; ++i) {
      EXPECT_EQ(model_->GetSource(i).id.type, DesktopMediaID::TYPE_WINDOW);
      EXPECT_EQ(model_->GetSource(i).name, u"Test window");
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

#if BUILDFLAG(IS_WIN)
  void CreateRealWindow() {
    window_open_ = true;
    window_info_ = CreateTestWindow();
  }
#endif  // BUILDFLAG(IS_WIN)

 protected:
  // Must be listed before |model_|, so it's destroyed last.
  MockObserver observer_;

  // Owned by |model_|;
  raw_ptr<FakeWindowCapturer, DanglingUntriaged> window_capturer_;

  webrtc::DesktopCapturer::SourceList window_list_;
  std::vector<views::UniqueWidgetPtr> desktop_widgets_;
  std::map<DesktopMediaID::Id, DesktopMediaID::Id> native_aura_id_map_;
  std::unique_ptr<NativeDesktopMediaList> model_;

#if BUILDFLAG(IS_WIN)
  bool window_open_ = false;
  WindowInfo window_info_;
#endif  // BUILDFLAG(IS_WIN)
};

TEST_F(NativeDesktopMediaListTest, Windows) {
  AddWindowsAndVerify(false);
}

TEST_F(NativeDesktopMediaListTest, ScreenOnly) {
  model_ = std::make_unique<NativeDesktopMediaList>(
      DesktopMediaList::Type::kScreen, std::make_unique<FakeScreenCapturer>());

  // Set update period to reduce the time it takes to run tests.
  model_->SetUpdatePeriod(base::Milliseconds(20));

  base::RunLoop run_loop;

  {
    testing::InSequence dummy;
    EXPECT_CALL(observer_, OnSourceAdded(0))
        .WillOnce(CheckListSize(model_.get(), 1));
    EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
        .WillOnce(QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                              &run_loop));
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
  EXPECT_CALL(observer_, OnSourceAdded(index))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), kDefaultWindowCount + 1),
                QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            &run_loop)));

  AddNativeWindow(WindowIndex(index));
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();

  EXPECT_EQ(model_->GetSource(index).id.type, DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(model_->GetSource(index).id.id, WindowIndex(index));
}

#if defined(ENABLE_AURA_WINDOW_TESTS)
TEST_F(NativeDesktopMediaListTest, AddAuraWindow) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  const int index = kDefaultWindowCount;
  EXPECT_CALL(observer_, OnSourceAdded(index))
      .WillOnce(
          DoAll(CheckListSize(kDefaultWindowCount + 1),
                QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            &run_loop)));

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

  EXPECT_CALL(observer_, OnSourceRemoved(0))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), kDefaultWindowCount - 1),
                QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            &run_loop)));

  window_list_.erase(window_list_.begin());
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();
}

#if defined(ENABLE_AURA_WINDOW_TESTS)
TEST_F(NativeDesktopMediaListTest, RemoveAuraWindow) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  int aura_window_start_index = kDefaultWindowCount - kDefaultAuraCount;
  EXPECT_CALL(observer_, OnSourceRemoved(aura_window_start_index))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), kDefaultWindowCount - 1),
                QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            &run_loop)));

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
    EXPECT_CALL(observer_, OnSourceRemoved(0))
        .WillOnce(CheckListSize(model_.get(), kDefaultWindowCount - i - 1));
  }
  EXPECT_CALL(observer_, OnSourceRemoved(0))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), 0),
                QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            &run_loop)));

  window_list_.clear();
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();
}

TEST_F(NativeDesktopMediaListTest, UpdateTitle) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  EXPECT_CALL(observer_, OnSourceNameChanged(0))
      .WillOnce(QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            &run_loop));

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
    EXPECT_CALL(observer_, OnSourceThumbnailChanged(i))
        .Times(testing::AnyNumber());
  }

  base::RunLoop run_loop;

  EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
      .WillOnce(QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            &run_loop));

  // Update frame for the window and verify that we get notification about it.
  window_capturer_->SetNextFrameValue(WindowIndex(0), 10);

  run_loop.Run();
}

TEST_F(NativeDesktopMediaListTest, MoveWindow) {
  AddWindowsAndVerify(false);

  base::RunLoop run_loop;

  EXPECT_CALL(observer_, OnSourceMoved(1, 0))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), kDefaultWindowCount),
                QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            &run_loop)));

  std::swap(window_list_[0], window_list_[1]);
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();
}

// This test verifies that webrtc::DesktopCapturer::CaptureFrame() is not
// called when the thumbnail size is empty.
TEST_F(NativeDesktopMediaListTest, EmptyThumbnail) {
  CreateCapturerAndModel();
  model_->SetThumbnailSize(gfx::Size());

  // Set update period to reduce the time it takes to run tests.
  model_->SetUpdatePeriod(base::Milliseconds(20));

  base::RunLoop run_loop;

  EXPECT_CALL(observer_, OnSourceAdded(0))
      .WillOnce(
          DoAll(CheckListSize(model_.get(), 1),
                QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                            &run_loop)));
  // Called upon webrtc::DesktopCapturer::CaptureFrame() call.
  ON_CALL(observer_, OnSourceThumbnailChanged(_))
      .WillByDefault(
          testing::InvokeWithoutArgs([]() { NOTREACHED_IN_MIGRATION(); }));

  model_->StartUpdating(&observer_);

  AddNativeWindow(WindowIndex(0));
  window_capturer_->SetWindowList(window_list_);

  run_loop.Run();

  EXPECT_EQ(model_->GetSource(0).id.type, DesktopMediaID::TYPE_WINDOW);
  EXPECT_EQ(model_->GetSource(0).id.id, WindowIndex(0));
  EXPECT_EQ(model_->GetSource(0).thumbnail.size(), gfx::Size());
}

#if BUILDFLAG(IS_WIN)
TEST_F(NativeDesktopMediaListTest, GetSourceListAvoidsDeadlock) {
  // We need a real window so we can send a message and reproduce the deadlock
  // scenario. This window must be created on a different thread than from where
  // `GetSourceList` will be called. Otherwise, it can directly invoke the
  // window procedure and avoid the deadlock.
  base::Thread window_thread("GetSourceListDeadlockTestWindowThread");
  window_thread.Start();
  base::RunLoop run_loop;
  WindowInfo info;
  window_thread.task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CreateTestWindow),
      base::BindLambdaForTesting([&](WindowInfo window_info) {
        info = window_info;
        run_loop.Quit();
      }));
  // After this point, the window will be unresponsive because we've quit its
  // message loop. This means any messages sent to the window will cause a
  // deadlock.
  run_loop.Run();
  EXPECT_NE(info.hwnd, static_cast<HWND>(0));

  // These `options` should have the `enumerate_current_process_windows`
  // option set to false, so that `GetSourceList` won't send a `WM_GETTEXT`
  // message to our window.
  webrtc::DesktopCaptureOptions options =
      content::desktop_capture::CreateDesktopCaptureOptions();
  EXPECT_FALSE(options.enumerate_current_process_windows());
  auto window_capturer = std::make_unique<FakeWindowCapturer>(options);
  window_capturer->SetWindowList(
      {{reinterpret_cast<intptr_t>(info.hwnd), kWindowTitle}});

  // This should not hang, because we told it to ignore windows owned by the
  // current process.
  webrtc::DesktopCapturer::SourceList source_list;
  EXPECT_TRUE(window_capturer->GetSourceList(&source_list));

  window_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DestroyTestWindow, info));
  window_thread.Stop();
}

TEST_F(NativeDesktopMediaListTest, CollectsCurrentProcessWindows) {
  // We need a real window so we can ensure windows owned by the current
  // process are picked up by `model_` even if they aren't enumerated by the
  // capturer.
  CreateRealWindow();
  CreateCapturerAndModel();
  UpdateModel();

  // Ensure that `model_` is finding and adding the window to it's sources, and
  // not getting it from the capturer.
  webrtc::DesktopCapturer::SourceList source_list;
  EXPECT_TRUE(window_capturer_->GetSourceList(&source_list));
  EXPECT_EQ(source_list.size(), 0ull);

  content::DesktopMediaID::Id window_id =
      reinterpret_cast<intptr_t>(window_info_.hwnd);
  DesktopMediaList::Source source = GetSourceFromModel(window_id);
  EXPECT_EQ(source.id.id, window_id);
  EXPECT_STREQ(base::as_wcstr(source.name.c_str()), kWideWindowTitle);
}

TEST_F(NativeDesktopMediaListTest, MinimizedCurrentProcessWindows) {
  CreateRealWindow();
  CreateCapturerAndModel();

  webrtc::DesktopCapturer::SourceList source_list;
  EXPECT_TRUE(window_capturer_->GetSourceList(&source_list));
  EXPECT_EQ(source_list.size(), 0ull);

  // If we minimize the window it should not appear in `model_`s sources.
  ::ShowWindow(window_info_.hwnd, SW_MINIMIZE);
  UpdateModel();
  DesktopMediaList::Source source =
      GetSourceFromModel(reinterpret_cast<intptr_t>(window_info_.hwnd));

  // We expect the source is not found.
  EXPECT_EQ(source.id.id, content::DesktopMediaID::kNullId);
}
#endif  // BUILDFLAG(IS_WIN)

class DelegatedFakeScreenCapturer
    : public FakeScreenCapturer,
      public webrtc::DelegatedSourceListController {
 public:
  webrtc::DelegatedSourceListController* GetDelegatedSourceListController()
      override {
    return this;
  }

  void Observe(
      webrtc::DelegatedSourceListController::Observer* observer) override {
    DCHECK(!observer_ || !observer);
    observer_ = observer;
  }

  void SimulateSourceListSelection() {
    DCHECK(observer_);
    observer_->OnSelection();
  }

  void SimulateSourceListCancelled() {
    DCHECK(observer_);
    observer_->OnCancelled();
  }

  void SimulateSourceListError() {
    DCHECK(observer_);
    observer_->OnError();
  }

  void EnsureVisible() override { ensure_visible_call_count_++; }

  void EnsureHidden() override { ensure_hidden_call_count_++; }

  int ensure_visible_call_count() const { return ensure_visible_call_count_; }

  int ensure_hidden_call_count() const { return ensure_hidden_call_count_; }

 private:
  raw_ptr<webrtc::DelegatedSourceListController::Observer> observer_ = nullptr;
  int ensure_visible_call_count_ = 0;
  int ensure_hidden_call_count_ = 0;
};

class NativeDesktopMediaListDelegatedTest : public ChromeViewsTestBase {
 public:
  NativeDesktopMediaListDelegatedTest() {
    auto capturer = std::make_unique<DelegatedFakeScreenCapturer>();
    capturer_ = capturer.get();
    model_ = std::make_unique<NativeDesktopMediaList>(
        DesktopMediaList::Type::kScreen, std::move(capturer));

    model_->SetUpdatePeriod(base::Milliseconds(20));
  }

  ~NativeDesktopMediaListDelegatedTest() override = default;

  void SetUp() override {
    // We're not testing this behavior, but if we don't add an expectation, then
    // the tests will complain about unexpected calls occurring.
    EXPECT_CALL(observer_, OnSourceAdded(0)).Times(testing::AnyNumber());
    EXPECT_CALL(observer_, OnSourceThumbnailChanged(0))
        .Times(testing::AnyNumber());

    // Start updating and then ensure that the capturer is ready before allowing
    // the test to proceed.
    model_->StartUpdating(&observer_);
    WaitForCapturerTasks();
    ChromeViewsTestBase::SetUp();
  }

  void WaitForCapturerTasks() {
    base::RunLoop run_loop;
    model_->GetCapturerTaskRunnerForTesting()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
    run_loop.Run();
  }

  void TriggerAndWaitForSelection() {
    base::RunLoop run_loop;
    EXPECT_CALL(observer_, OnDelegatedSourceListSelection())
        .WillOnce(QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                              &run_loop));
    capturer_->SimulateSourceListSelection();
    run_loop.Run();
  }

  void TriggerAndWaitForError() {
    base::RunLoop run_loop;
    // We don't differentiate to the observer *why* the list is dismissed, just
    // that it was.
    EXPECT_CALL(observer_, OnDelegatedSourceListDismissed())
        .WillOnce(QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                              &run_loop));
    capturer_->SimulateSourceListError();
    run_loop.Run();
  }

  void TriggerAndWaitForCancelled() {
    base::RunLoop run_loop;
    // We don't differentiate to the observer *why* the list is dismissed, just
    // that it was.
    EXPECT_CALL(observer_, OnDelegatedSourceListDismissed())
        .WillOnce(QuitRunLoop(base::SingleThreadTaskRunner::GetCurrentDefault(),
                              &run_loop));
    capturer_->SimulateSourceListCancelled();
    run_loop.Run();
  }

 protected:
  // The order is important here. Obsever must outlive model, and the capturer
  // is owned by the model, so we should dispose of it's raw pointer before
  // we dispose of the model which would invalidate it. Hence observer is listed
  // first, followed by model, followed by the capturer.
  MockObserver observer_;
  std::unique_ptr<NativeDesktopMediaList> model_;
  raw_ptr<DelegatedFakeScreenCapturer> capturer_;
};

TEST_F(NativeDesktopMediaListDelegatedTest, Selection) {
  // This triggers a selection and waits for it to come through the Observer.
  // This will time out if it fails.
  TriggerAndWaitForSelection();
}

TEST_F(NativeDesktopMediaListDelegatedTest, Cancelled) {
  // This triggers a cancellation and waits for it to come through the Observer.
  // This will time out if it fails.
  TriggerAndWaitForCancelled();
}

TEST_F(NativeDesktopMediaListDelegatedTest, Error) {
  // This triggers an error and waits for it to come through the Observer.
  // This will time out if it fails.
  TriggerAndWaitForError();
}

// Verify that all calls to FocusList are passed on unless there is a selection.
TEST_F(NativeDesktopMediaListDelegatedTest, ShowRepeatedlyNotForced) {
  const int expected_call_times = 5;

  for (int i = 0; i < expected_call_times; i++) {
    model_->FocusList();
  }

  WaitForCapturerTasks();
  EXPECT_EQ(expected_call_times, capturer_->ensure_visible_call_count());

  TriggerAndWaitForSelection();

  for (int i = 0; i < expected_call_times; i++) {
    model_->FocusList();
  }

  // No new calls should've come in via FocusList after being notified of a
  // selection.
  WaitForCapturerTasks();
  EXPECT_EQ(expected_call_times, capturer_->ensure_visible_call_count());
}

// Verify that all calls to HideList are passed on.
TEST_F(NativeDesktopMediaListDelegatedTest, HideRepeatedly) {
  const int expected_call_times = 5;

  for (int i = 0; i < expected_call_times; i++) {
    model_->HideList();
  }

  WaitForCapturerTasks();
  EXPECT_EQ(expected_call_times, capturer_->ensure_hidden_call_count());

  TriggerAndWaitForSelection();

  for (int i = 0; i < expected_call_times; i++) {
    model_->HideList();
  }

  // HideList may still be called after being notified of a selection.
  WaitForCapturerTasks();
  EXPECT_EQ(2 * expected_call_times, capturer_->ensure_hidden_call_count());
}

TEST_F(NativeDesktopMediaListDelegatedTest, FocusAfterSelect) {
  TriggerAndWaitForSelection();

  // Calling FocusList after a selection should not trigger an EnsureVisible
  // call.
  model_->FocusList();
  WaitForCapturerTasks();
  EXPECT_EQ(0, capturer_->ensure_visible_call_count());

  // Clearing the selection while visible (a result of calling FocusList above),
  // should trigger an EnsureVisible call.
  model_->ClearDelegatedSourceListSelection();
  WaitForCapturerTasks();
  EXPECT_EQ(1, capturer_->ensure_visible_call_count());

  // Calling FocusList after clearing the selection should again trigger an
  // EnsureVisible call.
  model_->FocusList();
  WaitForCapturerTasks();
  EXPECT_EQ(2, capturer_->ensure_visible_call_count());
}

// Verify that ClearSelection is a no-op if there is not a selection or if it is
// not focused.
TEST_F(NativeDesktopMediaListDelegatedTest, ClearSelectionNoOp) {
  model_->ClearDelegatedSourceListSelection();

  // Because we never triggered a selection, we should not have a call to
  // EnsureVisible.
  WaitForCapturerTasks();
  EXPECT_EQ(0, capturer_->ensure_visible_call_count());

  // Select and then clear the call.
  TriggerAndWaitForSelection();
  model_->ClearDelegatedSourceListSelection();

  // Because we have never called |FocusList|, ensure visible shouldn't have
  // been called.
  WaitForCapturerTasks();
  EXPECT_EQ(0, capturer_->ensure_visible_call_count());

  // Because our selection has been cleared, we should become visible the next
  // time that we are focused.
  model_->FocusList();
  WaitForCapturerTasks();
  EXPECT_EQ(1, capturer_->ensure_visible_call_count());
}
