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
#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "cc/resources/ui_resource_client.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/android/java_bitmap.h"

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

}  // namespace

class TabContentManagerTest : public ::testing::Test {
 protected:
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

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  MockUIResourceProvider ui_resource_provider_;
  std::unique_ptr<TabContentManager> tab_content_manager_;
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
  EXPECT_TRUE(cb_defer.Get());
  EXPECT_FALSE(future4.IsReady());
  EXPECT_TRUE(HasPendingReadback(kTabId4));

  base::test::TestFuture<bool> cb_flush;
  tab_content_manager().CaptureThumbnail(
      tab4.get(), 1.f, /*return_bitmap=*/false,
      /*defer_until_next_frame=*/false, make_cb(cb_flush));
  EXPECT_TRUE(cb_flush.Get());
  EXPECT_TRUE(future4.Get());
  EXPECT_FALSE(HasPendingReadback(kTabId4));
}

}  // namespace android
