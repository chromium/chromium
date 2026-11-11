// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/camera_portal_focus_notifier_linux.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/system/system_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "media/capture/capture_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace {

// Drains tasks already posted to the current sequence (such as
// SystemMonitor's ObserverListThreadSafe notifications) via an explicit
// QuitClosure.
void FlushCurrentSequence() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

class FakeBrowserCollection : public BrowserCollection {
 public:
  FakeBrowserCollection() = default;
  ~FakeBrowserCollection() override = default;

  void AddExistingBrowser(BrowserWindowInterface* browser) {
    browsers_.push_back(browser);
  }

  void ClearBrowsers() { browsers_.clear(); }

  void Activate(BrowserWindowInterface* browser = nullptr) {
    for (BrowserCollectionObserver& observer : observers()) {
      observer.OnBrowserActivated(browser);
    }
  }

  bool HasObservers() { return !observers().empty(); }

  // BrowserCollection:
  bool IsEmpty() const override { return browsers_.empty(); }
  size_t GetSize() const override { return browsers_.size(); }

 protected:
  BrowserVector GetBrowsers(Order order) override { return browsers_; }

 private:
  BrowserVector browsers_;
};

class WidgetBackedBaseWindow : public ui::BaseWindow {
 public:
  explicit WidgetBackedBaseWindow(views::Widget* widget) : widget_(widget) {}

  void set_is_active_override(std::optional<bool> active) {
    is_active_override_ = active;
  }

  // ui::BaseWindow:
  bool IsActive() const override {
    return is_active_override_.value_or(widget_->IsActive());
  }
  bool IsMaximized() const override { return false; }
  bool IsMinimized() const override { return false; }
  bool IsFullscreen() const override { return false; }
  gfx::NativeWindow GetNativeWindow() const override {
    return widget_->GetNativeWindow();
  }
  gfx::Rect GetRestoredBounds() const override { return gfx::Rect(); }
  ui::mojom::WindowShowState GetRestoredState() const override {
    return ui::mojom::WindowShowState::kNormal;
  }
  gfx::Rect GetBounds() const override { return gfx::Rect(); }
  void Show() override {}
  void Hide() override {}
  bool IsVisible() const override { return true; }
  void ShowInactive() override {}
  void Close() override {}
  void Activate() override {}
  void Deactivate() override {}
  void Maximize() override {}
  void Minimize() override {}
  void Restore() override {}
  void SetBounds(const gfx::Rect& bounds) override {}
  void FlashFrame(bool flash) override {}
  ui::ZOrderLevel GetZOrderLevel() const override {
    return ui::ZOrderLevel::kNormal;
  }
  void SetZOrderLevel(ui::ZOrderLevel order) override {}

 private:
  raw_ptr<views::Widget> widget_;
  std::optional<bool> is_active_override_;
};

class DevicesChangedCounter
    : public base::SystemMonitor::DevicesChangedObserver {
 public:
  void OnDevicesChanged(base::SystemMonitor::DeviceType device_type) override {
    if (device_type == base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE) {
      ++count_;
    }
  }

  int count() const { return count_; }

 private:
  int count_ = 0;
};

class CameraPortalFocusNotifierLinuxTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    if (!base::SystemMonitor::Get()) {
      system_monitor_.emplace();
    }
    base::SystemMonitor::Get()->AddDevicesChangedObserver(&counter_);
  }

  void TearDown() override {
    collection_.ClearBrowsers();
    browsers_.clear();
    base::SystemMonitor::Get()->RemoveDevicesChangedObserver(&counter_);
    ChromeViewsTestBase::TearDown();
  }

  // A browser whose window is backed by a shown Widget.
  struct TestBrowser {
    explicit TestBrowser(std::unique_ptr<views::Widget> test_widget)
        : widget(std::move(test_widget)), window(widget.get()) {
      ON_CALL(browser, GetWindow()).WillByDefault(testing::Return(&window));
    }

    std::unique_ptr<views::Widget> widget;
    WidgetBackedBaseWindow window;
    testing::NiceMock<MockBrowserWindowInterface> browser;
  };

  // Returns a browser whose window is active (`active` true) or shown
  // inactive. The fixture owns it until TearDown().
  TestBrowser* CreateBrowser(bool active) {
    auto test_browser = std::make_unique<TestBrowser>(
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET));
    if (active) {
      test_browser->widget->Show();
      test_browser->window.set_is_active_override(true);
    } else {
      test_browser->widget->ShowInactive();
      test_browser->window.set_is_active_override(false);
    }
    return browsers_.emplace_back(std::move(test_browser)).get();
  }

  std::optional<base::SystemMonitor> system_monitor_;
  DevicesChangedCounter counter_;
  std::vector<std::unique_ptr<TestBrowser>> browsers_;
  FakeBrowserCollection collection_;
};

