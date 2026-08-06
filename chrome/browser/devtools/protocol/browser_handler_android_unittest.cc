// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/browser_handler_android.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/native_unit_test_support_jni/BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_jni.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Minimal ui::BaseWindow stub for bounds and state reads and mutations.
class FakeBaseWindow : public ui::BaseWindow {
 public:
  FakeBaseWindow(gfx::Rect bounds,
                 gfx::Rect restored_bounds,
                 bool fullscreen,
                 bool maximized,
                 bool minimized,
                 bool can_resize = true)
      : bounds_(bounds),
        restored_bounds_(restored_bounds),
        fullscreen_(fullscreen),
        maximized_(maximized),
        minimized_(minimized),
        resize_precheck_result_(
            can_resize
                ? ui::WindowResizePrecheckResult::kOk
                : ui::WindowResizePrecheckResult::kAndroidNotAFreeformWindow) {}

  bool IsActive() const override { return false; }
  bool IsMaximized() const override { return maximized_; }
  bool IsMinimized() const override { return minimized_; }
  bool IsFullscreen() const override { return fullscreen_; }
  gfx::NativeWindow GetNativeWindow() const override { return {}; }
  gfx::Rect GetRestoredBounds() const override { return restored_bounds_; }
  ui::mojom::WindowShowState GetRestoredState() const override {
    return ui::mojom::WindowShowState::kNormal;
  }
  gfx::Rect GetBounds() const override { return bounds_; }
  void Show() override {}
  void Hide() override {}
  bool IsVisible() const override { return true; }
  void ShowInactive() override {}
  void Close() override {}
  void Activate() override {}
  void Deactivate() override {}
  bool CanResize(ui::WindowResizePrecheckResult& result) const override {
    result = resize_precheck_result_;
    return result == ui::WindowResizePrecheckResult::kOk;
  }
  void Maximize() override {
    maximized_ = true;
    minimized_ = false;
  }
  void Minimize() override {
    maximized_ = false;
    minimized_ = true;
  }
  void Restore() override {
    maximized_ = false;
    minimized_ = false;
  }
  void SetBounds(const gfx::Rect& bounds) override {
    bounds_ = bounds;
    maximized_ = false;
    minimized_ = false;
  }
  void FlashFrame(bool flash) override {}
  ui::ZOrderLevel GetZOrderLevel() const override {
    return ui::ZOrderLevel::kNormal;
  }
  void SetZOrderLevel(ui::ZOrderLevel order) override {}

  void SetResizePrecheckResult(ui::WindowResizePrecheckResult result) {
    resize_precheck_result_ = result;
  }

 private:
  gfx::Rect bounds_;
  gfx::Rect restored_bounds_;
  bool fullscreen_;
  bool maximized_;
  bool minimized_;
  ui::WindowResizePrecheckResult resize_precheck_result_;
};

class FakeBrowserWindowInterface : public BrowserWindowInterface {
 public:
  FakeBrowserWindowInterface(Profile* profile,
                             SessionID session_id,
                             ui::BaseWindow* window)
      : profile_(profile), session_id_(session_id), window_(window) {}

  ~FakeBrowserWindowInterface() override {
    browser_did_close_callback_list_.Notify(this);
  }

  Profile* GetProfile() override { return profile_; }
  const Profile* GetProfile() const override { return profile_; }
  const SessionID& GetSessionID() const override { return session_id_; }
  bool IsDeleteScheduled() const override { return false; }
  base::CallbackListSubscription RegisterBrowserDidClose(
      BrowserDidCloseCallback callback) override {
    return browser_did_close_callback_list_.Add(std::move(callback));
  }
  ui::UnownedUserDataHost& GetUnownedUserDataHost() override {
    return user_data_host_;
  }
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override {
    return user_data_host_;
  }
  Type GetType() const override { return Type::TYPE_NORMAL; }
  ui::BaseWindow* GetWindow() override { return window_; }
  const ui::BaseWindow* GetWindow() const override { return window_; }
  content::WebContents* OpenURL(
      const content::OpenURLParams&,
      base::OnceCallback<void(content::NavigationHandle&)>) override {
    return nullptr;
  }
  base::WeakPtr<BrowserWindowInterface> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<Profile> profile_;
  const SessionID session_id_;
  raw_ptr<ui::BaseWindow> window_;
  ui::UnownedUserDataHost user_data_host_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)>
      browser_did_close_callback_list_;
  base::WeakPtrFactory<FakeBrowserWindowInterface> weak_ptr_factory_{this};
};

