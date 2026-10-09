// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/voice_focused_contents_tracker_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

// Records every active tab change reported by a VoiceFocusedContentsTracker.
// The recorded pointers are only compared, never dereferenced, since the
// WebContents they point to may since have been destroyed.
class RecordingObserver : public VoiceFocusedContentsTracker::Observer {
 public:
  explicit RecordingObserver(VoiceFocusedContentsTracker& tracker) {
    observation_.Observe(&tracker);
  }

  void OnVoiceFocusedContentsChanged(
      content::WebContents* web_contents) override {
    changes_.push_back(web_contents);
  }

  // Returns the changes recorded since the last call, and clears them.
  std::vector<content::WebContents*> TakeChanges() {
    return std::exchange(changes_, {});
  }

 private:
  std::vector<content::WebContents*> changes_;
  base::ScopedObservation<VoiceFocusedContentsTracker,
                          VoiceFocusedContentsTracker::Observer>
      observation_{this};
};

// Extends OwningTestTabModel to emit TabModelObserver notifications that
// OwningTestTabModel does not natively fire (active-state transitions, pending
// undoable tab closures, and deferred WebContents initialization). Because
// OwningTestTabModel::observer_list_ is private and
// OwningTestTabModel::SelectTab dereferences TabAndroid::web_contents()
// unconditionally, FakeTabModel keeps its own observer list and unloaded-tab
// state.
class FakeTabModel : public OwningTestTabModel {
 public:
  explicit FakeTabModel(Profile* profile)
      : FakeTabModel(profile,
                     chrome::android::ActivityType::kTabbed,
                     TabModelType::kStandard) {}

  FakeTabModel(Profile* profile,
               chrome::android::ActivityType activity_type,
               TabModelType tab_model_type)
      : OwningTestTabModel(profile, activity_type, tab_model_type) {
    // Re-register with TabModelList now that the derived vtable is active so
    // any TabModelListObserver already listening calls FakeTabModel's
    // AddObserver override.
    TabModelList::RemoveTabModel(this);
    TabModelList::AddTabModel(this);
  }

  ~FakeTabModel() override = default;

