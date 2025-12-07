// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.util.Pair;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Manual;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Random;

/**
 * A deterministic simulation test for the {@link TabCollectionTabModelImpl}. At present, this
 * focuses solely on the regular tab model and the {@link TabCollectionTabModelImpl} public API
 * surface.
 *
 * <p>This test is intended to be run manually to figure out possible sequences of inputs that could
 * result in crashes or unrecoverable states.
 *
 * <p>This is not as "deterministic" or "simulated" as possible as the test is run in a real Chrome
 * instance on a device. As such, things like clocks, and threading/scheduling are not simulated. It
 * is likely snackbars will close mid test resulting in tab closures and group operations getting
 * "committed" mid-test. This also does not test multi-window.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class TabsDeterministicSimulationTest {
    private static final String TAG = "TDST";

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private final Random mGlobalRandom = new Random();
    private TabCollectionTabModelImpl mCollectionModel;

    private class WeightedRandomRunnable implements Runnable {
        private final List<Pair<Integer, Runnable>> mWeightedRunnables;
        private final int mTotalWeight;

        WeightedRandomRunnable(List<Pair<Integer, Runnable>> weightedRunnables) {
            mWeightedRunnables = weightedRunnables;
            int totalWeight = 0;
            for (Pair<Integer, Runnable> pair : mWeightedRunnables) {
                totalWeight += pair.first;
            }
            mTotalWeight = totalWeight;
        }

        @Override
        public void run() {
            if (mTotalWeight == 0) return;
            int randomValue = mGlobalRandom.nextInt(mTotalWeight);
            int currentWeight = 0;
            for (Pair<Integer, Runnable> pair : mWeightedRunnables) {
                currentWeight += pair.first;
                if (randomValue < currentWeight) {
                    pair.second.run();
                    return;
                }
            }
        }
    }

    @Before
    public void setUp() throws Exception {
        // Start on the tab switcher this should prevent any automatic exit logic from triggering.
        var page = mActivityTestRule.startOnBlankPage();
        page.openRegularTabSwitcher();

        TabModelSelector tabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
        TabModel regularModel = tabModelSelector.getModel(/* incognito= */ false);
        mCollectionModel = (TabCollectionTabModelImpl) regularModel;
    }

    // The weights are somewhat arbitrary with the intent to largely favor mutation, but when an add
    // or remove is done they happen in roughly equal proportion.
    private final WeightedRandomRunnable mAdd =
            new WeightedRandomRunnable(
                    List.of(
                            Pair.create(6, this::addTab),
                            Pair.create(2, this::cancelClosure),
                            Pair.create(1, this::openMostRecentlyClosedEntry)));
    private final WeightedRandomRunnable mMutate =
            new WeightedRandomRunnable(
                    List.of(
                            Pair.create(1, this::setIndex),
                            Pair.create(1, this::moveTabOrGroup),
                            Pair.create(1, this::pinOrUnpin),
                            Pair.create(1, this::createOrAddTabToGroup),
                            Pair.create(1, this::ungroupTabsOrGroup),
                            Pair.create(1, this::updateGroupVisualData)));
    private final WeightedRandomRunnable mRemove =
            new WeightedRandomRunnable(
                    List.of(
                            Pair.create(6, this::closeTabs),
                            Pair.create(2, this::commitClosure),
                            Pair.create(1, this::removeTab)));
    private final WeightedRandomRunnable mRootRunnable =
            new WeightedRandomRunnable(
                    List.of(
                            Pair.create(1, mAdd),
                            Pair.create(4, mMutate),
                            Pair.create(1, mRemove)));

    @Test
    @Manual // pass `-A Manual` to the test runner to include this test.
    public void runSimulation() {
        final int stepCount = 100000;
        long seed = mGlobalRandom.nextLong();
        Log.i(TAG, "Seed: " + seed);
        mGlobalRandom.setSeed(seed);
        for (int i = 0; i < stepCount; i++) {
            // A large number of steps end up no-oping due to invalid state. An improvement would
            // be to increase the weight of `mAdd` if there are no or very few tabs and reduce its
            // weight if there are several tabs. However, dynamic weights would possibly introduce
            // additional complexity and avoid the search space for no tabs.
            mRootRunnable.run();
        }
    }

    private void addTab() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Log.i(TAG, "addTab: launchUrl(\"about:blank\")");
                    mCollectionModel
                            .getTabCreator(false)
                            .launchUrl("about:blank", TabLaunchType.FROM_CHROME_UI);
                });
    }

    private void cancelClosure() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (!mCollectionModel.supportsPendingClosures()) return;

                    List<Tab> pendingTabs = new ArrayList<>();
                    for (Tab tab : mCollectionModel.getComprehensiveModel()) {
                        if (mCollectionModel.isClosurePending(tab.getId())) {
                            pendingTabs.add(tab);
                        }
                    }

                    if (pendingTabs.isEmpty() && !mGlobalRandom.nextBoolean()) return;

                    int tabId;
                    if (!pendingTabs.isEmpty() && mGlobalRandom.nextBoolean()) {
                        tabId = pendingTabs.get(mGlobalRandom.nextInt(pendingTabs.size())).getId();
                    } else {
                        tabId = Tab.INVALID_TAB_ID;
                    }
                    Log.i(TAG, "cancelClosure: cancelTabClosure(" + tabId + ")");
                    mCollectionModel.cancelTabClosure(tabId);
                });
    }

    private void openMostRecentlyClosedEntry() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Log.i(TAG, "openMostRecentlyClosedEntry");
                    mCollectionModel.openMostRecentlyClosedEntry();
                });
    }

    private void setIndex() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int count = mCollectionModel.getCount();
                    if (count == 0) return;

                    // Pick a random valid index or an invalid one.
                    int index = mGlobalRandom.nextInt(count + 2) - 1; // Range [-1, count]
                    Log.i(TAG, "setIndex: setIndex(" + index + ")");
                    mCollectionModel.setIndex(index, TabSelectionType.FROM_USER);
                });
    }

    private void moveTabOrGroup() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int count = mCollectionModel.getCount();
                    if (count == 0 && !mGlobalRandom.nextBoolean()) return;

                    int tabId;
                    if (count > 0 && mGlobalRandom.nextBoolean()) {
                        tabId = mCollectionModel.getTabAt(mGlobalRandom.nextInt(count)).getId();
                    } else {
                        tabId = Tab.INVALID_TAB_ID;
                    }

                    int newIndex = count > 0 ? mGlobalRandom.nextInt(count) : 0;
                    Log.i(TAG, "moveTabOrGroup: moveRelatedTabs(" + tabId + ", " + newIndex + ")");
                    mCollectionModel.moveRelatedTabs(tabId, newIndex);
                });
    }

    private void pinOrUnpin() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int count = mCollectionModel.getCount();
                    if (count == 0 && !mGlobalRandom.nextBoolean()) return;

                    int tabId;
                    boolean isPinned = false;
                    if (count > 0 && mGlobalRandom.nextBoolean()) {
                        Tab tab = mCollectionModel.getTabAt(mGlobalRandom.nextInt(count));
                        tabId = tab.getId();
                        isPinned = tab.getIsPinned();
                    } else {
                        tabId = Tab.INVALID_TAB_ID;
                    }

                    if (isPinned) {
                        Log.i(TAG, "pinOrUnpin: unpinTab(" + tabId + ")");
                        mCollectionModel.unpinTab(tabId);
                    } else {
                        // This may ungroup the tab if it is in a group.
                        Log.i(TAG, "pinOrUnpin: pinTab(" + tabId + ")");
                        mCollectionModel.pinTab(tabId, false, null);
                    }
                });
    }

    private void createOrAddTabToGroup() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int count = mCollectionModel.getCount();
                    if (count < 2 && mGlobalRandom.nextBoolean()) {
                        // Not enough tabs to do anything meaningful, so 50% of the time try an
                        // invalid operation.
                        mCollectionModel.mergeTabsToGroup(Tab.INVALID_TAB_ID, Tab.INVALID_TAB_ID);
                        return;
                    }
                    if (count < 2) return;

                    Tab sourceTab = mCollectionModel.getTabAt(mGlobalRandom.nextInt(count));
                    Tab destinationTab;
                    do {
                        destinationTab = mCollectionModel.getTabAt(mGlobalRandom.nextInt(count));
                    } while (sourceTab == destinationTab);

                    int sourceTabId = sourceTab.getId();
                    int destinationTabId = destinationTab.getId();

                    int choice = mGlobalRandom.nextInt(4);
                    if (choice == 1) {
                        sourceTabId = Tab.INVALID_TAB_ID;
                    } else if (choice == 2) {
                        destinationTabId = Tab.INVALID_TAB_ID;
                    } else if (choice == 3) {
                        sourceTabId = Tab.INVALID_TAB_ID;
                        destinationTabId = Tab.INVALID_TAB_ID;
                    }

                    Log.i(
                            TAG,
                            "createOrAddTabToGroup: mergeTabsToGroup("
                                    + sourceTabId
                                    + ", "
                                    + destinationTabId
                                    + ")");
                    mCollectionModel.mergeTabsToGroup(sourceTabId, destinationTabId);
                });
    }

    private void ungroupTabsOrGroup() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabUngrouper ungrouper = mCollectionModel.getTabUngrouper();
                    List<Token> groupIds = new ArrayList<>(mCollectionModel.getAllTabGroupIds());
                    if (groupIds.isEmpty() && !mGlobalRandom.nextBoolean()) return;

                    Token groupId;
                    if (!groupIds.isEmpty() && mGlobalRandom.nextBoolean()) {
                        groupId = groupIds.get(mGlobalRandom.nextInt(groupIds.size()));
                    } else {
                        groupId = Token.createRandom(); // Invalid group ID
                    }
                    List<Tab> tabsInGroup = mCollectionModel.getTabsInGroup(groupId);

                    if (tabsInGroup.isEmpty()) return;

                    if (mGlobalRandom.nextBoolean() && tabsInGroup.size() > 1) {
                        // Ungroup a random subset of tabs.
                        List<Tab> mutableTabsInGroup = new ArrayList<>(tabsInGroup);
                        Collections.shuffle(mutableTabsInGroup, mGlobalRandom);
                        // Size of subset is between 1 and size - 1.
                        int subsetSize = mGlobalRandom.nextInt(mutableTabsInGroup.size() - 1) + 1;
                        List<Tab> tabsToUngroup =
                                new ArrayList<>(mutableTabsInGroup.subList(0, subsetSize));
                        Log.i(TAG, "ungroupTabsOrGroup: ungroupTabs(" + tabsToUngroup + ")");
                        ungrouper.ungroupTabs(
                                tabsToUngroup,
                                mGlobalRandom.nextBoolean(),
                                /* allowDialog= */ false);
                    } else {
                        // Ungroup the whole group.
                        Log.i(TAG, "ungroupTabsOrGroup: ungroupTabGroup(" + groupId + ")");
                        ungrouper.ungroupTabGroup(
                                groupId, mGlobalRandom.nextBoolean(), /* allowDialog= */ false);
                    }
                });
    }

    private void updateGroupVisualData() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<Token> groupIds = new ArrayList<>(mCollectionModel.getAllTabGroupIds());
                    // TabCollectionTabModelImpl asserts or crashes if one of these operations
                    // happens on an invalid group ID.
                    if (groupIds.isEmpty()) return;

                    Token groupId = groupIds.get(mGlobalRandom.nextInt(groupIds.size()));

                    int choice = mGlobalRandom.nextInt(3);
                    if (choice == 0) {
                        String title = "Title " + mGlobalRandom.nextInt();
                        Log.i(
                                TAG,
                                "updateGroupVisualData: setTabGroupTitle("
                                        + groupId
                                        + ", "
                                        + title
                                        + ")");
                        mCollectionModel.setTabGroupTitle(groupId, title);
                    } else if (choice == 1) {
                        int color = mGlobalRandom.nextInt(TabGroupColorId.NUM_ENTRIES);
                        Log.i(
                                TAG,
                                "updateGroupVisualData: setTabGroupColor("
                                        + groupId
                                        + ", "
                                        + color
                                        + ")");
                        mCollectionModel.setTabGroupColor(groupId, color);
                    } else {
                        boolean collapsed = !mCollectionModel.getTabGroupCollapsed(groupId);
                        Log.i(
                                TAG,
                                "updateGroupVisualData: setTabGroupCollapsed("
                                        + groupId
                                        + ", "
                                        + collapsed
                                        + ")");
                        mCollectionModel.setTabGroupCollapsed(groupId, collapsed, false);
                    }
                });
    }

    private void closeTabs() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabRemover remover = mCollectionModel.getTabRemover();
                    int count = mCollectionModel.getCount();
                    if (count == 0) return;

                    int choice = mGlobalRandom.nextInt(4);
                    if (choice == 0) {
                        // Close single tab
                        Tab tabToClose = mCollectionModel.getTabAt(mGlobalRandom.nextInt(count));
                        Log.i(TAG, "closeTabs: closeTab(" + tabToClose.getId() + ")");
                        remover.closeTabs(TabClosureParams.closeTab(tabToClose).build(), false);
                    } else if (choice == 1 && count >= 2) {
                        // Close a random subset of tabs.
                        List<Tab> allTabs = new ArrayList<>(mCollectionModel.getAllTabs());
                        Collections.shuffle(allTabs, mGlobalRandom);
                        // Size of subset is between 2 and count.
                        int subsetSize = mGlobalRandom.nextInt(count - 1) + 2;
                        List<Tab> tabsToClose = allTabs.subList(0, subsetSize);
                        Log.i(TAG, "closeTabs: closeTabs(" + tabsToClose + ")");
                        remover.closeTabs(TabClosureParams.closeTabs(tabsToClose).build(), false);
                    } else if (choice == 2 && mCollectionModel.getTabGroupCount() > 0) {
                        // Close a group
                        List<Token> groupIds =
                                new ArrayList<>(mCollectionModel.getAllTabGroupIds());
                        Token groupId = groupIds.get(mGlobalRandom.nextInt(groupIds.size()));
                        Log.i(TAG, "closeTabs: closeTabGroup(" + groupId + ")");
                        var params = TabClosureParams.forCloseTabGroup(mCollectionModel, groupId);
                        if (params != null) {
                            remover.closeTabs(params.build(), false);
                        }
                    } else if (choice == 3) {
                        // Close all tabs
                        Log.i(TAG, "closeTabs: closeAllTabs()");
                        remover.closeTabs(TabClosureParams.closeAllTabs().build(), false);
                    }
                });
    }

    private void commitClosure() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (!mCollectionModel.supportsPendingClosures()) return;

                    List<Tab> pendingTabs = new ArrayList<>();
                    for (Tab tab : mCollectionModel.getComprehensiveModel()) {
                        if (mCollectionModel.isClosurePending(tab.getId())) {
                            pendingTabs.add(tab);
                        }
                    }

                    if (pendingTabs.isEmpty() && !mGlobalRandom.nextBoolean()) return;

                    int choice = mGlobalRandom.nextInt(3);
                    if (choice == 0) {
                        Log.i(TAG, "commitClosure: commitAllTabClosures()");
                        mCollectionModel.commitAllTabClosures();
                    } else if (choice == 1 && !pendingTabs.isEmpty()) {
                        Tab tabToCommit =
                                pendingTabs.get(mGlobalRandom.nextInt(pendingTabs.size()));
                        Log.i(TAG, "commitClosure: commitTabClosure(" + tabToCommit.getId() + ")");
                        mCollectionModel.commitTabClosure(tabToCommit.getId());
                    } else {
                        int tabId = Tab.INVALID_TAB_ID;
                        Log.i(TAG, "commitClosure: commitTabClosure(" + tabId + ")");
                        mCollectionModel.commitTabClosure(tabId);
                    }
                });
    }

    private void removeTab() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int count = mCollectionModel.getCount();
                    if (count == 0) return;

                    Tab tab = mCollectionModel.getTabAt(mGlobalRandom.nextInt(count));
                    Log.i(TAG, "removeTab: removeTab(" + tab.getId() + ")");
                    mCollectionModel.getTabRemover().removeTab(tab, false);
                });
    }
}
