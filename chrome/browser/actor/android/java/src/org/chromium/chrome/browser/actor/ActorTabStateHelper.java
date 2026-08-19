// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

/**
 * Manages tab detachment and transitions from physical activities to offscreen background
 * rendering.
 */
@NullMarked
public class ActorTabStateHelper {
    private ActorTabStateHelper() {}

    /**
     * Identifies, detaches, and returns tabs running active tasks from the given selector as
     * background sessions with a specified window ID.
     *
     * @param selector The TabModelSelector of the stopping activity.
     * @param windowId The window ID where the selector is located.
     * @param onTabDetaching Callback invoked for each tab before it is removed from the TabModel,
     *     e.g. to start offscreen rendering.
     * @return A list of prepared BackgroundSession objects.
     */
    public static List<BackgroundSession> detachActiveBackgroundSessions(
            TabModelSelector selector, int windowId, Callback<Tab> onTabDetaching) {
        ThreadUtils.assertOnUiThread();
        TabModel model = selector.getModel(/* incognito= */ false);
        ActorKeyedService service = getActorKeyedService(model);

        if (model == null || service == null || service.getActiveTasksCount() == 0) {
            return Collections.emptyList();
        }

        return findAndDetachActiveSessions(model, service, windowId, onTabDetaching);
    }

    /**
     * Iterates over a copy of the model's tabs, detects active tasks, and performs transitions.
     * Only creates and populates sessions for tabs whose placeholders were inserted correctly.
     */
    private static List<BackgroundSession> findAndDetachActiveSessions(
            TabModel model, ActorKeyedService service, int windowId, Callback<Tab> onTabDetaching) {
        List<BackgroundSession> sessions = new ArrayList<>();

        for (Tab originalTab : model) {
            if (originalTab == null) continue;

            Integer taskId = ActorTaskHelper.getActiveTaskIdOnTab(service, originalTab);
            if (taskId == null) continue;

            int originalIndex = model.indexOf(originalTab);
            Tab placeholderTab = createAndInsertPlaceholder(originalTab, model);
            if (placeholderTab == null) {
                continue;
            }

            BackgroundSession.BackgroundTabData tabData =
                    new BackgroundSession.BackgroundTabData(
                            originalTab, placeholderTab.getId(), originalIndex, windowId);
            BackgroundSession session = BackgroundSession.getSessionForTask(sessions, taskId);
            if (session != null) {
                session.addTabData(tabData);
            } else {
                sessions.add(new BackgroundSession(tabData, taskId));
            }
            onTabDetaching.onResult(originalTab);
            // TODO(b/544014273) : Consider canceling the task if detaching tab was not successful
            model.getTabRemover().removeTab(originalTab, /* allowDialog= */ false);
        }

        return sessions;
    }

    /**
     * Creates and inserts a dormant placeholder tab in the TabModel at the index immediately
     * following the original tab, duplicating its visual properties and state.
     */
    public static @Nullable Tab createAndInsertPlaceholder(Tab originalTab, TabModel model) {
        ThreadUtils.assertOnUiThread();

        int originalIndex = model.indexOf(originalTab);
        if (originalIndex == TabModel.INVALID_TAB_INDEX) return null;

        TabCreator tabCreator = model.getTabCreator();

        TabState originalState = TabStateExtractor.from(originalTab);
        if (originalState == null) return null;

        int placeholderId = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);

        Tab placeholderTab =
                tabCreator.createFrozenTab(originalState, placeholderId, originalIndex + 1);

        if (placeholderTab != null) {
            transferGroupAndPinState(originalTab, placeholderTab, model, originalIndex);
        }