  // OwningTestTabModel:
  void AddObserver(TabModelObserver* observer) override {
    OwningTestTabModel::AddObserver(observer);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(TabModelObserver* observer) override {
    observers_.RemoveObserver(observer);
    OwningTestTabModel::RemoveObserver(observer);
  }

  bool HasTab(TabAndroid* tab) const override {
    if (tab && deferred_tab_.get() == tab) {
      return true;
    }
    return OwningTestTabModel::HasTab(tab);
  }

  int GetActiveIndex() const override {
    if (all_tabs_pending_closure_) {
      return kInvalidIndex;
    }
    if (deferred_tab_) {
      return 0;
    }
    return OwningTestTabModel::GetActiveIndex();
  }

  tabs::TabInterface* GetActiveTab() override {
    if (all_tabs_pending_closure_) {
      return nullptr;
    }
    if (deferred_tab_) {
      return deferred_tab_.get();
    }
    return OwningTestTabModel::GetActiveTab();
  }

  content::WebContents* GetWebContentsAt(int index) const override {
    if (all_tabs_pending_closure_) {
      return nullptr;
    }
    if (deferred_tab_) {
      return index == 0 ? deferred_web_contents_.get() : nullptr;
    }
    return OwningTestTabModel::GetWebContentsAt(index);
  }

  void ActivateModel(bool active) {
    observers_.Notify(&TabModelObserver::OnWillActiveStateChange,
                      std::ref(*this), active);
    SetIsActiveModel(active);
    observers_.Notify(&TabModelObserver::OnDidActiveStateChange,
                      std::ref(*this), active);
  }

  TabAndroid* AddWebContentsTab(Profile* profile) {
    return AddWebContentsTab(profile, /*select=*/true);
  }

  TabAndroid* AddWebContentsTab(Profile* profile, bool select) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);
    return AddTabFromWebContents(std::move(web_contents), GetTabCount(),
                                 select);
  }

  void NotifyDidSelectTab(TabAndroid* tab) {
    observers_.Notify(&TabModelObserver::DidSelectTab, tab);
  }

  // Simulates an undoable closure of all tabs in the model on Clank
  // (TabCollectionTabModelImpl.closeTabsInternal), which removes the tab from
  // the model and fires DidRemoveTabForClosure followed by OnTabClosePending
  // without calling DidSelectTab(nullptr).
  void MarkAllTabsPendingClosure(const std::vector<TabAndroid*>& tabs) {
    all_tabs_pending_closure_ = true;
    for (TabAndroid* tab : tabs) {
      observers_.Notify(&TabModelObserver::DidRemoveTabForClosure, tab);
    }
    observers_.Notify(&TabModelObserver::OnTabClosePending, tabs);
  }

  void UndoSingleTabClosure(TabAndroid* tab) {
    all_tabs_pending_closure_ = false;
    observers_.Notify(&TabModelObserver::TabClosureUndone, tab);
  }

  void UndoMultiTabClosure(const std::vector<TabAndroid*>& tabs) {
    all_tabs_pending_closure_ = false;
    observers_.Notify(&TabModelObserver::OnTabCloseUndone, tabs);
  }

  void CommitPendingTabClosure(TabAndroid* tab) {
    observers_.Notify(&TabModelObserver::TabClosureCommitted, tab);
  }

  // Simulates selecting an unloaded/frozen tab whose WebContents is not yet
  // initialized, followed by deferred initialization.
  TabAndroid* SelectUnloadedTab(Profile* profile) {
    deferred_web_contents_.reset();
    deferred_tab_ = TabAndroid::CreateForTesting(profile, /*tab_id=*/999,
                                                 /*web_contents=*/nullptr);
    observers_.Notify(&TabModelObserver::DidSelectTab, deferred_tab_.get());
    return deferred_tab_.get();
  }

  content::WebContents* InitializeDeferredWebContents(Profile* profile) {
    deferred_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);
    return deferred_web_contents_.get();
  }

  void ClearDeferredTabWithRemoval() {
    TabAndroid* raw_tab = deferred_tab_.get();
    deferred_web_contents_.reset();
    std::unique_ptr<TabAndroid> dying_tab = std::move(deferred_tab_);
    observers_.Notify(&TabModelObserver::DidRemoveTabForClosure, raw_tab);
  }

  // Simulates TabCollectionTabModelImpl.destroy(), which notifies WillDetach
  // with DetachReason::kDelete and destroys the TabAndroid before firing
  // DidRemoveTabForClosure or OnTabModelDestroyed, including when another
  // TabModelObserver notification triggers UpdateActiveState() before the tab
  // is removed from GetActiveTab().
  void DestroyDeferredTabBeforeModelTeardown() {
    ASSERT_TRUE(deferred_tab_);
    deferred_tab_->SendWillDetachUpdate(
        nullptr,
        static_cast<int32_t>(tabs::TabInterface::DetachReason::kDelete));
    observers_.Notify(&TabModelObserver::OnTabClosePending,
                      std::vector<TabAndroid*>{});
    deferred_web_contents_.reset();
    deferred_tab_.reset();
  }

  // Simulates TabImpl.updateAttachment() detaching the unloaded active tab for
  // reparenting into another window, which sends WillDetach with
  // DetachReason::kInsertIntoOtherWindow and never sends kDelete if the tab is
  // destroyed before being re-inserted.
  void DetachDeferredTabForReparenting() {
    ASSERT_TRUE(deferred_tab_);
    deferred_tab_->SendWillDetachUpdate(
        nullptr, static_cast<int32_t>(
                     tabs::TabInterface::DetachReason::kInsertIntoOtherWindow));
  }

  // Destroys a detached deferred tab without any further WillDetach or
  // TabModelObserver notification, as happens when TabImpl.destroy() runs on a
  // tab whose mCurrentTabSupplier was already cleared by updateAttachment().
  void DestroyDetachedDeferredTab() {
    ASSERT_TRUE(deferred_tab_);
    deferred_web_contents_.reset();
    deferred_tab_.reset();
  }

  void NotifyTabClosePendingForNoTabs() {
    observers_.Notify(&TabModelObserver::OnTabClosePending,
                      std::vector<TabAndroid*>{});
  }

 private:
  bool all_tabs_pending_closure_ = false;
  std::unique_ptr<TabAndroid> deferred_tab_;
  std::unique_ptr<content::WebContents> deferred_web_contents_;
  base::ObserverList<TabModelObserver>::Unchecked observers_;
};

class VoiceFocusedContentsTrackerAndroidTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfile profile_;
};

// When created with an active TabModel that already has an active tab, the
// tracker immediately returns its WebContents and resolves its TabInterface.
TEST_F(VoiceFocusedContentsTrackerAndroidTest, ActiveTabAtCreation) {
  FakeTabModel model(&profile_);
  model.SetIsActiveModel(true);
  TabAndroid* tab = model.AddWebContentsTab(&profile_);

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  EXPECT_EQ(tracker.GetActiveWebContents(), tab->web_contents());
  EXPECT_EQ(
      tabs::TabInterface::MaybeGetFromContents(tracker.GetActiveWebContents()),
      tab);
}

// When no TabModel of the profile is active, GetActiveWebContents() is null.
TEST_F(VoiceFocusedContentsTrackerAndroidTest, NullWhenNoActiveTabModel) {
  FakeTabModel inactive_model(&profile_);
  inactive_model.AddWebContentsTab(&profile_);

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
}

// Switching the active tab in the bound TabModel updates GetActiveWebContents()
// and notifies observers only when the active WebContents changes.
TEST_F(VoiceFocusedContentsTrackerAndroidTest,
       SwitchingTabsNotifiesAndUpdatesActiveWebContents) {
  FakeTabModel model(&profile_);
  model.SetIsActiveModel(true);
  TabAndroid* tab0 = model.AddWebContentsTab(&profile_, /*select=*/true);
  TabAndroid* tab1 = model.AddWebContentsTab(&profile_, /*select=*/false);
  ASSERT_EQ(model.GetActiveWebContents(), tab0->web_contents());

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);
  ASSERT_EQ(tracker.GetActiveWebContents(), tab0->web_contents());

  model.SetActiveIndex(1);
  EXPECT_EQ(tracker.GetActiveWebContents(), tab1->web_contents());
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(tab1->web_contents()));

  // Redundant DidSelectTab for the same active tab must not re-notify.
  model.NotifyDidSelectTab(tab1);
  EXPECT_EQ(tracker.GetActiveWebContents(), tab1->web_contents());
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());

  model.SetActiveIndex(0);
  EXPECT_EQ(tracker.GetActiveWebContents(), tab0->web_contents());
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(tab0->web_contents()));
}

// Switching active state between two TabModels of the same profile rebinds the
// tracker to the newly active model's active WebContents, and ignores tab
// selections in the inactive model.
TEST_F(VoiceFocusedContentsTrackerAndroidTest,
       SwitchingTabModelsFollowsActiveModel) {
  FakeTabModel model1(&profile_);
  model1.SetIsActiveModel(true);
  TabAndroid* model1_tab0 =
      model1.AddWebContentsTab(&profile_, /*select=*/true);
  TabAndroid* model1_tab1 =
      model1.AddWebContentsTab(&profile_, /*select=*/false);

  FakeTabModel model2(&profile_);
  TabAndroid* model2_tab0 =
      model2.AddWebContentsTab(&profile_, /*select=*/true);

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);
  ASSERT_EQ(tracker.GetActiveWebContents(), model1_tab0->web_contents());

  // Transition active state from model1 to model2.
  model1.SetIsActiveModel(false);
  model2.ActivateModel(true);
  EXPECT_EQ(tracker.GetActiveWebContents(), model2_tab0->web_contents());
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(model2_tab0->web_contents()));

  // Selecting a tab in the inactive model1 must not change the tracked
  // WebContents or emit a notification.
  model1.SetActiveIndex(1);
  EXPECT_EQ(model1.GetActiveWebContents(), model1_tab1->web_contents());
  EXPECT_EQ(tracker.GetActiveWebContents(), model2_tab0->web_contents());
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());

  // Switching back to model1 binds to model1's now-active tab1.
  model2.SetIsActiveModel(false);
  model1.ActivateModel(true);
  EXPECT_EQ(tracker.GetActiveWebContents(), model1_tab1->web_contents());
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(model1_tab1->web_contents()));

  // Deactivating model1 when no other model is active clears the tracked
  // WebContents and notifies once with nullptr.
  model1.ActivateModel(false);
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::ElementsAre(nullptr));
}

