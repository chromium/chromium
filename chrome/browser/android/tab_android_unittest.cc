// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/android/chrome_jni_headers/TabAndroidTestHelper_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_group_android.h"
#include "chrome/browser/android/tab_interface_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr int kTabId = 1;
}  // namespace

class TabAndroidTest : public testing::Test {
 public:
  TabAndroidTest() = default;
  ~TabAndroidTest() override = default;

  void SetUp() override {
    env_ = base::android::AttachCurrentThread();

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    TestingProfile::Builder builder;
    profile_ = builder.Build();

    java_tab_ = Java_TabAndroidTestHelper_createAndInitializeTabImpl(
        env_, kTabId, profile_->GetJavaObject(),
        static_cast<jint>(TabModel::TabLaunchType::FROM_LINK));
    ASSERT_FALSE(java_tab_.is_null()) << "Java tab creation failed.";

    tab_android_ = TabAndroid::GetNativeTab(env_, java_tab_);
    ASSERT_NE(nullptr, tab_android_)
        << "Failed to get native TabAndroid from Java TabImpl";
  }

  void TearDown() override {
    if (!java_tab_.is_null()) {
      // Call the destroy() method on the Java TabImpl object.
      // This will trigger TabAndroid::Destroy() via JNI.
      auto tab_impl_class = base::android::ScopedJavaLocalRef<jclass>::Adopt(
          env_, env_->GetObjectClass(java_tab_.obj()));
      ASSERT_FALSE(tab_impl_class.is_null());

      jmethodID destroy_method =
          env_->GetMethodID(tab_impl_class.obj(), "destroy", "()V");
      ASSERT_NE(nullptr, destroy_method)
          << "Failed to find TabImpl.destroy() method";

      env_->CallVoidMethod(java_tab_.obj(), destroy_method);
      // TabAndroid::Destroy calls 'delete this', so tab_android_ is now
      // dangling.
      tab_android_ = nullptr;
    }

    java_tab_.Reset();
    profile_.reset();  // Destroys TestingProfile.
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  raw_ptr<JNIEnv> env_ = nullptr;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<TestingProfile> profile_;
  base::android::ScopedJavaGlobalRef<jobject> java_tab_;

  // TabAndroid is owned by its Java counterpart via the native pointer.
  // It's deleted when the Java TabImpl.destroy() calls TabAndroid::Destroy().
  raw_ptr<TabAndroid> tab_android_ = nullptr;
};

TEST_F(TabAndroidTest, TabIsInitialized) {
  EXPECT_EQ(kTabId, tab_android_->GetAndroidId());
  EXPECT_NE(nullptr, tab_android_->profile());
}

TEST_F(TabAndroidTest, PinnedCollectionParent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chrome::android::kAndroidPinnedTabs);

  EXPECT_FALSE(tab_android_->IsPinned());

  std::unique_ptr<tabs::PinnedTabCollection> pinned_collection =
      std::make_unique<tabs::PinnedTabCollection>();
  pinned_collection->AddTab(std::make_unique<TabInterfaceAndroid>(tab_android_),
                            0);

  EXPECT_TRUE(tab_android_->IsPinned());
}

TEST_F(TabAndroidTest, TabGroupTabCollectionParent) {
  EXPECT_FALSE(tab_android_->GetGroup());

  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data;
  TabGroupAndroid::Factory factory(profile_.get());
  std::unique_ptr<tabs::TabGroupTabCollection> tab_group_collection =
      std::make_unique<tabs::TabGroupTabCollection>(factory, tab_group_id,
                                                    visual_data);
  tab_group_collection->AddTab(
      std::make_unique<TabInterfaceAndroid>(tab_android_), 0);

  EXPECT_EQ(tab_group_id, *(tab_android_->GetGroup()));
}

DEFINE_JNI(TabAndroidTestHelper)
