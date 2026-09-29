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
#include "cc/slim/layer.h"
#include "chrome/android/chrome_jni_headers/TabAndroidTestHelper_jni.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/actor/ui/test_support/mock_actor_ui_tab_controller.h"
#include "chrome/browser/android/tab_features.h"
#include "chrome/browser/android/tab_group_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/actor/core/actor_features.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/common/window_container_type.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"

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
  content::RenderViewHostTestEnabler rvh_test_enabler_;

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
  EXPECT_NE(nullptr, tab_android_->GetProfile());
}

TEST_F(TabAndroidTest, FromTabInterface) {
  // Non-const valid pointer.
  tabs::TabInterface* tab_interface = tab_android_.get();
  EXPECT_EQ(tab_android_.get(), TabAndroid::FromTabInterface(tab_interface));

  // Non-const null pointer.
  EXPECT_EQ(nullptr, TabAndroid::FromTabInterface(
                         static_cast<tabs::TabInterface*>(nullptr)));

  // Const valid pointer.
  const tabs::TabInterface* const_tab_interface = tab_android_.get();
  const TabAndroid* const_tab_android = tab_android_.get();
  EXPECT_EQ(const_tab_android,
            TabAndroid::FromTabInterface(const_tab_interface));

  // Const null pointer.
  EXPECT_EQ(nullptr, TabAndroid::FromTabInterface(
                         static_cast<const tabs::TabInterface*>(nullptr)));
}

TEST_F(TabAndroidTest, FromTabHandle) {
  // Valid handle.
  tabs::TabHandle valid_handle = tab_android_->GetHandle();
  EXPECT_EQ(tab_android_.get(), TabAndroid::FromTabHandle(valid_handle));

  // Invalid handle: default constructed.
  tabs::TabHandle default_handle;
  EXPECT_EQ(nullptr, TabAndroid::FromTabHandle(default_handle));

  // Invalid handle: explicit null handle.
  tabs::TabHandle null_handle = tabs::TabHandle::Null();
  EXPECT_EQ(nullptr, TabAndroid::FromTabHandle(null_handle));

  // Invalid handle: non-existent ID.
  tabs::TabHandle non_existent_handle(123456);
  EXPECT_EQ(nullptr, TabAndroid::FromTabHandle(non_existent_handle));

  // Invalid handle: handle for a destroyed tab.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  std::unique_ptr<TabAndroid> temp_tab = TabAndroid::CreateForTesting(
      profile_.get(), kTabId + 1, std::move(web_contents));
  tabs::TabHandle temp_handle = temp_tab->GetHandle();
  EXPECT_EQ(temp_tab.get(), TabAndroid::FromTabHandle(temp_handle));
  temp_tab.reset();
  EXPECT_EQ(nullptr, TabAndroid::FromTabHandle(temp_handle));
}

TEST_F(TabAndroidTest, PinnedCollectionParent) {
  EXPECT_FALSE(tab_android_->IsPinned());

  std::unique_ptr<tabs::PinnedTabCollection> pinned_collection =
      std::make_unique<tabs::PinnedTabCollection>();
  pinned_collection->AddTab(tabs::ScopedTab(tab_android_), 0);

  EXPECT_TRUE(tab_android_->IsPinned());
  tabs::ScopedTab removed_pinned_tab =
      pinned_collection->MaybeRemoveTab(tab_android_);
  EXPECT_EQ(removed_pinned_tab.get(), tab_android_);
}

