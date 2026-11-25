// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "cc/slim/layer.h"
#include "chrome/android/chrome_jni_headers/TabAndroidTestHelper_jni.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_registry.h"
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
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/pinned_tab_collection.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_collection.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/window_container_type.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
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

    tab_android_ = CreateTab();
  }

  void TearDown() override {
    // DestroyTab() calls TabAndroid::Destroy(), which deletes the TabAndroid
    // object, so here we need to clear the `tab_android_` raw_ptr first to
    // avoid a dangling pointer.
    DestroyTab(tab_android_.ExtractAsDangling());

    profile_.reset();
  }

 protected:
  TabAndroid* CreateTab() {
    base::android::ScopedJavaLocalRef<jobject> java_tab =
        Java_TabAndroidTestHelper_createAndInitializeTabImpl(
            env_, next_tab_id_++, profile_->GetJavaObject(),
            static_cast<int32_t>(TabModel::TabLaunchType::FROM_LINK));
    CHECK(!java_tab.is_null()) << "Java tab creation failed.";

    TabAndroid* tab_android = TabAndroid::GetNativeTab(env_, java_tab);
    CHECK(tab_android) << "Failed to get native TabAndroid from Java TabImpl";
    return tab_android;
  }

  void DestroyTab(TabAndroid* tab_android) {
    CHECK(tab_android);

    base::android::ScopedJavaLocalRef<jobject> java_tab =
        tab_android->GetJavaObject();
    CHECK(!java_tab.is_null()) << "Java tab is null";

    // Call the destroy() method on the Java TabImpl object.
    // This will trigger TabAndroid::Destroy() via JNI.
    auto tab_impl_class =
        jni_zero::AdoptRef(env_, env_->GetObjectClass(java_tab.obj()));
    CHECK(!tab_impl_class.is_null()) << "Failed to get TabImpl class";

    jmethodID destroy_method =
        env_->GetMethodID(tab_impl_class.obj(), "destroy", "()I");
    CHECK(destroy_method) << "Failed to find TabImpl.destroy() method";

    env_->CallIntMethod(java_tab.obj(), destroy_method);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  raw_ptr<JNIEnv> env_ = nullptr;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<TestingProfile> profile_;
  int next_tab_id_ = kTabId;

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

  // Arrange:
  tabs::TabStripCollection tab_strip_collection;

  // Act: Pin the tab.
  tab_strip_collection.AddTabRecursive(tabs::ScopedTab(tab_android_),
                                       /*index=*/0,
                                       /*new_group_id=*/std::nullopt,
                                       /*new_pinned_state=*/true);

  // Assert:
  EXPECT_TRUE(tab_android_->IsPinned());

  // Act: Remove the pinned tab.
  tabs::ScopedTab removed_tab = tab_strip_collection.RemoveTab(tab_android_);

  // Assert:
  // Cached pinned state is retained after removal for undoable closures.
  EXPECT_EQ(removed_tab.get(), tab_android_);
  EXPECT_TRUE(tab_android_->IsPinned());
}

TEST_F(TabAndroidTest, TabGroupTabCollectionParent) {
  EXPECT_FALSE(tab_android_->GetGroup());

  // Arrange:
  tabs::TabStripCollection tab_strip_collection;
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data;
  TabGroupAndroid::Factory factory(profile_.get());

  // Act: Create a tab group.
  tab_strip_collection.CreateTabGroup(
      std::make_unique<tabs::TabGroupTabCollection>(factory, tab_group_id,
                                                    visual_data));
  tab_strip_collection.AddTabRecursive(tabs::ScopedTab(tab_android_),
                                       /*index=*/0,
                                       /*new_group_id=*/std::nullopt,
                                       /*new_pinned_state=*/false);
  tab_strip_collection.MoveTabRecursive(/*initial_index=*/0, /*final_index=*/0,
                                        tab_group_id,
                                        /*new_pinned_state=*/false);

  // Assert:
  EXPECT_EQ(tab_group_id, tab_android_->GetGroup());

  // Act: Remove the tab from the tab group.
  tabs::ScopedTab removed_tab = tab_strip_collection.RemoveTab(tab_android_);

  // Assert:
  // Cached group ID is retained after removal for undoable closures.
  EXPECT_EQ(removed_tab.get(), tab_android_);
  EXPECT_EQ(tab_group_id, tab_android_->GetGroup());
}