// Covers TabModelSelectorBase.initialize() (which sets IsActiveModel(true)
// after OnTabModelAdded without firing OnDidActiveStateChange) and multi-window
// regular TabModels (where multiple models have IsActiveModel() == true and
// selecting a tab in another window rebinds the tracker).
TEST_F(VoiceFocusedContentsTrackerAndroidTest,
       DidSelectTabBindsUnboundAndMultiWindowActiveModels) {
  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);
  ASSERT_EQ(tracker.GetActiveWebContents(), nullptr);

  // Model is added while inactive (as in TabModelSelectorImpl), then marked
  // active without OnDidActiveStateChange before its first tab is selected.
  FakeTabModel window1_model(&profile_);
  window1_model.SetIsActiveModel(true);
  TabAndroid* window1_tab0 =
      window1_model.AddWebContentsTab(&profile_, /*select=*/true);
  EXPECT_EQ(tracker.GetActiveWebContents(), window1_tab0->web_contents());
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(window1_tab0->web_contents()));

  // A second regular window's TabModel also has IsActiveModel() == true.
  // Adding a background tab to window2_model does not steal focus from
  // window1_model, but selecting a tab in window2_model rebinds the tracker.
  FakeTabModel window2_model(&profile_);
  window2_model.SetIsActiveModel(true);
  TabAndroid* window2_tab0 =
      window2_model.AddWebContentsTab(&profile_, /*select=*/true);
  EXPECT_EQ(tracker.GetActiveWebContents(), window2_tab0->web_contents());
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(window2_tab0->web_contents()));

  // Adding a background tab to window1_model does not rebind away from
  // window2_model.
  TabAndroid* window1_tab1 =
      window1_model.AddWebContentsTab(&profile_, /*select=*/false);
  EXPECT_EQ(tracker.GetActiveWebContents(), window2_tab0->web_contents());
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());

  // Selecting window1_tab1 in window1_model rebinds to window1_model.
  window1_model.SetActiveIndex(1);
  EXPECT_EQ(tracker.GetActiveWebContents(), window1_tab1->web_contents());
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(window1_tab1->web_contents()));
}

// TabModels belonging to a different profile or an incognito/OTR profile are
// ignored.
TEST_F(VoiceFocusedContentsTrackerAndroidTest,
       IgnoresOtherProfileAndIncognitoTabModels) {
  FakeTabModel main_model(&profile_);
  main_model.SetIsActiveModel(true);
  TabAndroid* main_tab = main_model.AddWebContentsTab(&profile_);

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);
  ASSERT_EQ(tracker.GetActiveWebContents(), main_tab->web_contents());

  TestingProfile other_profile;
  FakeTabModel other_profile_model(&other_profile);
  other_profile_model.AddWebContentsTab(&other_profile);
  other_profile_model.ActivateModel(true);

  Profile* otr_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  FakeTabModel otr_model(otr_profile);
  otr_model.AddWebContentsTab(otr_profile);
  otr_model.ActivateModel(true);

  EXPECT_EQ(tracker.GetActiveWebContents(), main_tab->web_contents());
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());
}

// Non-standard (archived, headless) TabModels are ignored even if they report
// IsActiveModel.
TEST_F(VoiceFocusedContentsTrackerAndroidTest, IgnoresNonStandardTabModels) {
  FakeTabModel main_model(&profile_);
  main_model.SetIsActiveModel(true);
  TabAndroid* main_tab = main_model.AddWebContentsTab(&profile_);

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);
  ASSERT_EQ(tracker.GetActiveWebContents(), main_tab->web_contents());

  FakeTabModel archived_model(&profile_, chrome::android::ActivityType::kTabbed,
                              TabModel::TabModelType::kArchived);
  archived_model.ActivateModel(true);

  FakeTabModel headless_model(&profile_, chrome::android::ActivityType::kTabbed,
                              TabModel::TabModelType::kHeadless);
  headless_model.ActivateModel(true);

  EXPECT_EQ(tracker.GetActiveWebContents(), main_tab->web_contents());
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());
}

