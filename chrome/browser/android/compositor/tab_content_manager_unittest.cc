// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_content_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "cc/resources/ui_resource_client.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/test/base/testing_profile.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "gpu/ipc/common/surface_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/gurl.h"

namespace android {
namespace {

#if DCHECK_IS_ON()
#define EXPECT_DCHECK(statement, regex) \
  EXPECT_DEATH_IF_SUPPORTED(statement, regex)
#else
#define EXPECT_DCHECK(statement, regex) \
  {                                     \
    statement;                          \
  }
#endif

constexpr int kDefaultCacheSize = 3;
constexpr int kCompressionQueueMaxSize = 2;
constexpr int kWriteQueueMaxSize = 2;

class MockUIResourceProvider : public ui::UIResourceProvider {
 public:
  MOCK_METHOD(cc::UIResourceId,
              CreateUIResource,
              (cc::UIResourceClient*),
              (override));
  MOCK_METHOD(void, DeleteUIResource, (cc::UIResourceId), (override));
  MOCK_METHOD(bool, SupportsETC1NonPowerOfTwo, (), (const, override));

  base::WeakPtr<UIResourceProvider> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockUIResourceProvider> weak_factory_{this};
};

class MockWindowAndroidCompositor : public ui::WindowAndroidCompositor {
 public:
  MOCK_METHOD(ScopedKeepSurfaceAliveCallback,
              TakeScopedKeepSurfaceAliveCallback,
              (const viz::SurfaceId& surface_id),
              (override));
  MOCK_METHOD(void,
              RequestCopyOfOutputOnRootLayer,
              (std::unique_ptr<viz::CopyOutputRequest> request),
              (override));
  MOCK_METHOD(void, SetNeedsAnimate, (), (override));
  MOCK_METHOD(ui::ResourceManager&, GetResourceManager, (), (override));
  MOCK_METHOD(viz::FrameSinkId, GetFrameSinkId, (), (override));
  MOCK_METHOD(gpu::SurfaceHandle, GetSurfaceHandle, (), (override));
  MOCK_METHOD(void,
              AddChildFrameSink,
              (const viz::FrameSinkId& frame_sink_id),
              (override));
  MOCK_METHOD(void,
              RemoveChildFrameSink,
              (const viz::FrameSinkId& frame_sink_id),
              (override));
  MOCK_METHOD(bool, IsDrawingFirstVisibleFrame, (), (const, override));
  MOCK_METHOD(void, OnUpdateRefreshRate, (float refresh_rate), (override));
  MOCK_METHOD(void,
              OnUpdateSupportedRefreshRates,
              (const std::vector<float>& supported_refresh_rates),
              (override));
  MOCK_METHOD(void, OnAdaptiveRefreshRateInfoChanged, (), (override));
  MOCK_METHOD(std::unique_ptr<ui::CompositorLock>,
              GetCompositorLock,
              (base::TimeDelta timeout),
              (override));
  MOCK_METHOD(void, OnUpdateOverlayTransform, (), (override));
  MOCK_METHOD(void,
              PostRequestSuccessfulPresentationTimeForNextFrame,
              (SuccessfulPresentationTimeCallback callback),
              (override));
  MOCK_METHOD(void,
              AddFrameSubmissionObserver,
              (FrameSubmissionObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveFrameSubmissionObserver,
              (FrameSubmissionObserver * observer),
              (override));
};

}  // namespace

class TabContentManagerTest : public ::testing::Test {
 protected:
  explicit TabContentManagerTest(
      const std::vector<base::test::FeatureRef>& enabled_features = {})
      : feature_list_(enabled_features) {}

  void SetUp() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    tab_content_manager_ = std::make_unique<TabContentManager>(
        env, nullptr, kDefaultCacheSize, kCompressionQueueMaxSize,
        kWriteQueueMaxSize, /*save_jpeg_thumbnails=*/true);
    tab_content_manager_->SetUIResourceProvider(
        ui_resource_provider_.GetWeakPtr());

    EXPECT_CALL(ui_resource_provider_, CreateUIResource(::testing::_))
        .WillRepeatedly(::testing::Return(1));
  }