TEST_F(TabAndroidTest, TabGroupTabCollectionParent) {
  EXPECT_FALSE(tab_android_->GetGroup());

  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data;
  TabGroupAndroid::Factory factory(profile_.get());
  std::unique_ptr<tabs::TabGroupTabCollection> tab_group_collection =
      std::make_unique<tabs::TabGroupTabCollection>(factory, tab_group_id,
                                                    visual_data);
  tab_group_collection->AddTab(tabs::ScopedTab(tab_android_), 0);

  EXPECT_EQ(tab_group_id, *(tab_android_->GetGroup()));
  tabs::ScopedTab removed_group_tab =
      tab_group_collection->MaybeRemoveTab(tab_android_);
  EXPECT_EQ(removed_group_tab.get(), tab_android_);
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

// Covers the `beforeunload` auto-close gate: whether Content is allowed to
// close a page once a `beforeunload` completion arrives, and how the one-shot
// suppression mark that Java's TabRemover sets behaves.
class TabWebContentsDelegateBeforeUnloadTest : public TabAndroidTest {
 protected:
  void SetUp() override {
    TabAndroidTest::SetUp();
    delegate_ = std::make_unique<android::TabWebContentsDelegateAndroid>(
        env_, base::android::ScopedJavaLocalRef<jobject>(), nullptr);
  }

  void TearDown() override {
    delegate_.reset();
    TabAndroidTest::TearDown();
  }

  // The delegate is installed so that these tests run against the same object
  // graph production does: the gate only matters for a WebContents whose
  // delegate is a TabWebContentsDelegateAndroid.
  std::unique_ptr<content::WebContents> CreateWebContents() {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile_.get()));
    web_contents->SetDelegate(delegate_.get());
    return web_contents;
  }

  // Answers "would Content close the page?". The false initialiser mirrors
  // RenderFrameHostManager::BeforeUnloadCompleted, so a delegate that failed to
  // write an answer reads as "do not close".
  bool WouldAutoClose(content::WebContents* web_contents, bool proceed) {
    bool proceed_to_fire_unload = false;
    delegate_->BeforeUnloadFired(web_contents, proceed,
                                 &proceed_to_fire_unload);
    return proceed_to_fire_unload;
  }

  // Reads the mark without consuming it.
  bool IsSuppressed(const content::WebContents* web_contents) {
    return android::TabWebContentsDelegateAndroid::
        ShouldSuppressBeforeUnloadAutoCloseForTesting(web_contents);
  }

  std::unique_ptr<android::TabWebContentsDelegateAndroid> delegate_;
};

// An unmarked page closes when the user agrees to leave.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, AllowsAutoCloseWhenProceeding) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  EXPECT_FALSE(IsSuppressed(web_contents.get()));
  EXPECT_TRUE(WouldAutoClose(web_contents.get(), /*proceed=*/true));
}

// An unmarked page survives when the user declines. Every Android path that
// prompts before closing a tab depends on this, so a delegate that answered
// unconditionally -- as the base WebContentsDelegate does -- would close tabs
// out from under a "Stay".
TEST_F(TabWebContentsDelegateBeforeUnloadTest, BlocksAutoCloseWhenCancelling) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  EXPECT_FALSE(IsSuppressed(web_contents.get()));
  EXPECT_FALSE(WouldAutoClose(web_contents.get(), /*proceed=*/false));
}

// The mark must be consumed by the completion it was set for, so that a later
// unrelated closure is unaffected.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, SuppressionIsOneShot) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/true);
  EXPECT_TRUE(IsSuppressed(web_contents.get()));

  EXPECT_FALSE(WouldAutoClose(web_contents.get(), /*proceed=*/true));

  EXPECT_FALSE(IsSuppressed(web_contents.get()));
  EXPECT_TRUE(WouldAutoClose(web_contents.get(), /*proceed=*/true));
}

// Both answers end the closure, so the cancel path consumes the mark too. A
// mark left behind by a cancelled prompt would silently suppress whatever the
// tab did next.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, SuppressionConsumedOnCancel) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/true);

  EXPECT_FALSE(WouldAutoClose(web_contents.get(), /*proceed=*/false));
  EXPECT_FALSE(IsSuppressed(web_contents.get()));
}

// Marking one tab must not silence another. A batch closure marks each tab
// individually, so cross-talk here would drop real closures on the floor.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, SuppressionIsPerWebContents) {
  std::unique_ptr<content::WebContents> marked = CreateWebContents();
  std::unique_ptr<content::WebContents> unmarked = CreateWebContents();

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      marked.get(), /*suppress=*/true);

  EXPECT_TRUE(WouldAutoClose(unmarked.get(), /*proceed=*/true));
  EXPECT_TRUE(IsSuppressed(marked.get()));
  EXPECT_FALSE(WouldAutoClose(marked.get(), /*proceed=*/true));
}

