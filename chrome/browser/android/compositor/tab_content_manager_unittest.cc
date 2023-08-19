// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_content_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "cc/resources/ui_resource_client.h"
#include "chrome/browser/thumbnail/cc/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/ui_android_export.h"

namespace android {
namespace {

#if DCHECK_IS_ON()
#define EXPECT_DCHECK(statement, regex) \
  EXPECT_DEATH_IF_SUPPORTED(statement, regex)
#else
#define EXPECT_DCHECK(statement, regex) \
  { statement; }
#endif

constexpr int kDefaultCacheSize = 3;
constexpr int kApproximationCacheSize = 0;
constexpr int kCompressionQueueMaxSize = 2;
constexpr int kWriteQueueMaxSize = 2;
constexpr bool kUseApproximationThumbnail = false;
constexpr double kJpegAspectRatio = 0.85;

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
        env, nullptr, kDefaultCacheSize, kApproximationCacheSize,
        kCompressionQueueMaxSize, kWriteQueueMaxSize,
        kUseApproximationThumbnail, kJpegAspectRatio);
    tab_content_manager_->SetUIResourceProvider(
        ui_resource_provider_.GetWeakPtr());

    EXPECT_CALL(ui_resource_provider_, CreateUIResource(::testing::_))
        .WillRepeatedly(::testing::Return(1));
    scoped_feature_list_.InitWithFeatures({thumbnail::kThumbnailCacheRefactor},
                                          {});
  }

  TabContentManager& tab_content_manager() { return *tab_content_manager_; }

  content::BrowserTaskEnvironment task_environment_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockUIResourceProvider ui_resource_provider_;
  std::unique_ptr<TabContentManager> tab_content_manager_;
};

TEST_F(TabContentManagerTest, UpdateTabIdsForStaticLayerCache) {
  JNIEnv* env = base::android::AttachCurrentThread();
  constexpr int kTabId1 = 6;
  constexpr int kTabId2 = 7;
  EXPECT_DCHECK(
      {
        EXPECT_FALSE(
            tab_content_manager().GetOrCreateStaticLayer(kTabId1, true));
      },
      "");
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId1)); }, "");
  EXPECT_DCHECK(
      {
        EXPECT_FALSE(
            tab_content_manager().GetOrCreateStaticLayer(kTabId2, true));
      },
      "");
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId2)); }, "");

  auto jarr = base::android::ToJavaIntArray(env, std::vector<int>({kTabId1}));
  tab_content_manager().UpdateVisibleIds(
      env, base::android::JavaParamRef<jintArray>(env, jarr.obj()), kTabId1);
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId1));
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId2)); }, "");

  tab_content_manager().UpdateVisibleIds(
      env, base::android::JavaParamRef<jintArray>(env, jarr.obj()), -1);
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId1));
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId2)); }, "");

  jarr =
      base::android::ToJavaIntArray(env, std::vector<int>({kTabId1, kTabId2}));
  tab_content_manager().UpdateVisibleIds(
      env, base::android::JavaParamRef<jintArray>(env, jarr.obj()), -1);
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId1));
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId2));

  jarr = base::android::ToJavaIntArray(env, std::vector<int>({kTabId2}));
  tab_content_manager().UpdateVisibleIds(
      env, base::android::JavaParamRef<jintArray>(env, jarr.obj()), -1);
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId1)); }, "");
  EXPECT_TRUE(tab_content_manager().GetStaticLayer(kTabId2));

  jarr = base::android::ToJavaIntArray(env, std::vector<int>({}));
  tab_content_manager().UpdateVisibleIds(
      env, base::android::JavaParamRef<jintArray>(env, jarr.obj()), -1);
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId1)); }, "");
  EXPECT_DCHECK(
      { EXPECT_FALSE(tab_content_manager().GetStaticLayer(kTabId2)); }, "");
}

}  // namespace android