  TabContentManager& tab_content_manager() { return *tab_content_manager_; }

  void SendThumbnailToJava(
      base::OnceCallback<void(const base::android::JavaRef<jobject>&)> callback,
      bool need_downsampling,
      bool result,
      const SkBitmap& bitmap) {
    tab_content_manager_->SendThumbnailToJava(
        std::move(callback), need_downsampling, result, bitmap);
  }

  bool HasPendingReadback(int tab_id) const {
    return tab_content_manager_->pending_tab_readbacks_.contains(tab_id);
  }

  void SeedDeferredReadback(int tab_id,
                            TabContentManager::JavaBitmapCallback callback) {
    tab_content_manager_->SeedDeferredReadbackForTesting(tab_id,
                                                         std::move(callback));
  }

  void SetCopyFromSurfaceForTesting(
      TabContentManager::CopyFromSurfaceCallback callback) {
    tab_content_manager_->SetCopyFromSurfaceForTesting(std::move(callback));
  }

  void DestroyTabContentManager() { tab_content_manager_.reset(); }

  bool has_tab_content_manager() const {
    return tab_content_manager_ != nullptr;
  }

  // Must precede task_environment_ so FeatureList outlives ThreadPool workers.
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockUIResourceProvider ui_resource_provider_;

 private:
  std::unique_ptr<TabContentManager> tab_content_manager_;
};

class TabContentManagerDeferredCaptureTest : public TabContentManagerTest {
 protected:
  static constexpr int kTabId = 105;

  TabContentManagerDeferredCaptureTest()
      : TabContentManagerTest({kDeferTabSwitchThumbnailCapture}) {}

  void SetUp() override {
    TabContentManagerTest::SetUp();
    window_ = ui::WindowAndroid::CreateForTesting();
    window_->get()->AttachCompositor(&compositor_);

    std::unique_ptr<content::WebContents> owned_web_contents =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    window_->get()->AddChild(owned_web_contents->GetNativeView());
    tab_ = TabAndroid::CreateForTesting(&profile_, kTabId,
                                        std::move(owned_web_contents));

    SetCopyFromSurfaceForTesting(base::BindLambdaForTesting(
        [this](base::OnceCallback<void(const content::CopyFromSurfaceResult&)>
                   callback) {
          ++copy_from_surface_calls_;
          pending_copy_callback_ = std::move(callback);
        }));
  }

  void TearDown() override {
    if (has_tab_content_manager()) {
      SetCopyFromSurfaceForTesting(base::NullCallback());
    }
    pending_copy_callback_.Reset();
    tab_.reset();
    window_->get()->DetachCompositor();
    window_.reset();
    TabContentManagerTest::TearDown();
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
  MockWindowAndroidCompositor compositor_;
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window_;
  std::unique_ptr<TabAndroid> tab_;
  int copy_from_surface_calls_ = 0;
  base::OnceCallback<void(const content::CopyFromSurfaceResult&)>
      pending_copy_callback_;
};

TEST_F(TabContentManagerTest, UpdateTabIdsForStaticLayerCache) {
  constexpr int kTabId1 = 6;
  constexpr int kTabId2 = 7;
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId1)); }, "");
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId2)); }, "");

  tab_content_manager().UpdateVisibleIds({kTabId1}, kTabId1);
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId1));
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId2)); }, "");

  tab_content_manager().UpdateVisibleIds({kTabId1}, -1);
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId1));
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId2)); }, "");

  tab_content_manager().UpdateVisibleIds({kTabId1, kTabId2}, -1);
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId1));
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId2));

  tab_content_manager().UpdateVisibleIds({kTabId2}, -1);
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId1)); }, "");
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId2));

  tab_content_manager().UpdateVisibleIds({}, -1);
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId1)); }, "");
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId2)); }, "");
}