// A closure abandoned before `beforeunload` completes must be able to take the
// mark back off, so it cannot leak into whatever the tab does next.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, SuppressionCanBeClearedEarly) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/true);
  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/false);

  EXPECT_FALSE(IsSuppressed(web_contents.get()));
  EXPECT_TRUE(WouldAutoClose(web_contents.get(), /*proceed=*/true));
}

// Setting the mark twice must not make it take two completions to clear.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, RepeatedMarkIsIdempotent) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/true);
  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/true);

  EXPECT_FALSE(WouldAutoClose(web_contents.get(), /*proceed=*/true));
  EXPECT_TRUE(WouldAutoClose(web_contents.get(), /*proceed=*/true));
}

// Tab.getWebContents() is nullable, so the Java entry point can arrive with no
// native WebContents behind it. (BeforeUnloadFired() itself always gets a real
// one -- Content passes the WebContents that is firing.)
TEST_F(TabWebContentsDelegateBeforeUnloadTest, NullWebContentsIsSafe) {
  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      nullptr, /*suppress=*/true);
  EXPECT_FALSE(IsSuppressed(nullptr));
}

// Clearing a mark that was never set must not disturb the default behaviour.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, ClearingUnmarkedIsHarmless) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/false);

  EXPECT_TRUE(WouldAutoClose(web_contents.get(), /*proceed=*/true));
}

// A new primary page abandons a pending `beforeunload` without a completion,
// so the mark has to go with it. One left behind would sit on a live tab and
// spend itself on the next closure, which would then refuse to close a tab the
// user had agreed to leave.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, MarkClearedByPrimaryPageChange) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/true);
  ASSERT_TRUE(IsSuppressed(web_contents.get()));

  content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), web_contents.get())
      ->Commit();

  EXPECT_FALSE(IsSuppressed(web_contents.get()));
  EXPECT_TRUE(WouldAutoClose(web_contents.get(), /*proceed=*/true));
}

// A same-document navigation leaves the page in place, so a dispatch in flight
// still completes and the mark must survive to meet it. Clearing too eagerly is
// the dangerous direction: Content would then auto-close a tab that TabRemover
// is already closing, and for a cancelled batch it would close one the user
// chose to keep.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, MarkSurvivesSameDocument) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), web_contents.get())
      ->Commit();

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/true);
  ASSERT_TRUE(IsSuppressed(web_contents.get()));

  content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com#fragment"),
      web_contents->GetPrimaryMainFrame())
      ->CommitSameDocument();

  EXPECT_TRUE(IsSuppressed(web_contents.get()));
  EXPECT_FALSE(WouldAutoClose(web_contents.get(), /*proceed=*/true));
}

// A gone renderer does not stop RenderFrameHostImpl's `beforeunload` timeout,
// so a completion still arrives after it and must find the mark. Dropping the
// mark here would let Content close a tab whose closure Java has already
// resolved, half a second after the fact.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, MarkSurvivesRendererGone) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://example.com"), web_contents.get())
      ->Commit();

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      web_contents.get(), /*suppress=*/true);
  ASSERT_TRUE(IsSuppressed(web_contents.get()));

  static_cast<content::MockRenderProcessHost*>(
      web_contents->GetPrimaryMainFrame()->GetProcess())
      ->SimulateCrash();

  EXPECT_TRUE(IsSuppressed(web_contents.get()));
  EXPECT_FALSE(WouldAutoClose(web_contents.get(), /*proceed=*/true));
}