class TestFrontendChannel : public protocol::FrontendChannel {
 public:
  void SendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override {}

  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override {}

  void FlushProtocolNotifications() override {}
};

}  // namespace

class BrowserHandlerAndroidTest : public ChromeRenderViewHostTestHarness {};

TEST_F(BrowserHandlerAndroidTest, PrefersTabWindowIdOverTabModelSessionId) {
  OwningTestTabModel tab_model(profile());
  TabAndroid* tab =
      tab_model.AddTabFromWebContents(CreateTestWebContents(), /*index=*/0);
  sessions::SessionTabHelper::CreateForWebContents(tab->web_contents(),
                                                   base::NullCallback());

  SessionID tab_window_id = SessionID::NewUnique();
  tab->SetWindowSessionID(tab_window_id);

  ASSERT_NE(tab_model.GetSessionId(), tab_window_id);

  std::optional<int> resolved =
      BrowserHandlerAndroid::ResolveWindowIdForWebContents(tab->web_contents());
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(tab_window_id.id(), resolved.value());
}

TEST_F(BrowserHandlerAndroidTest,
       FallsBackToTabModelSessionIdWhenTabHasNoWindowId) {
  OwningTestTabModel tab_model(profile());
  TabAndroid* tab =
      tab_model.AddTabFromWebContents(CreateTestWebContents(), /*index=*/0);

  // Tab carries no explicit window id; resolver should fall back to the
  // owning TabModel's session id.
  ASSERT_FALSE(tab->GetWindowId().is_valid());

  std::optional<int> resolved =
      BrowserHandlerAndroid::ResolveWindowIdForWebContents(tab->web_contents());
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(tab_model.GetSessionId().id(), resolved.value());
}

TEST_F(BrowserHandlerAndroidTest, ReturnsNulloptWhenWebContentsUnknown) {
  std::unique_ptr<content::WebContents> standalone = CreateTestWebContents();

  std::optional<int> resolved =
      BrowserHandlerAndroid::ResolveWindowIdForWebContents(standalone.get());
  EXPECT_FALSE(resolved.has_value());
}

TEST(BrowserHandlerAndroidStateTest, FullscreenWinsOverEverything) {
  EXPECT_EQ("fullscreen", BrowserHandlerAndroid::ComputeWindowStateString(
                              /*is_fullscreen=*/true,
                              /*is_maximized=*/false,
                              /*is_minimized=*/false));
  EXPECT_EQ("fullscreen", BrowserHandlerAndroid::ComputeWindowStateString(
                              /*is_fullscreen=*/true,
                              /*is_maximized=*/true,
                              /*is_minimized=*/false));
  EXPECT_EQ("fullscreen", BrowserHandlerAndroid::ComputeWindowStateString(
                              /*is_fullscreen=*/true,
                              /*is_maximized=*/false,
                              /*is_minimized=*/true));
  EXPECT_EQ("fullscreen", BrowserHandlerAndroid::ComputeWindowStateString(
                              /*is_fullscreen=*/true,
                              /*is_maximized=*/true,
                              /*is_minimized=*/true));
}

TEST(BrowserHandlerAndroidStateTest, MaximizedBeatsMinimizedWhenNotFullscreen) {
  EXPECT_EQ("maximized", BrowserHandlerAndroid::ComputeWindowStateString(
                             /*is_fullscreen=*/false,
                             /*is_maximized=*/true,
                             /*is_minimized=*/true));
}

TEST(BrowserHandlerAndroidStateTest, MinimizedWhenOnlyMinimized) {
  EXPECT_EQ("minimized", BrowserHandlerAndroid::ComputeWindowStateString(
                             /*is_fullscreen=*/false,
                             /*is_maximized=*/false,
                             /*is_minimized=*/true));
}

TEST(BrowserHandlerAndroidStateTest, NormalWhenAllFalse) {
  EXPECT_EQ("normal", BrowserHandlerAndroid::ComputeWindowStateString(
                          /*is_fullscreen=*/false,
                          /*is_maximized=*/false,
                          /*is_minimized=*/false));
}