TEST_F(TabContentManagerTest,
       SendThumbnailToJavaNoDownsamplingPreservesPixels) {
  constexpr int kWidth = 8;
  constexpr int kHeight = 6;
  SkBitmap input;
  ASSERT_TRUE(input.tryAllocN32Pixels(kWidth, kHeight));
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      input.erase(
          SkColorSetARGB(0xFF, (x * 31 + 17) & 0xFF, (y * 43 + 29) & 0xFF,
                         ((x + y) * 19 + 7) & 0xFF),
          SkIRect::MakeXYWH(x, y, 1, 1));
    }
  }
  input.setImmutable();

  bool callback_invoked = false;
  SendThumbnailToJava(
      base::BindOnce(
          [](const SkBitmap& expected, bool* invoked,
             const base::android::JavaRef<jobject>& j_bitmap) {
            *invoked = true;
            ASSERT_FALSE(j_bitmap.is_null());
            SkBitmap actual =
                gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_bitmap));
            ASSERT_EQ(actual.width(), expected.width());
            ASSERT_EQ(actual.height(), expected.height());
            for (int y = 0; y < expected.height(); ++y) {
              for (int x = 0; x < expected.width(); ++x) {
                EXPECT_EQ(actual.getColor(x, y), expected.getColor(x, y));
              }
            }
          },
          input, &callback_invoked),
      /*need_downsampling=*/false, /*result=*/true, input);
  EXPECT_TRUE(callback_invoked);
}

TEST_F(TabContentManagerTest, SendThumbnailToJavaWithDownsamplingHalvesSize) {
  static constexpr int kWidth = 8;
  static constexpr int kHeight = 6;
  SkBitmap input;
  ASSERT_TRUE(input.tryAllocN32Pixels(kWidth, kHeight));
  input.eraseColor(SK_ColorBLUE);
  input.setImmutable();

  bool callback_invoked = false;
  SendThumbnailToJava(
      base::BindOnce(
          [](bool* invoked, const base::android::JavaRef<jobject>& j_bitmap) {
            *invoked = true;
            ASSERT_FALSE(j_bitmap.is_null());
            SkBitmap actual =
                gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_bitmap));
            EXPECT_EQ(actual.width(), kWidth / 2);
            EXPECT_EQ(actual.height(), kHeight / 2);
            EXPECT_EQ(actual.getColor(0, 0), SK_ColorBLUE);
          },
          &callback_invoked),
      /*need_downsampling=*/true, /*result=*/true, input);
  EXPECT_TRUE(callback_invoked);
}

