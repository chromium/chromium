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
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.Collections;
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
        TabModel regularModel = selector.getModel(/* incognito= */ false);
        ActorKeyedService service = getActorKeyedService(regularModel);

        if (regularModel == null || service == null || service.getActiveTasksCount() == 0) {
            return Collections.emptyList();
        }

        return findAndDetachActiveSessions(regularModel, service, windowId, onTabDetaching);
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
    public static @Nullable Tab createAndInsertPlaceholder(Tab originalTab, TabModel regularModel) {
        ThreadUtils.assertOnUiThread();

        int originalIndex = regularModel.indexOf(originalTab);
        if (originalIndex == TabModel.INVALID_TAB_INDEX) return null;

        TabCreator tabCreator = regularModel.getTabCreator();

        TabState originalState = TabStateExtractor.from(originalTab);
        if (originalState == null) return null;

        int placeholderId = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);

        Tab placeholderTab =
                tabCreator.createFrozenTab(originalState, placeholderId, originalIndex + 1);

        if (placeholderTab != null) {
            transferGroupAndPinState(originalTab, placeholderTab, regularModel, originalIndex);
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
}
