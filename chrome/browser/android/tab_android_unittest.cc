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
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_features.h"
#include "chrome/browser/android/tab_group_android.h"
#include "chrome/browser/android/tab_interface_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/actor/core/actor_features.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "content/public/common/window_container_type.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr int kTabId = 1;

class MockActorUiTabController
    : public actor::ui::ActorUiTabControllerInterface {
 public:
  explicit MockActorUiTabController(tabs::TabInterface& tab)
      : ActorUiTabControllerInterface(tab) {}
  ~MockActorUiTabController() override = default;

  // ActorUiTabControllerInterface:
  void OnUiTabStateChange(const actor::ui::UiTabState& ui_tab_state,
                          actor::ui::UiResultCallback callback) override {
    std::move(callback).Run(true);
  }
  void SetActorTaskPaused() override {}
  void SetActorTaskResume() override {}
  base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  actor::ui::UiTabState GetCurrentUiTabState() const override {
    return actor::ui::UiTabState();
  }

 private:
  base::WeakPtrFactory<MockActorUiTabController> weak_ptr_factory_{this};
};
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
        static_cast<int32_t>(TabModel::TabLaunchType::FROM_LINK));
    ASSERT_FALSE(java_tab_.is_null()) << "Java tab creation failed.";

    tab_android_ = TabAndroid::GetNativeTab(env_, java_tab_);
    ASSERT_NE(nullptr, tab_android_)
        << "Failed to get native TabAndroid from Java TabImpl";
  }

  void TearDown() override {
    if (!java_tab_.is_null()) {
      // Call the destroy() method on the Java TabImpl object.
      // This will trigger TabAndroid::Destroy() via JNI.
      auto tab_impl_class =
          jni_zero::AdoptRef(env_, env_->GetObjectClass(java_tab_.obj()));
      ASSERT_FALSE(tab_impl_class.is_null());

      jmethodID destroy_method =
          env_->GetMethodID(tab_impl_class.obj(), "destroy", "()I");
      ASSERT_NE(nullptr, destroy_method)
          << "Failed to find TabImpl.destroy() method";

      env_->CallIntMethod(java_tab_.obj(), destroy_method);
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

TEST_F(TabAndroidTest, WebUIEmbeddingContext) {
  // Create a test WebContents.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));

  content::WebContents* raw_web_contents = web_contents.get();

  // Create TabAndroid for testing.
  // Use a different tab ID than kTabId to avoid any potential conflicts.
  std::unique_ptr<TabAndroid> tab = TabAndroid::CreateForTesting(
      profile_.get(), kTabId + 1, std::move(web_contents));

  // Register the tab lookup helper.
  tabs::TabLookupFromWebContents::CreateForWebContents(raw_web_contents,
                                                       tab.get());

  // Verify context is initially empty.
  EXPECT_FALSE(webui::GetTabInterface(raw_web_contents));

  // Construct TabFeatures. This should initialize the embedding context.
  tabs::TabFeatures tab_features(raw_web_contents, profile_.get());

  // Verify that GetTabInterface returns correct tab.
  EXPECT_EQ(tab.get(), webui::GetTabInterface(raw_web_contents));
}

class GlicTabAndroidTest : public TabAndroidTest {
 public:
  GlicTabAndroidTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicActor},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicTabAndroidTest, IsWebContentsCreationOverridden_GlicSandboxCheck) {
  content::RenderViewHostTestEnabler rvh_test_enabler;

  // Create a WebContents.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  content::WebContents* raw_web_contents = web_contents.get();

  // Create TabAndroid for testing.
  std::unique_ptr<TabAndroid> tab = TabAndroid::CreateForTesting(
      profile_.get(), kTabId + 1, std::move(web_contents));

  // Register the tab lookup helper.
  tabs::TabLookupFromWebContents::CreateForWebContents(raw_web_contents,
                                                       tab.get());

  // Create the delegate with a null Java reference.
  auto delegate = std::make_unique<android::TabWebContentsDelegateAndroid>(
      env_, base::android::ScopedJavaLocalRef<jobject>());

  // Set the delegate on the web contents.
  raw_web_contents->SetDelegate(delegate.get());

  content::RenderFrameHost* main_frame =
      raw_web_contents->GetPrimaryMainFrame();

  // 1. Without Glic Actor active, it should return false.
  EXPECT_FALSE(delegate->IsWebContentsCreationOverridden(
      main_frame, nullptr, content::mojom::WindowContainerType::NORMAL, GURL(),
      "", GURL()));

  // 2. Start Glic Actor task and attach the tab to it.
  actor::ActorKeyedService* service =
      actor::ActorKeyedService::Get(profile_.get());
  ASSERT_NE(nullptr, service);
  actor::TaskId task_id = service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor::ActorTask* task = service->GetTask(task_id);
  ASSERT_NE(nullptr, task);

  // Add the tab to the task.
  MockActorUiTabController mock_controller(*tab);
  actor::AddTabToTask(*tab, *task);

  // Ensure HasActorTaskPreventingNewWebContents returns true.
  ASSERT_TRUE(actor::HasActorTaskPreventingNewWebContents(main_frame));

  // 3. Under Glic Actor control, if the frame is NOT sandboxed, it should
  // override creation.
  EXPECT_TRUE(delegate->IsWebContentsCreationOverridden(
      main_frame, nullptr, content::mojom::WindowContainerType::NORMAL, GURL(),
      "", GURL()));

  // 4. Under Glic Actor control, if the frame IS sandboxed with kTopNavigation,
  // it should NOT override creation.
  auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://sandboxed.example.com"), raw_web_contents);
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader("Content-Security-Policy", "sandbox allow-popups");
  simulator->SetResponseHeaders(headers);
  simulator->Commit();

  content::RenderFrameHost* sandboxed_frame =
      raw_web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(sandboxed_frame->IsSandboxed(
      network::mojom::WebSandboxFlags::kTopNavigation));

  EXPECT_FALSE(delegate->IsWebContentsCreationOverridden(
      sandboxed_frame, nullptr, content::mojom::WindowContainerType::NORMAL,
      GURL(), "", GURL()));
}

TEST_F(TabAndroidTest, Getters) {
  TabInterfaceAndroid tab_interface(tab_android_);
  EXPECT_EQ(u"about:blank", tab_interface.GetTitle());
  EXPECT_EQ(GURL("about:blank"), tab_interface.GetURL());
  base::Time last_active_time = tab_interface.GetLastActiveTime();
  EXPECT_LT(base::Time::UnixEpoch(), last_active_time);
}

DEFINE_JNI(TabAndroidTestHelper)
