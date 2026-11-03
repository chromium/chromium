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
#include "cc/resources/ui_resource_client.h"
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

  content::BrowserTaskEnvironment task_environment_;

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

}  // namespace android