// The first activation of an active window reports one video capture device
// change, later activations don't.
TEST_F(CameraPortalFocusNotifierLinuxTest, NotifiesOnFirstActivationOnly) {
  CameraPortalFocusNotifierLinux notifier(&collection_);
  FlushCurrentSequence();
  EXPECT_EQ(counter_.count(), 0);

  // An activation without a browser window can't be verified and is ignored.
  collection_.Activate(nullptr);
  FlushCurrentSequence();
  EXPECT_EQ(counter_.count(), 0);
  EXPECT_TRUE(collection_.HasObservers());

  TestBrowser* active = CreateBrowser(/*active=*/true);
  collection_.Activate(&active->browser);
  ASSERT_TRUE(base::test::RunUntil([&] { return counter_.count() == 1; }));
  EXPECT_FALSE(collection_.HasObservers());

  collection_.Activate(&active->browser);
  FlushCurrentSequence();
  EXPECT_EQ(counter_.count(), 1);
}

// A browser window that is already active when the notifier is created
// notifies right away.
TEST_F(CameraPortalFocusNotifierLinuxTest,
       NotifiesForPreExistingActiveBrowser) {
  TestBrowser* active = CreateBrowser(/*active=*/true);
  collection_.AddExistingBrowser(&active->browser);

  CameraPortalFocusNotifierLinux notifier(&collection_);
  ASSERT_TRUE(base::test::RunUntil([&] { return counter_.count() == 1; }));
  EXPECT_FALSE(collection_.HasObservers());

  collection_.Activate(&active->browser);
  FlushCurrentSequence();
  EXPECT_EQ(counter_.count(), 1);
}

// Simulates Linux startup: the initial Browser is already in the collection and
// BrowserView::Show() fired OnBrowserActivated() while the native Widget is not
// yet active. Notification waits until the Widget itself becomes active.
TEST_F(CameraPortalFocusNotifierLinuxTest,
       WaitsForWidgetActivationOnPreExistingInactiveBrowser) {
  TestBrowser* inactive = CreateBrowser(/*active=*/false);
  ASSERT_FALSE(inactive->widget->IsActive());

  collection_.AddExistingBrowser(&inactive->browser);
  CameraPortalFocusNotifierLinux notifier(&collection_);
  FlushCurrentSequence();
  EXPECT_EQ(counter_.count(), 0);

  // Eager OnBrowserActivated() while BaseWindow::IsActive() is still false must
  // not notify yet.
  collection_.Activate(&inactive->browser);
  FlushCurrentSequence();
  EXPECT_EQ(counter_.count(), 0);
  EXPECT_TRUE(collection_.HasObservers());

  // Display-server activation of the Widget triggers the single notification.
  inactive->window.set_is_active_override(std::nullopt);
  inactive->widget->Activate();
  ASSERT_TRUE(base::test::RunUntil([&] { return counter_.count() == 1; }));
  EXPECT_FALSE(collection_.HasObservers());
}

// A browser created inactive after the notifier is waited for the same way.
TEST_F(CameraPortalFocusNotifierLinuxTest,
       WaitsForWidgetActivationOnNewInactiveBrowser) {
  CameraPortalFocusNotifierLinux notifier(&collection_);
  TestBrowser* inactive = CreateBrowser(/*active=*/false);

  notifier.OnBrowserCreated(&inactive->browser);
  collection_.Activate(&inactive->browser);
  FlushCurrentSequence();
  EXPECT_EQ(counter_.count(), 0);

  inactive->window.set_is_active_override(std::nullopt);
  inactive->widget->Activate();
  ASSERT_TRUE(base::test::RunUntil([&] { return counter_.count() == 1; }));
  EXPECT_FALSE(collection_.HasObservers());
}

#if defined(WEBRTC_USE_PIPEWIRE)
TEST_F(CameraPortalFocusNotifierLinuxTest, CreateIfEnabledRespectsFeatureFlag) {
  {
    base::test::ScopedFeatureList disabled;
    disabled.InitAndDisableFeature(features::kWebRtcPipeWireCamera);
    EXPECT_FALSE(CameraPortalFocusNotifierLinux::CreateIfEnabled(&collection_));
  }

  base::test::ScopedFeatureList enabled{features::kWebRtcPipeWireCamera};
  auto notifier = CameraPortalFocusNotifierLinux::CreateIfEnabled(&collection_);
  ASSERT_TRUE(notifier);
  TestBrowser* active = CreateBrowser(/*active=*/true);
  collection_.Activate(&active->browser);
  ASSERT_TRUE(base::test::RunUntil([&] { return counter_.count() == 1; }));
}
#endif

}  // namespace