TEST_F(TabAndroidTest, SplitTabCollectionParent) {
  EXPECT_FALSE(tab_android_->GetSplit());
  EXPECT_FALSE(tab_android_->IsSplit());

  // Arrange:
  tabs::TabStripCollection tab_strip_collection;
  TabAndroid* second_tab_android = CreateTab();
  tab_strip_collection.AddTabRecursive(tabs::ScopedTab(tab_android_),
                                       /*index=*/0,
                                       /*new_group_id=*/std::nullopt,
                                       /*new_pinned_state=*/true);
  tab_strip_collection.AddTabRecursive(tabs::ScopedTab(second_tab_android),
                                       /*index=*/1,
                                       /*new_group_id=*/std::nullopt,
                                       /*new_pinned_state=*/true);

  // Act: Create SplitTabCollection.
  split_tabs::SplitTabId split_id = split_tabs::SplitTabId::GenerateNew();
  tab_strip_collection.CreateSplit(split_id, {tab_android_, second_tab_android},
                                   split_tabs::SplitTabVisualData());

  // Assert:
  EXPECT_TRUE(tab_android_->IsSplit());
  EXPECT_EQ(split_id, tab_android_->GetSplit());
  EXPECT_TRUE(second_tab_android->IsSplit());
  EXPECT_EQ(split_id, second_tab_android->GetSplit());

  // Act: Detaching the SplitTabCollection from the root (TabStripCollection).
  // This must not wipe cached pinned or split properties on the child tabs.
  tabs::SplitTabCollection* split_collection =
      tab_strip_collection.GetSplitTabCollection(split_id);
  ASSERT_NE(nullptr, split_collection);
  std::unique_ptr<tabs::TabCollection> detached_split_collection =
      tab_strip_collection.RemoveTabCollection(split_collection);
  ASSERT_NE(nullptr, detached_split_collection);

  // Assert:
  EXPECT_TRUE(tab_android_->IsPinned());
  EXPECT_TRUE(tab_android_->IsSplit());
  EXPECT_EQ(split_id, tab_android_->GetSplit());
  EXPECT_TRUE(second_tab_android->IsPinned());
  EXPECT_TRUE(second_tab_android->IsSplit());
  EXPECT_EQ(split_id, second_tab_android->GetSplit());

  // Cleanup:
  // After removing the tabs, we only need to destroy second_tab_android,
  // since the first tab (tab_android_) will be destroyed by TearDown().
  EXPECT_EQ(tab_android_,
            detached_split_collection->MaybeRemoveTab(tab_android_).get());
  EXPECT_EQ(
      second_tab_android,
      detached_split_collection->MaybeRemoveTab(second_tab_android).get());
  DestroyTab(second_tab_android);
}

TEST_F(TabAndroidTest, UnrootedSubCollectionDoesNotOverwriteProperties) {
  // Arrange & Act:
  std::unique_ptr<tabs::PinnedTabCollection> unrooted_pinned_collection =
      std::make_unique<tabs::PinnedTabCollection>();
  unrooted_pinned_collection->AddTab(tabs::ScopedTab(tab_android_),
                                     /*index=*/0);

  // Assert:
  // Without a TabStripCollection root, cached properties must remain unchanged.
  EXPECT_FALSE(tab_android_->IsPinned());
  EXPECT_EQ(tab_android_,
            unrooted_pinned_collection->MaybeRemoveTab(tab_android_).get());
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
      GURL("https://example.com#fragment"), web_contents->GetPrimaryMainFrame())
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

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      raw_web_contents, GURL("https://sandboxed.example.com"));
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
  service->GetSurfaceRegistry().OnTabCreated(*tab);
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

namespace {

class PdfTestDownloadManagerDelegate : public content::DownloadManagerDelegate {
 public:
  bool ShouldOpenPdfInline() override { return true; }
};

class PdfTestingProfile : public TestingProfile {
 public:
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override {
    return &download_manager_delegate_;
  }

 private:
  PdfTestDownloadManagerDelegate download_manager_delegate_;
};

class FileCapturingDelegate : public android::TabWebContentsDelegateAndroid {
 public:
  using TabWebContentsDelegateAndroid::TabWebContentsDelegateAndroid;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override {
    listener_ = std::move(listener);
  }

  scoped_refptr<content::FileSelectListener> TakeListener() {
    return std::move(listener_);
  }

 private:
  scoped_refptr<content::FileSelectListener> listener_;
};

}  // namespace

