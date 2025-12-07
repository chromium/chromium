// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/compositor_view.h"

#include <memory>
#include <optional>

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/presentation_feedback.h"

namespace android {

class MockCompositor : public content::Compositor {
 public:
  MockCompositor() = default;
  ~MockCompositor() override = default;

  MOCK_METHOD(void,
              SetRootWindow,
              (ui::WindowAndroid * window_android),
              (override));
  MOCK_METHOD(void,
              SetRootLayer,
              (scoped_refptr<cc::slim::Layer> root),
              (override));
  MOCK_METHOD(std::optional<int>,
              SetSurface,
              (const base::android::JavaRef<jobject>& surface,
               bool can_be_used_with_surface_control,
               const base::android::JavaRef<jobject>& browser_input_token),
              (override));
  MOCK_METHOD(void, SetWindowBounds, (const gfx::Size& size), (override));
  MOCK_METHOD(const gfx::Size&, GetWindowBounds, (), (override));
  MOCK_METHOD(void, SetRequiresAlphaChannel, (bool required), (override));
  MOCK_METHOD(void, SetNeedsComposite, (), (override));
  MOCK_METHOD(base::WeakPtr<ui::UIResourceProvider>,
              GetUIResourceProvider,
              (),
              (override));
  MOCK_METHOD(ui::ResourceManager&, GetResourceManager, (), (override));
  MOCK_METHOD(void, CacheBackBufferForCurrentSurface, (), (override));
  MOCK_METHOD(void, EvictCachedBackBuffer, (), (override));
  MOCK_METHOD(
      void,
      RequestSuccessfulPresentationTimeForNextFrame,
      (base::OnceCallback<void(const viz::FrameTimingDetails&)> callback),
      (override));
  MOCK_METHOD(
      void,
      RequestPresentationTimeForNextFrame,
      (base::OnceCallback<void(const gfx::PresentationFeedback&)> callback),
      (override));
  MOCK_METHOD(void, SetBackgroundColor, (int color), (override));
  MOCK_METHOD(void, PreserveChildSurfaceControls, (), (override));
  MOCK_METHOD(void,
              SetDidSwapBuffersCallbackEnabled,
              (bool enable),
              (override));
  MOCK_METHOD(void, OnViewAndroidAttached, (ui::ViewAndroid * view));
  MOCK_METHOD(void, OnViewAndroidDetached, (ui::ViewAndroid * view));
};

class FakeTabContentManager : public TabContentManager {
 public:
  explicit FakeTabContentManager(JNIEnv* env)
      : TabContentManager(env, nullptr, 0, 0, 0, false) {}
  ~FakeTabContentManager() override = default;
  void OnUIResourcesWereEvicted() {}
};

class CompositorViewTest : public ::testing::Test {
 public:
  CompositorViewTest() = default;

  void SetUp() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    auto compositor = std::make_unique<MockCompositor>();
    compositor_ = compositor.get();
    tab_content_manager_ = std::make_unique<FakeTabContentManager>(env);
    compositor_view_ =
        new CompositorView(env, nullptr, /*window_android=*/nullptr,
                           tab_content_manager_.get(), std::move(compositor));
  }

  void TearDown() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    compositor_view_->Destroy(env);
    compositor_view_ = nullptr;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<FakeTabContentManager> tab_content_manager_;
  raw_ptr<CompositorView> compositor_view_;
  raw_ptr<MockCompositor> compositor_;
};

TEST_F(CompositorViewTest, SetOverlayXrFullScreenMode) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Enable XR mode.
  EXPECT_CALL(*compositor_, SetBackgroundColor(SK_ColorTRANSPARENT));
  compositor_view_->SetOverlayXrFullScreenMode(env, true);

  // Calling again with true should do nothing.
  compositor_view_->SetOverlayXrFullScreenMode(env, true);

  // Disable XR mode.
  EXPECT_CALL(*compositor_, SetBackgroundColor(SK_ColorWHITE));
  compositor_view_->SetOverlayXrFullScreenMode(env, false);
}

}  // namespace android