        return placeholderTab;
    }

    /**
     * Symmetrically transfers the grouping and pinning properties from a source tab to a
     * destination tab within the TabModel.
     */
    private static void transferGroupAndPinState(
            Tab sourceTab, Tab destinationTab, TabModel model, int sourceIndex) {
        ThreadUtils.assertOnUiThread();

        if (sourceTab.getIsPinned()) {
            model.pinTab(destinationTab.getId(), /* showUngroupDialog= */ false);
            model.moveTab(destinationTab.getId(), sourceIndex + 1);
        }

        Token tabGroupId = sourceTab.getTabGroupId();
        if (tabGroupId != null) {
            List<Tab> relatedTabs = model.getRelatedTabList(sourceTab.getId());
            int indexInGroup = relatedTabs.indexOf(sourceTab);
            model.mergeListOfTabsToGroup(
                    Collections.singletonList(destinationTab),
                    sourceTab,
                    indexInGroup + 1,
                    TabGroupMergeNotificationType.DONT_NOTIFY);
        }
    }

    private static @Nullable ActorKeyedService getActorKeyedService(@Nullable TabModel model) {
        if (model == null) return null;
        Profile profile = model.getProfile();
        if (profile == null) return null;
        return ActorKeyedServiceFactory.getForProfile(profile.getOriginalProfile());
    }

    /**
     * Restores background tabs belonging to the active window context. Any tabs in the same session
     * belonging to other windows remain backgrounded/offscreen.
     *
     * @param selector The TabModelSelector of the active foreground window.
     * @param activeWindowId The WindowId of the active foreground window.
     * @param window The WindowAndroid instance of the active foreground window.
     * @param backgroundSessions The list of currently tracked active background sessions.
     * @param tabDelegateFactory The delegate factory for the foreground window.
     */
    // TODO(crbug.com/548056570): We plan to replace this with a different flow entirely once
    // tab decoupling allows true windowless Background Sessions.
    public static List<BackgroundSession> restoreActiveWindowBackgroundTabs(
            TabModelSelector selector,
            int activeWindowId,
            WindowAndroid window,
            List<BackgroundSession> backgroundSessions,
            TabDelegateFactory tabDelegateFactory) {
        ThreadUtils.assertOnUiThread();
        TabModel model = selector.getModel(/* incognito= */ false);
        if (model == null) return Collections.emptyList();

        List<BackgroundSession> sessionsToRemove = new ArrayList<>();

        for (BackgroundSession session : backgroundSessions) {
            Iterator<BackgroundSession.BackgroundTabData> iterator =
                    session.getTabDataList().iterator();
            while (iterator.hasNext()) {
                BackgroundSession.BackgroundTabData tabData = iterator.next();
                int tabWindowId = tabData.getTabWindowId();

                // Background sessions created directly in the background may not have a valid
                // window ID associated yet (defaults to INVALID_WINDOW_ID).
                boolean windowMatches =
                        (tabWindowId == TabWindowManager.INVALID_WINDOW_ID
                                || tabWindowId == activeWindowId);

                if (windowMatches) {
                    restoreSessionTabToForeground(tabData, model, window, tabDelegateFactory);
                    // Remove directly using iterator since we are safely iterating.
                    iterator.remove();
                }
            }

            if (session.getTabDataList().isEmpty()) {
                sessionsToRemove.add(session);
            }
        }

        return sessionsToRemove;
    }

    // TODO(crbug.com/548056570): Refactor this method as part of the unified restoration flow.
    private static void restoreSessionTabToForeground(
            BackgroundSession.BackgroundTabData tabData,
            TabModel model,
            WindowAndroid window,
            TabDelegateFactory tabDelegateFactory) {
        Tab originalTab = tabData.getTab();
        if (originalTab == null) return;

        OffscreenRenderingManager.getInstance().stopOffscreenRendering(originalTab);
        originalTab.updateAttachment(window, tabDelegateFactory);

        if (model.indexOf(originalTab) == TabModel.INVALID_TAB_INDEX) {
            Integer placeholderTabId = tabData.getPlaceholderTabId();
            int targetRemoveId = placeholderTabId != null ? placeholderTabId : originalTab.getId();

            Tab placeholderTab = model.getTabById(targetRemoveId);

            int targetIndex;
            boolean wasActive = false;

            if (placeholderTab != null) {
                targetIndex = model.indexOf(placeholderTab);
                assert targetIndex != TabModel.INVALID_TAB_INDEX;
                wasActive = TabModelUtils.getCurrentTab(model) == placeholderTab;
            } else {
                int originalIndex = tabData.getOriginalTabIndex();
                int modelCount = model.getCount();
                targetIndex =
                        originalIndex != TabModel.INVALID_TAB_INDEX
                                ? Math.min(originalIndex, modelCount)
                                : modelCount;
            }

            model.addTab(
                    originalTab,
                    targetIndex,
                    TabLaunchType.FROM_RESTORE,
                    TabCreationState.LIVE_IN_FOREGROUND);

            if (placeholderTab != null) {
                transferGroupAndPinState(placeholderTab, originalTab, model, targetIndex);
                model.getTabRemover().removeTab(placeholderTab, /* allowDialog= */ false);
                placeholderTab.destroy();

                if (wasActive) {
                    TabModelUtils.setIndex(model, model.indexOf(originalTab));
                }
            }
        }
    }
}