class TabAndroidOpenFileTest : public TabAndroidTest {
 protected:
  // Triggers the open file flow, selects `file_path`, and returns the URL of
  // the resulting visible navigation entry.
  GURL OpenFileAndGetLoadedUrl(Profile* profile,
                               const base::FilePath& file_path) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile));
    FileCapturingDelegate delegate(
        env_, base::android::ScopedJavaLocalRef<jobject>(), web_contents.get());
    web_contents->SetDelegate(&delegate);

    Java_TabAndroidTestHelper_openFile(env_, web_contents.get());
    scoped_refptr<content::FileSelectListener> listener =
        delegate.TakeListener();
    EXPECT_NE(nullptr, listener);
    if (!listener) {
      return GURL();
    }

    std::vector<blink::mojom::FileChooserFileInfoPtr> files;
    files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file_path,
                                          file_path.BaseName().AsUTF16Unsafe(),
                                          std::vector<std::u16string>())));
    listener->FileSelected(std::move(files), base::FilePath(),
                           blink::mojom::FileChooserParams::Mode::kOpen);
    // The MIME type is resolved asynchronously on the thread pool, so drain any
    // posted tasks and check the resulting visible NavigationEntry. A new
    // WebContents already has an initial NavigationEntry, so only return the
    // URL if a non-initial entry was created.
    task_environment_.RunUntilIdle();

    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetVisibleEntry();
    GURL url = (entry && !entry->IsInitialEntry()) ? entry->GetURL() : GURL();
    web_contents->SetDelegate(nullptr);
    return url;
  }
};

TEST_F(TabAndroidOpenFileTest, EncodesPdfFileWhenInlineEnabled) {
  PdfTestingProfile pdf_profile;
  base::HistogramTester histogram_tester;
  EXPECT_EQ(GURL("chrome-native://pdf/"
                 "link?url=file%3A%2F%2F%2Fsdcard%2FDownload%2Fsample.pdf"),
            OpenFileAndGetLoadedUrl(
                &pdf_profile, base::FilePath("/sdcard/Download/sample.pdf")));
  histogram_tester.ExpectUniqueSample("Android.Pdf.DownloadUrlEncoded", true,
                                      1);
}

TEST_F(TabAndroidOpenFileTest, LoadsRawPdfFileUrlWhenInlineDisabled) {
  // TestingProfile has no DownloadManagerDelegate, so inline PDF is disabled.
  TestingProfile profile;
  base::HistogramTester histogram_tester;
  EXPECT_EQ(GURL("file:///sdcard/Download/sample.pdf"),
            OpenFileAndGetLoadedUrl(
                &profile, base::FilePath("/sdcard/Download/sample.pdf")));
  histogram_tester.ExpectTotalCount("Android.Pdf.DownloadUrlEncoded", 0);
}

TEST_F(TabAndroidOpenFileTest, LoadsRawUrlForNonPdfFile) {
  PdfTestingProfile pdf_profile;
  base::HistogramTester histogram_tester;
  EXPECT_EQ(GURL("file:///sdcard/Download/sample.txt"),
            OpenFileAndGetLoadedUrl(
                &pdf_profile, base::FilePath("/sdcard/Download/sample.txt")));
  histogram_tester.ExpectTotalCount("Android.Pdf.DownloadUrlEncoded", 0);
}

TEST_F(TabAndroidOpenFileTest, LoadsContentUri) {
  TestingProfile profile;
  EXPECT_EQ(GURL("content://media/external/file/123"),
            OpenFileAndGetLoadedUrl(
                &profile, base::FilePath("content://media/external/file/123")));
}

TEST_F(TabAndroidOpenFileTest, IgnoresNonFileAndNonContentPaths) {
  PdfTestingProfile pdf_profile;
  EXPECT_EQ(GURL(), OpenFileAndGetLoadedUrl(
                        &pdf_profile, base::FilePath("relative/sample.txt")));
  EXPECT_EQ(GURL(),
            OpenFileAndGetLoadedUrl(
                &pdf_profile, base::FilePath("https://example.com/file.txt")));
  EXPECT_EQ(GURL(), OpenFileAndGetLoadedUrl(&pdf_profile,
                                            base::FilePath("about:blank")));
  EXPECT_EQ(GURL(),
            OpenFileAndGetLoadedUrl(
                &pdf_profile, base::FilePath("content://host:invalid/file")));
  EXPECT_EQ(GURL(), OpenFileAndGetLoadedUrl(
                        &pdf_profile,
                        base::FilePath("content:/media/external/file/123")));
}

DEFINE_JNI(TabAndroidTestHelper)