TEST_F(BrowserHandlerAndroidTest,
       FindBrowserWindowByIdReturnsWindowForRegisteredBwi) {
  // Positive case: when a live AndroidBrowserWindow owns a session id,
  // FindBrowserWindowById returns its ui::BaseWindow.
  BrowserWindowInterface* live_window = reinterpret_cast<
      BrowserWindowInterface*>(
      Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_createBrowserWindow(
          base::android::AttachCurrentThread(), /*taskId=*/1,
          profile()->GetJavaObject()));
  ASSERT_NE(nullptr, live_window);
  SessionID live_id = live_window->GetSessionID();

  ui::BaseWindow* resolved =
      BrowserHandlerAndroid::FindBrowserWindowById(live_id.id());
  EXPECT_EQ(live_window->GetWindow(), resolved);

  Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_destroyBrowserWindow(
      base::android::AttachCurrentThread(), /*taskId=*/1);
}

TEST_F(BrowserHandlerAndroidTest,
       FindBrowserWindowByIdReturnsNullForUnregisteredWindowId) {
  // An id without a matching BWI returns nullptr even when other BWIs are
  // registered.
  BrowserWindowInterface* live_window = reinterpret_cast<
      BrowserWindowInterface*>(
      Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_createBrowserWindow(
          base::android::AttachCurrentThread(), /*taskId=*/1,
          profile()->GetJavaObject()));
  ASSERT_NE(nullptr, live_window);
  SessionID unregistered_window_id = SessionID::NewUnique();
  ASSERT_NE(live_window->GetSessionID(), unregistered_window_id);

  EXPECT_EQ(nullptr, BrowserHandlerAndroid::FindBrowserWindowById(
                         unregistered_window_id.id()));

  Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_destroyBrowserWindow(
      base::android::AttachCurrentThread(), /*taskId=*/1);
}

TEST_F(BrowserHandlerAndroidTest,
       GetWindowBoundsReturnsErrorForUnknownWindowId) {
  TestFrontendChannel channel;
  protocol::UberDispatcher dispatcher(&channel);
  BrowserHandlerAndroid handler(&dispatcher, /*target_id=*/"");

  std::unique_ptr<protocol::Browser::Bounds> bounds;
  protocol::Response response =
      handler.GetWindowBounds(SessionID::NewUnique().id(), &bounds);

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ("Browser window not found", response.Message());
  EXPECT_EQ(nullptr, bounds);
}

TEST_F(BrowserHandlerAndroidTest,
       GetWindowBoundsUsesWindowTrackedByDevToolsSession) {
  TestFrontendChannel channel;
  protocol::UberDispatcher dispatcher(&channel);
  BrowserHandlerAndroid handler(&dispatcher, /*target_id=*/"");
  FakeBaseWindow base_window(/*bounds=*/gfx::Rect(10, 20, 800, 600),
                             /*restored_bounds=*/gfx::Rect(),
                             /*fullscreen=*/false,
                             /*maximized=*/true,
                             /*minimized=*/false);
  const SessionID session_id = SessionID::NewUnique();
  FakeBrowserWindowInterface browser_window(profile(), session_id,
                                            &base_window);

  ASSERT_EQ(nullptr,
            BrowserHandlerAndroid::FindBrowserWindowById(session_id.id()));
  handler.TrackBrowserWindow(&browser_window);

  std::unique_ptr<protocol::Browser::Bounds> bounds;
  protocol::Response response =
      handler.GetWindowBounds(session_id.id(), &bounds);

  EXPECT_TRUE(response.IsSuccess());
  ASSERT_NE(nullptr, bounds);
  EXPECT_EQ(10, bounds->GetLeft());
  EXPECT_EQ(20, bounds->GetTop());
  EXPECT_EQ(800, bounds->GetWidth());
  EXPECT_EQ(600, bounds->GetHeight());
  EXPECT_EQ("maximized", bounds->GetWindowState());
}