// Closing the active tab switches to the remaining tab when one exists, and
// transitions to nullptr when the last tab in the active model is closed.
TEST_F(VoiceFocusedContentsTrackerAndroidTest,
       ClosingActiveTabSwitchesToRemainingTabOrNull) {
  FakeTabModel model(&profile_);
  model.SetIsActiveModel(true);
  TabAndroid* tab0 = model.AddWebContentsTab(&profile_, /*select=*/true);
  TabAndroid* tab1 = model.AddWebContentsTab(&profile_, /*select=*/false);

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);
  ASSERT_EQ(tracker.GetActiveWebContents(), tab0->web_contents());
  content::WebContents* tab1_contents = tab1->web_contents();

  // Closing tab0 selects tab1 and notifies once with tab1's WebContents.
  model.CloseTabAt(0);
  EXPECT_EQ(tracker.GetActiveWebContents(), tab1_contents);
  EXPECT_THAT(observer.TakeChanges(), ::testing::ElementsAre(tab1_contents));

  // Closing the last remaining tab transitions to nullptr and notifies once.
  model.CloseTabAt(0);
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::ElementsAre(nullptr));
}

// On Clank, undoable closure of the last tab removes the tab from the model
// (DidRemoveTabForClosure) and marks closure pending (OnTabClosePending)
// without calling DidSelectTab(nullptr), and may later be undone
// (TabClosureUndone / OnTabCloseUndone) or committed (TabClosureCommitted).
TEST_F(VoiceFocusedContentsTrackerAndroidTest, PendingTabClosureAndUndo) {
  FakeTabModel model(&profile_);
  model.SetIsActiveModel(true);
  TabAndroid* tab = model.AddWebContentsTab(&profile_, /*select=*/true);

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);
  ASSERT_EQ(tracker.GetActiveWebContents(), tab->web_contents());

  // Pending closure of the last tab immediately clears GetActiveWebContents()
  // and emits a single nullptr notification across DidRemoveTabForClosure and
  // OnTabClosePending.
  model.MarkAllTabsPendingClosure({tab});
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::ElementsAre(nullptr));

  // Undoing single-tab closure restores the active WebContents and notifies.
  model.UndoSingleTabClosure(tab);
  EXPECT_EQ(tracker.GetActiveWebContents(), tab->web_contents());
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(tab->web_contents()));

  // Pending closure again, followed by multi-tab undo.
  model.MarkAllTabsPendingClosure({tab});
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::ElementsAre(nullptr));

  model.UndoMultiTabClosure({tab});
  EXPECT_EQ(tracker.GetActiveWebContents(), tab->web_contents());
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(tab->web_contents()));

  // Pending closure followed by TabClosureCommitted must not emit a duplicate
  // notification when the closure is committed.
  model.MarkAllTabsPendingClosure({tab});
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::ElementsAre(nullptr));

  model.CommitPendingTabClosure(tab);
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());
}