// A WebContents that leaves its tab leaves the tab's closure behind. The
// delegate that consumes marks is detached by the same call, so a surviving
// mark would be spent on a later closure under whichever delegate comes next.
TEST_F(TabWebContentsDelegateBeforeUnloadTest, MarkClearedWhenTabReleasesIt) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  content::WebContents* raw_web_contents = web_contents.get();
  std::unique_ptr<TabAndroid> tab = TabAndroid::CreateForTesting(
      profile_.get(), kTabId + 1, std::move(web_contents));

  android::TabWebContentsDelegateAndroid::SetSuppressBeforeUnloadAutoClose(
      raw_web_contents, /*suppress=*/true);
  ASSERT_TRUE(IsSuppressed(raw_web_contents));

  std::unique_ptr<content::WebContents> released =
      tab->ReleaseWebContentsForTesting();

  EXPECT_FALSE(IsSuppressed(released.get()));
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
      env_, base::android::ScopedJavaLocalRef<jobject>(), raw_web_contents);

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
  actor::ui::MockActorUiTabController mock_controller(*tab);
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
  EXPECT_EQ(u"about:blank", tab_android_->GetTitle());
  EXPECT_EQ(GURL("about:blank"), tab_android_->GetURL());
  base::Time last_active_time = tab_android_->GetLastActiveTime();
  EXPECT_LT(base::Time::UnixEpoch(), last_active_time);
}

TEST_F(TabAndroidTest, LazyInitialization) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  content::WebContents* raw_web_contents = web_contents.get();
  std::unique_ptr<TabAndroid> tab = TabAndroid::CreateForTesting(
      profile_.get(), kTabId + 1, std::move(web_contents));

  // SyncedTabDelegate is lazily initialized on first access.
  sync_sessions::SyncedTabDelegate* synced_tab_delegate =
      tab->GetSyncedTabDelegate();
  ASSERT_NE(nullptr, synced_tab_delegate);
  EXPECT_EQ(synced_tab_delegate, tab->GetSyncedTabDelegate());
  EXPECT_FALSE(synced_tab_delegate->IsPlaceholderTab());

  // ContentLayer is lazily initialized on first access and parents the
  // WebContents native view layer.
  scoped_refptr<cc::slim::Layer> content_layer = tab->GetContentLayer();
  ASSERT_NE(nullptr, content_layer);
  EXPECT_EQ(content_layer, tab->GetContentLayer());
  ASSERT_EQ(1u, content_layer->children().size());
  EXPECT_EQ(raw_web_contents->GetNativeView()->GetLayer(),
            content_layer->children()[0]);

  // Releasing web contents resets content_layer_ and delegate.
  std::unique_ptr<content::WebContents> released_contents =
      tab->ReleaseWebContentsForTesting();

  // Next call to GetContentLayer lazily recreates the layer with no children.
  scoped_refptr<cc::slim::Layer> new_content_layer = tab->GetContentLayer();
  ASSERT_NE(nullptr, new_content_layer);
  EXPECT_NE(content_layer, new_content_layer);
  EXPECT_TRUE(new_content_layer->children().empty());

  // SyncedTabDelegate remains functional with null WebContents and becomes
  // a placeholder tab.
  EXPECT_NE(nullptr, tab->GetSyncedTabDelegate());
  EXPECT_TRUE(tab->GetSyncedTabDelegate()->IsPlaceholderTab());
}

TEST_F(TabAndroidTest, DestroyWebContentsResetsContentLayerAndPlaceholder) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  content::WebContents* raw_web_contents = web_contents.get();
  std::unique_ptr<TabAndroid> tab = TabAndroid::CreateForTesting(
      profile_.get(), kTabId + 1, std::move(web_contents));

  sync_sessions::SyncedTabDelegate* synced_tab_delegate =
      tab->GetSyncedTabDelegate();
  ASSERT_NE(nullptr, synced_tab_delegate);
  EXPECT_FALSE(synced_tab_delegate->IsPlaceholderTab());

  scoped_refptr<cc::slim::Layer> content_layer = tab->GetContentLayer();
  ASSERT_NE(nullptr, content_layer);
  ASSERT_EQ(1u, content_layer->children().size());
  EXPECT_EQ(raw_web_contents->GetNativeView()->GetLayer(),
            content_layer->children()[0]);

  // Destroying WebContents resets content_layer_ and resets WebContents on
  // SyncedTabDelegate.
  tab->DestroyWebContents();

  // SyncedTabDelegate now reports as a placeholder tab.
  EXPECT_TRUE(synced_tab_delegate->IsPlaceholderTab());

  // Next call to GetContentLayer creates an empty layer.
  scoped_refptr<cc::slim::Layer> new_content_layer = tab->GetContentLayer();
  ASSERT_NE(nullptr, new_content_layer);
  EXPECT_NE(content_layer, new_content_layer);
  EXPECT_TRUE(new_content_layer->children().empty());
}