TEST_F(BrowserHandlerAndroidTest,
       GetWindowBoundsRejectsDestroyedTrackedWindow) {
  TestFrontendChannel channel;
  protocol::UberDispatcher dispatcher(&channel);
  BrowserHandlerAndroid handler(&dispatcher, /*target_id=*/"");
  FakeBaseWindow base_window(/*bounds=*/gfx::Rect(10, 20, 800, 600),
                             /*restored_bounds=*/gfx::Rect(),
                             /*fullscreen=*/false,
                             /*maximized=*/true,
                             /*minimized=*/false);
  const SessionID session_id = SessionID::NewUnique();
  auto browser_window = std::make_unique<FakeBrowserWindowInterface>(
      profile(), session_id, &base_window);
  handler.TrackBrowserWindow(browser_window.get());
  browser_window.reset();

  std::unique_ptr<protocol::Browser::Bounds> bounds;
  protocol::Response response =
      handler.GetWindowBounds(session_id.id(), &bounds);

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ("Browser window not found", response.Message());
  EXPECT_EQ(nullptr, bounds);
}

TEST_F(BrowserHandlerAndroidTest,
       SetWindowBoundsPrefersRegisteredWindowOverTrackedFallback) {
  BrowserWindowInterface* live_window = reinterpret_cast<
      BrowserWindowInterface*>(
      Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_createBrowserWindow(
          base::android::AttachCurrentThread(), /*taskId=*/1,
          profile()->GetJavaObject()));
  ASSERT_NE(nullptr, live_window);

  TestFrontendChannel channel;
  protocol::UberDispatcher dispatcher(&channel);
  BrowserHandlerAndroid handler(&dispatcher, /*target_id=*/"");
  FakeBaseWindow fallback_window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false,
                                 false, false);
  fallback_window.SetResizePrecheckResult(
      ui::WindowResizePrecheckResult::kAndroidNoActivity);
  FakeBrowserWindowInterface tracked_window(
      profile(), live_window->GetSessionID(), &fallback_window);
  handler.TrackBrowserWindow(&tracked_window);

  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("minimized").Build();
  protocol::Response response = handler.SetWindowBounds(
      live_window->GetSessionID().id(), std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ(
      "Window state or bounds cannot be changed in the current Android "
      "configuration",
      response.Message());
  EXPECT_FALSE(fallback_window.IsMinimized());

  Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_destroyBrowserWindow(
      base::android::AttachCurrentThread(), /*taskId=*/1);
}

TEST(BrowserHandlerAndroidBoundsTest, BuildsBoundsAndStateFromWindow) {
  FakeBaseWindow window(/*bounds=*/gfx::Rect(10, 20, 800, 600),
                        /*restored_bounds=*/gfx::Rect(0, 0, 400, 300),
                        /*fullscreen=*/false,
                        /*maximized=*/false,
                        /*minimized=*/false);

  auto bounds = BrowserHandlerAndroid::BuildBrowserWindowBounds(&window);
  ASSERT_NE(nullptr, bounds);
  EXPECT_EQ(10, bounds->GetLeft());
  EXPECT_EQ(20, bounds->GetTop());
  EXPECT_EQ(800, bounds->GetWidth());
  EXPECT_EQ(600, bounds->GetHeight());
  EXPECT_EQ("normal", bounds->GetWindowState(""));
}

TEST(BrowserHandlerAndroidBoundsTest, BuildsRestoredBoundsWhenMinimized) {
  FakeBaseWindow window(/*bounds=*/gfx::Rect(10, 20, 800, 600),
                        /*restored_bounds=*/gfx::Rect(0, 0, 400, 300),
                        /*fullscreen=*/false,
                        /*maximized=*/false,
                        /*minimized=*/true);

  auto bounds = BrowserHandlerAndroid::BuildBrowserWindowBounds(&window);
  ASSERT_NE(nullptr, bounds);
  // Minimized windows report their restored bounds, not current bounds.
  EXPECT_EQ(0, bounds->GetLeft());
  EXPECT_EQ(0, bounds->GetTop());
  EXPECT_EQ(400, bounds->GetWidth());
  EXPECT_EQ(300, bounds->GetHeight());
  EXPECT_EQ("minimized", bounds->GetWindowState(""));
}

TEST(BrowserHandlerAndroidBoundsTest, BuildsFullscreenStateAndBounds) {
  FakeBaseWindow window(/*bounds=*/gfx::Rect(0, 0, 1920, 1080),
                        /*restored_bounds=*/gfx::Rect(100, 100, 800, 600),
                        /*fullscreen=*/true,
                        /*maximized=*/false,
                        /*minimized=*/false);

  auto bounds = BrowserHandlerAndroid::BuildBrowserWindowBounds(&window);
  ASSERT_NE(nullptr, bounds);
  EXPECT_EQ(1920, bounds->GetWidth());
  EXPECT_EQ(1080, bounds->GetHeight());
  EXPECT_EQ("fullscreen", bounds->GetWindowState(""));
}