TEST_F(TabContentManagerTest, DeferredReadbackLifecycle) {
  constexpr int kTabId1 = 101;
  constexpr int kTabId2 = 102;
  constexpr int kTabId3 = 103;
  constexpr int kTabId4 = 104;

  auto make_cb = [](base::test::TestFuture<bool>& future) {
    return base::BindOnce(
        [](base::test::TestFuture<bool>* f,
           const base::android::JavaRef<jobject>&) { f->SetValue(true); },
        &future);
  };

  // 1. NativeRemoveTabThumbnail synchronously cancels unstarted request and
  // invokes callback.
  base::test::TestFuture<bool> future1;
  SeedDeferredReadback(kTabId1, make_cb(future1));
  EXPECT_TRUE(HasPendingReadback(kTabId1));
  tab_content_manager().NativeRemoveTabThumbnail(kTabId1);
  EXPECT_TRUE(future1.Get());
  EXPECT_FALSE(HasPendingReadback(kTabId1));

  // 2. WaitForJpegTabThumbnail flushes unstarted request and resolves JPEG
  // waiter.
  base::test::TestFuture<bool> future2;
  SeedDeferredReadback(kTabId2, make_cb(future2));
  EXPECT_TRUE(HasPendingReadback(kTabId2));
  base::test::TestFuture<bool> jpeg_waiter_future;
  tab_content_manager().WaitForJpegTabThumbnail(
      kTabId2, jpeg_waiter_future.GetCallback());
  EXPECT_TRUE(future2.Get());
  EXPECT_FALSE(jpeg_waiter_future.Get());
  EXPECT_FALSE(HasPendingReadback(kTabId2));

  // 3. FastForwardBy(base::Milliseconds(150)) triggers 150ms fallback timer.
  base::test::TestFuture<bool> future3;
  SeedDeferredReadback(kTabId3, make_cb(future3));
  EXPECT_TRUE(HasPendingReadback(kTabId3));
  task_environment_.FastForwardBy(base::Milliseconds(149));
  EXPECT_FALSE(future3.IsReady());
  EXPECT_TRUE(HasPendingReadback(kTabId3));
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(future3.Get());
  EXPECT_FALSE(HasPendingReadback(kTabId3));

  // 4. CaptureThumbnail on pending readback keeps deferral when
  // defer_until_next_frame=true && !return_bitmap, but flushes when
  // !defer_until_next_frame.
  base::test::TestFuture<bool> future4;
  SeedDeferredReadback(kTabId4, make_cb(future4));
  EXPECT_TRUE(HasPendingReadback(kTabId4));
  TestingProfile profile;
  std::unique_ptr<TabAndroid> tab4 =
      TabAndroid::CreateForTesting(&profile, kTabId4, nullptr);
  base::test::TestFuture<bool> cb_defer;
  tab_content_manager().CaptureThumbnail(
      tab4.get(), 1.f, /*return_bitmap=*/false, /*defer_until_next_frame=*/true,
      make_cb(cb_defer));
  EXPECT_FALSE(cb_defer.IsReady());
  EXPECT_FALSE(future4.IsReady());
  EXPECT_TRUE(HasPendingReadback(kTabId4));

  base::test::TestFuture<bool> cb_flush;
  tab_content_manager().CaptureThumbnail(
      tab4.get(), 1.f, /*return_bitmap=*/false,
      /*defer_until_next_frame=*/false, make_cb(cb_flush));
  ASSERT_TRUE(cb_flush.IsReady());
  ASSERT_TRUE(cb_defer.IsReady());
  ASSERT_TRUE(future4.IsReady());
  EXPECT_TRUE(cb_flush.Get());
  EXPECT_TRUE(cb_defer.Get());
  EXPECT_TRUE(future4.Get());
  EXPECT_FALSE(HasPendingReadback(kTabId4));
}

TEST_F(TabContentManagerDeferredCaptureTest,
       DeferredCaptureCollidingWithBitmapRequestPreservesCallback) {
  ui::WindowAndroidCompositor::SuccessfulPresentationTimeCallback
      presentation_callback;
  EXPECT_CALL(compositor_,
              PostRequestSuccessfulPresentationTimeForNextFrame(::testing::_))
      .WillOnce(
          [&](ui::WindowAndroidCompositor::SuccessfulPresentationTimeCallback
                  callback) { presentation_callback = std::move(callback); });

  base::test::TestFuture<bool> cb1_future;
  tab_content_manager().CaptureThumbnail(
      tab_.get(), 1.f, /*return_bitmap=*/false, /*defer_until_next_frame=*/true,
      base::BindLambdaForTesting(
          [&](const base::android::JavaRef<jobject>& j_bitmap) {
            cb1_future.SetValue(j_bitmap.is_null());
          }));

  EXPECT_TRUE(presentation_callback);
  EXPECT_EQ(copy_from_surface_calls_, 0);
  EXPECT_TRUE(tab_->GetContents()->IsBeingCaptured());
  EXPECT_FALSE(cb1_future.IsReady());

  bool cb2_called = false;
  SkBitmap cb2_bitmap;
  tab_content_manager().CaptureThumbnail(
      tab_.get(), 1.f, /*return_bitmap=*/true, /*defer_until_next_frame=*/false,
      base::BindLambdaForTesting(
          [&](const base::android::JavaRef<jobject>& j_bitmap) {
            cb2_called = true;
            if (!j_bitmap.is_null()) {
              cb2_bitmap =
                  gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_bitmap));
            }
          }));

  EXPECT_EQ(copy_from_surface_calls_, 1);
  ASSERT_TRUE(pending_copy_callback_);
  EXPECT_FALSE(cb2_called);

  SkBitmap captured_bitmap;
  ASSERT_TRUE(captured_bitmap.tryAllocN32Pixels(4, 4));
  captured_bitmap.eraseColor(SK_ColorRED);
  captured_bitmap.setImmutable();
  std::move(pending_copy_callback_)
      .Run(viz::CopyOutputBitmapWithMetadata(captured_bitmap));

  EXPECT_TRUE(cb1_future.Get());
  EXPECT_TRUE(cb2_called);
  ASSERT_FALSE(cb2_bitmap.isNull());
  EXPECT_EQ(cb2_bitmap.width(), 2);
  EXPECT_EQ(cb2_bitmap.height(), 2);
  EXPECT_EQ(cb2_bitmap.getColor(0, 0), SK_ColorRED);
  EXPECT_FALSE(tab_->GetContents()->IsBeingCaptured());
  EXPECT_FALSE(HasPendingReadback(kTabId));
}