TEST_F(TabAndroidTest, ReverseInitializationOrder) {
  // 1. Create tab without WebContents.
  std::unique_ptr<TabAndroid> tab =
      TabAndroid::CreateForTesting(profile_.get(), kTabId + 2, nullptr);

  // 2. SyncedTabDelegate is lazily created and reports as a placeholder tab.
  sync_sessions::SyncedTabDelegate* sync_delegate = tab->GetSyncedTabDelegate();
  ASSERT_NE(nullptr, sync_delegate);
  EXPECT_TRUE(sync_delegate->IsPlaceholderTab());

  // 3. ContentLayer is lazily created and has no child layers.
  scoped_refptr<cc::slim::Layer> content_layer = tab->GetContentLayer();
  ASSERT_NE(nullptr, content_layer);
  EXPECT_TRUE(content_layer->children().empty());

  // 4. Attach a WebContents layer and verify layer hierarchy parenting.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  content::WebContents* raw_web_contents = web_contents.get();
  tab->AttachWebContentsToContentLayer(raw_web_contents);

  ASSERT_EQ(1u, content_layer->children().size());
  EXPECT_EQ(raw_web_contents->GetNativeView()->GetLayer(),
            content_layer->children()[0]);

  // 5. Test idempotency of AttachWebContentsToContentLayer.
  tab->AttachWebContentsToContentLayer(raw_web_contents);
  ASSERT_EQ(1u, content_layer->children().size());
  EXPECT_EQ(raw_web_contents->GetNativeView()->GetLayer(),
            content_layer->children()[0]);

  // 6. Test placeholder transition and layer reset on destruction/release
  // with a tab holding WebContents.
  std::unique_ptr<content::WebContents> web_contents2 =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile_.get()));
  content::WebContents* raw_web_contents2 = web_contents2.get();
  std::unique_ptr<TabAndroid> tab_with_contents = TabAndroid::CreateForTesting(
      profile_.get(), kTabId + 3, std::move(web_contents2));

  EXPECT_FALSE(tab_with_contents->GetSyncedTabDelegate()->IsPlaceholderTab());
  scoped_refptr<cc::slim::Layer> layer2 = tab_with_contents->GetContentLayer();
  ASSERT_NE(nullptr, layer2);
  ASSERT_EQ(1u, layer2->children().size());
  EXPECT_EQ(raw_web_contents2->GetNativeView()->GetLayer(),
            layer2->children()[0]);

  std::unique_ptr<content::WebContents> released_contents =
      tab_with_contents->ReleaseWebContentsForTesting();
  EXPECT_TRUE(tab_with_contents->GetSyncedTabDelegate()->IsPlaceholderTab());
  scoped_refptr<cc::slim::Layer> reset_layer =
      tab_with_contents->GetContentLayer();
  ASSERT_NE(nullptr, reset_layer);
  EXPECT_NE(layer2, reset_layer);
  EXPECT_TRUE(reset_layer->children().empty());
}


TEST_F(TabAndroidTest, CollectionDestructionClearsParentPointer) {
  EXPECT_EQ(tab_android_->GetParentCollection(), nullptr);

  auto pinned_collection = std::make_unique<tabs::PinnedTabCollection>();
  tabs::TabCollection* pinned_collection_ptr = pinned_collection.get();
  pinned_collection->AddTab(tabs::ScopedTab(tab_android_), 0);
  EXPECT_EQ(tab_android_->GetParentCollection(), pinned_collection_ptr);

  // Destroy the collection holding the tab.
  pinned_collection.reset();

  // The TabAndroid parent pointer should be cleanly cleared to nullptr,
  // and subsequent destruction in TearDown() will not hit
  // CHECK(!parent_collection_).
  EXPECT_EQ(tab_android_->GetParentCollection(), nullptr);
}

DEFINE_JNI(TabAndroidTestHelper)