TEST(BrowserHandlerAndroidBoundsTest,
     BuildsRestoredBoundsWhenMinimizedAndMaximized) {
  FakeBaseWindow window(/*bounds=*/gfx::Rect(10, 20, 800, 600),
                        /*restored_bounds=*/gfx::Rect(0, 0, 400, 300),
                        /*fullscreen=*/false,
                        /*maximized=*/true,
                        /*minimized=*/true);

  auto bounds = BrowserHandlerAndroid::BuildBrowserWindowBounds(&window);
  ASSERT_NE(nullptr, bounds);
  EXPECT_EQ(0, bounds->GetLeft());
  EXPECT_EQ(0, bounds->GetTop());
  EXPECT_EQ(400, bounds->GetWidth());
  EXPECT_EQ(300, bounds->GetHeight());
  EXPECT_EQ("maximized", bounds->GetWindowState(""));
}

TEST(BrowserHandlerAndroidBoundsTest,
     BuildsMaximizedBoundsForNonResizableMinimizedTask) {
  FakeBaseWindow window(/*bounds=*/gfx::Rect(10, 20, 800, 600),
                        /*restored_bounds=*/gfx::Rect(0, 0, 400, 300),
                        /*fullscreen=*/false,
                        /*maximized=*/false,
                        /*minimized=*/true,
                        /*can_resize=*/false);

  auto bounds = BrowserHandlerAndroid::BuildBrowserWindowBounds(&window);
  ASSERT_NE(nullptr, bounds);
  EXPECT_EQ(10, bounds->GetLeft());
  EXPECT_EQ(20, bounds->GetTop());
  EXPECT_EQ(800, bounds->GetWidth());
  EXPECT_EQ(600, bounds->GetHeight());
  EXPECT_EQ("maximized", bounds->GetWindowState(""));
}

class BrowserHandlerAndroidMutationTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void TrackWindow(FakeBaseWindow* window) {
    session_id_ = SessionID::NewUnique();
    browser_window_ = std::make_unique<FakeBrowserWindowInterface>(
        profile(), session_id_, window);
    handler_.TrackBrowserWindow(browser_window_.get());
  }

  protocol::Response SetWindowBounds(
      std::unique_ptr<protocol::Browser::Bounds> bounds) {
    return handler_.SetWindowBounds(session_id_.id(), std::move(bounds));
  }

  TestFrontendChannel channel_;
  protocol::UberDispatcher dispatcher_{&channel_};
  BrowserHandlerAndroid handler_{&dispatcher_, /*target_id=*/""};
  SessionID session_id_ = SessionID::NewUnique();
  std::unique_ptr<FakeBrowserWindowInterface> browser_window_;
};

TEST_F(BrowserHandlerAndroidMutationTest, RejectsUnknownWindow) {
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("maximized").Build();

  protocol::Response response =
      handler_.SetWindowBounds(SessionID::NewUnique().id(), std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ("Browser window not found", response.Message());
}

TEST_F(BrowserHandlerAndroidMutationTest, RejectsBoundsWithNonNormalState) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, false,
                        false);
  TrackWindow(&window);
  auto bounds = protocol::Browser::Bounds::Create()
                    .SetWidth(400)
                    .SetWindowState("maximized")
                    .Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ(
      "The 'minimized', 'maximized' and 'fullscreen' states cannot be "
      "combined with 'left', 'top', 'width' or 'height'",
      response.Message());
  EXPECT_FALSE(window.IsMaximized());
}

TEST_F(BrowserHandlerAndroidMutationTest, RejectsFullscreen) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, false,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("fullscreen").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ("Fullscreen not supported on Android", response.Message());
}

TEST_F(BrowserHandlerAndroidMutationTest, RejectsInvalidWindowState) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, false,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("invalid").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ("Invalid windowState: invalid", response.Message());
}

TEST_F(BrowserHandlerAndroidMutationTest, RejectsMaximizeFromMinimized) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, false,
                        true);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("maximized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ(
      "To maximize a minimized or fullscreen window, restore it to normal "
      "state first.",
      response.Message());
  EXPECT_FALSE(window.IsMaximized());
}