// Selecting an unloaded/frozen tab (where TabAndroid::web_contents() is
// initially null) observes the TabAndroid and updates as soon as
// OnInitWebContents fires, or cleans up observation if the tab closes or is
// destroyed during TabModel teardown first.
TEST_F(VoiceFocusedContentsTrackerAndroidTest,
       DeferredWebContentsInitializationOnTab) {
  FakeTabModel model(&profile_);
  model.SetIsActiveModel(true);

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);
  ASSERT_EQ(tracker.GetActiveWebContents(), nullptr);

  TabAndroid* unloaded_tab = model.SelectUnloadedTab(&profile_);
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());
  EXPECT_TRUE(tracker.TabAndroid::Observer::IsInObserverList());

  content::WebContents* initialized_contents =
      model.InitializeDeferredWebContents(&profile_);
  // TabAndroid::InitWebContents requires a live JNI TabImpl, so after
  // verifying `tracker` is registered in `unloaded_tab`'s observer list above,
  // invoke the TabAndroid::Observer callback directly.
  tracker.OnInitWebContents(unloaded_tab);
  EXPECT_EQ(tracker.GetActiveWebContents(), initialized_contents);
  EXPECT_THAT(observer.TakeChanges(),
              ::testing::ElementsAre(initialized_contents));
  EXPECT_FALSE(tracker.TabAndroid::Observer::IsInObserverList());

  // Selecting another unloaded tab and closing it before WebContents
  // initializes must detach the TabAndroid::Observer cleanly.
  model.SelectUnloadedTab(&profile_);
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::ElementsAre(nullptr));
  EXPECT_TRUE(tracker.TabAndroid::Observer::IsInObserverList());

  model.ClearDeferredTabWithRemoval();
  EXPECT_FALSE(tracker.TabAndroid::Observer::IsInObserverList());

  // Destroying an unloaded active tab during TabCollectionTabModelImpl.destroy
  // (which fires WillDetach(kDelete) before ~TabAndroid and before
  // OnTabModelDestroyed) must detach TabAndroid::Observer before freeing the
  // tab so subsequent model teardown does not UAF.
  model.SelectUnloadedTab(&profile_);
  EXPECT_TRUE(tracker.TabAndroid::Observer::IsInObserverList());
  model.DestroyDeferredTabBeforeModelTeardown();
  EXPECT_FALSE(tracker.TabAndroid::Observer::IsInObserverList());
}

// Detaching an unloaded active tab for reparenting (WillDetach with a reason
// other than kDelete) must drop the TabAndroid::Observer registration, since a
// detached tab that is destroyed before re-insertion never sends kDelete.
TEST_F(VoiceFocusedContentsTrackerAndroidTest,
       WillDetachForReparentDropsObservation) {
  FakeTabModel model(&profile_);
  model.SetIsActiveModel(true);

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);

  model.SelectUnloadedTab(&profile_);
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());
  EXPECT_TRUE(tracker.TabAndroid::Observer::IsInObserverList());

  model.DetachDeferredTabForReparenting();
  EXPECT_FALSE(tracker.TabAndroid::Observer::IsInObserverList());
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());

  // The detached tab is destroyed with no further notification. A later
  // UpdateActiveState() trigger must not touch the freed TabAndroid.
  model.DestroyDetachedDeferredTab();
  model.NotifyTabClosePendingForNoTabs();
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_FALSE(tracker.TabAndroid::Observer::IsInObserverList());
  EXPECT_THAT(observer.TakeChanges(), ::testing::IsEmpty());
}

// Removing or destroying the bound TabModel falls back to another active
// TabModel of the profile if one exists, or to nullptr otherwise.
TEST_F(VoiceFocusedContentsTrackerAndroidTest,
       RemovingOrDestroyingActiveTabModelFallsBackOrNull) {
  auto model1 = std::make_unique<FakeTabModel>(&profile_);
  model1->SetIsActiveModel(true);
  TabAndroid* tab1 = model1->AddWebContentsTab(&profile_);

  auto model2 = std::make_unique<FakeTabModel>(&profile_);
  TabAndroid* tab2 = model2->AddWebContentsTab(&profile_);
  content::WebContents* tab2_contents = tab2->web_contents();

  VoiceFocusedContentsTrackerAndroid tracker(profile_);
  RecordingObserver observer(tracker);
  ASSERT_EQ(tracker.GetActiveWebContents(), tab1->web_contents());

  // Mark model2 active and destroy model1; tracker falls back to model2.
  model2->SetIsActiveModel(true);
  model1.reset();
  EXPECT_EQ(tracker.GetActiveWebContents(), tab2_contents);
  EXPECT_THAT(observer.TakeChanges(), ::testing::ElementsAre(tab2_contents));

  // Destroying the last active model transitions to nullptr.
  model2.reset();
  EXPECT_EQ(tracker.GetActiveWebContents(), nullptr);
  EXPECT_THAT(observer.TakeChanges(), ::testing::ElementsAre(nullptr));
}

}  // namespace
}  // namespace ttc