TEST_F(TabContentManagerDeferredCaptureTest,
       DroppingInFlightReadbackReplacedByCollidingCapture) {
  struct HiddenObserver : public content::RenderWidgetHostObserver {
    void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost*,
                                           bool became_visible) override {
      if (!became_visible) {
        ++hidden_count;
      }
    }
    int hidden_count = 0;
  };

  tab_->GetContents()->WasShown();
  base::test::TestFuture<bool> dropped_cb_future;
  tab_content_manager().CaptureThumbnail(
      tab_.get(), 1.f, /*return_bitmap=*/false,
      /*defer_until_next_frame=*/false,
      base::BindLambdaForTesting(
          [&](const base::android::JavaRef<jobject>& j_bitmap) {
            dropped_cb_future.SetValue(j_bitmap.is_null());
          }));
  EXPECT_EQ(copy_from_surface_calls_, 1);
  ASSERT_TRUE(pending_copy_callback_);

  // Hide the tab while captured so hidden_capturer_count_ keeps it painting.
  tab_->GetContents()->WasHidden();
  HiddenObserver observer;
  base::ScopedObservation<content::RenderWidgetHost,
                          content::RenderWidgetHostObserver>
      observation(&observer);
  observation.Observe(tab_->GetContents()->GetRenderViewHost()->GetWidget());

  tab_content_manager().NativeRemoveTabThumbnail(kTabId);
  EXPECT_TRUE(HasPendingReadback(kTabId));
  EXPECT_FALSE(dropped_cb_future.IsReady());
  EXPECT_EQ(observer.hidden_count, 0);

  pending_copy_callback_.Reset();
  bool fresh_cb_called = false;
  SkBitmap fresh_cb_bitmap;
  tab_content_manager().CaptureThumbnail(
      tab_.get(), 0.5f, /*return_bitmap=*/true,
      /*defer_until_next_frame=*/false,
      base::BindLambdaForTesting(
          [&](const base::android::JavaRef<jobject>& j_bitmap) {
            fresh_cb_called = true;
            if (!j_bitmap.is_null()) {
              fresh_cb_bitmap =
                  gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_bitmap));
            }
          }));
  ASSERT_TRUE(dropped_cb_future.IsReady());
  EXPECT_TRUE(dropped_cb_future.Get());
  EXPECT_EQ(observer.hidden_count, 0);
  EXPECT_EQ(copy_from_surface_calls_, 2);
  EXPECT_FALSE(fresh_cb_called);
  ASSERT_TRUE(pending_copy_callback_);

  SkBitmap captured_bitmap;
  ASSERT_TRUE(captured_bitmap.tryAllocN32Pixels(4, 4));
  captured_bitmap.eraseColor(SK_ColorRED);
  captured_bitmap.setImmutable();
  std::move(pending_copy_callback_)
      .Run(viz::CopyOutputBitmapWithMetadata(captured_bitmap));
  EXPECT_TRUE(fresh_cb_called);
  ASSERT_FALSE(fresh_cb_bitmap.isNull());
  EXPECT_EQ(fresh_cb_bitmap.getColor(0, 0), SK_ColorRED);
  EXPECT_EQ(observer.hidden_count, 1);
  EXPECT_FALSE(HasPendingReadback(kTabId));
}