TEST_F(BrowserHandlerAndroidMutationTest, RejectsMaximizeFromFullscreen) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), true, false,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("maximized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ(
      "To maximize a minimized or fullscreen window, restore it to normal "
      "state first.",
      response.Message());
  EXPECT_FALSE(window.IsMaximized());
}

TEST_F(BrowserHandlerAndroidMutationTest, MaximizesWindow) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, false,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("maximized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_TRUE(response.IsSuccess());
  EXPECT_TRUE(window.IsMaximized());
}

TEST_F(BrowserHandlerAndroidMutationTest,
       MaximizeIsIdempotentForFixedSizeWindow) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, true,
                        false, /*can_resize=*/false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("maximized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_TRUE(response.IsSuccess());
  EXPECT_TRUE(window.IsMaximized());
}

TEST_F(BrowserHandlerAndroidMutationTest, MinimizesWindow) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, false,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("minimized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_TRUE(response.IsSuccess());
  EXPECT_TRUE(window.IsMinimized());
}

TEST_F(BrowserHandlerAndroidMutationTest, MinimizeIsIdempotent) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, false,
                        true);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("minimized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_TRUE(response.IsSuccess());
  EXPECT_TRUE(window.IsMinimized());
}

TEST_F(BrowserHandlerAndroidMutationTest, MinimizesMaximizedWindow) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, true,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("minimized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_TRUE(response.IsSuccess());
  EXPECT_FALSE(window.IsMaximized());
  EXPECT_TRUE(window.IsMinimized());
}

TEST_F(BrowserHandlerAndroidMutationTest, RejectsMinimizeFromFullscreen) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), true, false,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("minimized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ(
      "To minimize a fullscreen window, restore it to normal state first.",
      response.Message());
  EXPECT_FALSE(window.IsMinimized());
}

TEST_F(BrowserHandlerAndroidMutationTest,
       RejectsMutationForFixedSizeAndroidWindow) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, true,
                        false, /*can_resize=*/false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("minimized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ(
      "Window state or bounds cannot be changed in the current Android "
      "configuration",
      response.Message());
  EXPECT_FALSE(window.IsMinimized());
}

TEST_F(BrowserHandlerAndroidMutationTest,
       AllowsMutationWhileAndroidWindowIsPending) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(10, 20, 400, 300),
                        false, false, false,
                        /*can_resize=*/false);
  window.SetResizePrecheckResult(
      ui::WindowResizePrecheckResult::kAndroidNoActivity);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("minimized").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_TRUE(response.IsSuccess());
  EXPECT_TRUE(window.IsMinimized());
  auto reported_bounds =
      BrowserHandlerAndroid::BuildBrowserWindowBounds(&window);
  EXPECT_EQ("minimized", reported_bounds->GetWindowState(""));
  EXPECT_EQ(400, reported_bounds->GetWidth());
  EXPECT_EQ(300, reported_bounds->GetHeight());
}

TEST_F(BrowserHandlerAndroidMutationTest, RestoresWindow) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), false, true,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("normal").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_TRUE(response.IsSuccess());
  EXPECT_FALSE(window.IsMaximized());
  EXPECT_FALSE(window.IsMinimized());
}

TEST_F(BrowserHandlerAndroidMutationTest,
       PartialBoundsPreserveUnspecifiedValuesAndRestoreWindow) {
  FakeBaseWindow window(gfx::Rect(10, 20, 800, 600), gfx::Rect(), false, true,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetLeft(30).SetWidth(400).Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_TRUE(response.IsSuccess());
  EXPECT_EQ(gfx::Rect(30, 20, 400, 600), window.GetBounds());
  EXPECT_FALSE(window.IsMaximized());
}

TEST_F(BrowserHandlerAndroidMutationTest, RejectsNormalFromFullscreen) {
  FakeBaseWindow window(gfx::Rect(0, 0, 800, 600), gfx::Rect(), true, false,
                        false);
  TrackWindow(&window);
  auto bounds =
      protocol::Browser::Bounds::Create().SetWindowState("normal").Build();

  protocol::Response response = SetWindowBounds(std::move(bounds));

  EXPECT_FALSE(response.IsSuccess());
  EXPECT_EQ("Cannot exit fullscreen on Android", response.Message());
}