TEST_F(TabContentManagerDeferredCaptureTest,
       CacheTabWithBitmapCancelsPendingReadback) {
  content::WebContentsTester::For(tab_->GetContents())
      ->NavigateAndCommit(GURL("https://example.com"));
  tab_content_manager().UpdateVisibleIds({kTabId}, -1);

  base::test::TestFuture<bool> stale_cb_future;
  tab_content_manager().CaptureThumbnail(
      tab_.get(), 1.f, /*return_bitmap=*/true,
      /*defer_until_next_frame=*/false,
      base::BindLambdaForTesting(
          [&](const base::android::JavaRef<jobject>& j_bitmap) {
            stale_cb_future.SetValue(j_bitmap.is_null());
          }));
  EXPECT_EQ(copy_from_surface_calls_, 1);
  ASSERT_TRUE(pending_copy_callback_);
  EXPECT_TRUE(HasPendingReadback(kTabId));
  EXPECT_TRUE(tab_->GetContents()->IsBeingCaptured());

  content::WebContentsTester::For(tab_->GetContents())
      ->NavigateAndCommit(GURL("chrome-native://newtab"));

  SkBitmap native_bitmap;
  ASSERT_TRUE(native_bitmap.tryAllocN32Pixels(4, 4));
  native_bitmap.eraseColor(SK_ColorGREEN);
  native_bitmap.setImmutable();
  auto j_native_bitmap = gfx::ConvertToJavaBitmap(native_bitmap);

  EXPECT_CALL(ui_resource_provider_, CreateUIResource(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(1));
  tab_content_manager().CacheTabWithBitmap(tab_.get(), j_native_bitmap, 1.f);

  ASSERT_TRUE(stale_cb_future.IsReady());
  EXPECT_TRUE(stale_cb_future.Get());
  EXPECT_FALSE(HasPendingReadback(kTabId));
  EXPECT_FALSE(tab_->GetContents()->IsBeingCaptured());

  // Completing the stale readback callback must not overwrite the cached
  // bitmap.
  SkBitmap stale_bitmap;
  ASSERT_TRUE(stale_bitmap.tryAllocN32Pixels(4, 4));
  stale_bitmap.eraseColor(SK_ColorRED);
  stale_bitmap.setImmutable();
  std::move(pending_copy_callback_)
      .Run(viz::CopyOutputBitmapWithMetadata(stale_bitmap));
}

TEST_F(TabContentManagerDeferredCaptureTest,
       SynchronousReadbackCallbackCanDestroyManager) {
  tab_content_manager().UpdateVisibleIds({kTabId}, -1);

  SkBitmap captured_bitmap;
  ASSERT_TRUE(captured_bitmap.tryAllocN32Pixels(4, 4));
  captured_bitmap.eraseColor(SK_ColorRED);
  captured_bitmap.setImmutable();

  SetCopyFromSurfaceForTesting(base::BindLambdaForTesting(
      [&](base::OnceCallback<void(const content::CopyFromSurfaceResult&)>
              callback) {
        ++copy_from_surface_calls_;
        std::move(callback).Run(
            viz::CopyOutputBitmapWithMetadata(captured_bitmap));
      }));

  EXPECT_CALL(ui_resource_provider_, CreateUIResource(::testing::_))
      .Times(1)
      .WillOnce(::testing::Return(1));

  bool callback_invoked = false;
  SkBitmap received_bitmap;
  tab_content_manager().CaptureThumbnail(
      tab_.get(), 1.f, /*return_bitmap=*/true, /*defer_until_next_frame=*/false,
      base::BindLambdaForTesting(
          [&](const base::android::JavaRef<jobject>& j_bitmap) {
            callback_invoked = true;
            if (!j_bitmap.is_null()) {
              received_bitmap =
                  gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_bitmap));
            }
            DestroyTabContentManager();
          }));

  EXPECT_EQ(copy_from_surface_calls_, 1);
  EXPECT_TRUE(callback_invoked);
  EXPECT_FALSE(has_tab_content_manager());
  ASSERT_FALSE(received_bitmap.isNull());
  EXPECT_EQ(received_bitmap.width(), 2);
  EXPECT_EQ(received_bitmap.height(), 2);
  EXPECT_EQ(received_bitmap.getColor(0, 0), SK_ColorRED);
  EXPECT_FALSE(tab_->GetContents()->IsBeingCaptured());
}

}  // namespace android
