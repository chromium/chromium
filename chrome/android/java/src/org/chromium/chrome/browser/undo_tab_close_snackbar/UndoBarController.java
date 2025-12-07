// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.Pair;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Queue;

/**
 * A controller that listens to and visually represents cancelable tab closures. This is an abstract
 * class that is implemented for both regular {@link Tab}s and groups based on them as well as tabs
 * combined with {@link SavedTabGroup}s.
 */
@NullMarked
public abstract class UndoBarController
        implements SnackbarManager.SnackbarController, UndoBarThrottle {
    private final TokenHolder mThrottle = new TokenHolder(this::maybeProcessEvents);
    protected final TabModelSelector mTabModelSelector;
    protected final SnackbarManager.SnackbarManageable mSnackbarManageable;
    protected final Context mContext;
    protected final Queue<TabClosureEvent> mEventQueue = new ArrayDeque<>();

    protected static class TabClosureEvent {
        public final List<Tab> tabs = new ArrayList<>();
        public final List<String> savedTabGroupSyncIds = new ArrayList<>();
        public final boolean isAllTabs;

        TabClosureEvent(List<Tab> tabs, boolean isAllTabs) {
            this.tabs.addAll(tabs);
            this.isAllTabs = isAllTabs;
        }

        TabClosureEvent(List<Tab> tabs, List<String> savedTabGroupSyncIds, boolean isAllTabs) {
            this.tabs.addAll(tabs);
            this.savedTabGroupSyncIds.addAll(savedTabGroupSyncIds);
            this.isAllTabs = isAllTabs;
        }
    }

    /**
     * Creates an instance of a {@link UndoBarController}.
     *
     * @param context The {@link Context} in which snackbar is shown.
     * @param selector The {@link TabModelSelector} that will be used to commit and undo tab
     *     closures.
     * @param snackbarManageable The holder class to get the manager that helps to show up snackbar.
     */
    public UndoBarController(
            Context context, TabModelSelector selector, SnackbarManageable snackbarManageable) {
        mSnackbarManageable = snackbarManageable;
        mTabModelSelector = selector;
        mContext = context;
    }

    @Override
    public int startThrottling() {
        return mThrottle.acquireToken();
    }

    @Override
    public void stopThrottling(int token) {
        mThrottle.releaseToken(token);
    }

    protected void queueUndoBar(TabClosureEvent event) {
        mEventQueue.add(event);

        maybeProcessEvents();
    }

    protected void dropFromQueue(List<Tab> tabs) {
        for (Iterator<TabClosureEvent> iterator = mEventQueue.iterator(); iterator.hasNext(); ) {
            TabClosureEvent event = iterator.next();
            event.tabs.removeAll(tabs);
            if (event.tabs.isEmpty()) {
                iterator.remove();
            }
        }
    }

    protected void maybeProcessEvents() {
        if (mThrottle.hasTokens()) return;

        TabClosureEvent event = mEventQueue.poll();
        while (event != null) {
            showUndoBar(event.tabs, event.savedTabGroupSyncIds, event.isAllTabs);
            event = mEventQueue.poll();
        }
    }

    /**
     * Shows an undo close all bar. Based on user actions, this will cause a call to either {@link
     * TabModel#commitTabClosure(int)} or {@link TabModel#cancelTabClosure(int)} to be called for
     * each tab in {@code closedTabs}. This will happen unless {@code
     * SnackbarManager#removeFromStackForData(Object)} is called.
     *
     * @param closedTabs A list of tabs that were closed.
     * @param savedTabGroupSyncIds A list of tab group sync ids that were closed.
     * @param isAllTabs Whether all tabs were closed.
     */
    private void showUndoBar(
            List<Tab> closedTabs, List<String> savedTabGroupSyncIds, boolean isAllTabs) {
        if (closedTabs.isEmpty() && savedTabGroupSyncIds.isEmpty()) return;

        boolean singleTab = closedTabs.size() == 1;
        Pair<String, String> templateAndContent =
                getTemplateAndContentText(closedTabs, savedTabGroupSyncIds);
        // This must always come after retrieving the template and content text.
        int umaType = getUmaType(singleTab, isDeletingTabGroups(savedTabGroupSyncIds), isAllTabs);

        UndoActionData undoActionData = new UndoActionData(closedTabs, savedTabGroupSyncIds);

        mSnackbarManageable
                .getSnackbarManager()
                .showSnackbar(
                        Snackbar.make(
                                        templateAndContent.second,
                                        this,
                                        Snackbar.TYPE_ACTION,
                                        umaType)
                                .setDuration(
                                        isAllTabs
                                                ? SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS
                                                : SnackbarManager.DEFAULT_SNACKBAR_DURATION_MS)
                                .setTemplateText(templateAndContent.first)
                                .setAction(mContext.getString(R.string.undo), undoActionData));
    }

    private int getUmaType(boolean singleTab, boolean deletingTabGroup, boolean isAllTabs) {
        if (deletingTabGroup) {
            return singleTab
                    ? Snackbar.UMA_SINGLE_TAB_GROUP_DELETE_UNDO
                    : Snackbar.UMA_TAB_GROUP_DELETE_UNDO;
        } else if (isAllTabs) {
            return Snackbar.UMA_TAB_CLOSE_ALL_UNDO;
        }
        return singleTab ? Snackbar.UMA_TAB_CLOSE_UNDO : Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO;
    }

    /**
     * Concrete class required to indicate whether tab groups are being deleted.
     *
     * @param savedTabGroupSyncIds The list of closed tab group sync ids.
     */
    protected abstract boolean isDeletingTabGroups(List<String> savedTabGroupSyncIds);

    /**
     * Concrete class required to define the structure and content of the snackbar text.
     *
     * @param closedTabs The list of closed tabs.
     * @param savedTabGroupSyncIds The list of closed tab group sync ids.
     */
    protected abstract Pair<String, String> getTemplateAndContentText(
            List<Tab> closedTabs, List<String> savedTabGroupSyncIds);

    protected static class UndoActionData {
        public final List<Tab> closedTabs = new ArrayList<>();
        public final List<String> closedSavedTabGroupSyncIds = new ArrayList<>();

        UndoActionData(List<Tab> closedTabs, List<String> closedSavedTabGroupSyncIds) {
            this.closedTabs.addAll(closedTabs);
            this.closedSavedTabGroupSyncIds.addAll(closedSavedTabGroupSyncIds);
        }
    }

    /**
     * Calls {@link TabModel#cancelTabClosure(int)} for the tab or for each tab in the list of
     * closed tabs.
     */
    @SuppressWarnings("unchecked")
    @Override
    public void onAction(@Nullable Object actionData) {
        UndoActionData undoActionData = assumeNonNull((UndoActionData) actionData);
        List<Tab> closedTabs = undoActionData.closedTabs;
        if (!closedTabs.isEmpty()) {
            for (Tab closedTab : closedTabs) {
                cancelTabClosure(closedTab.getId());
            }
        }
    }

    private void cancelTabClosure(int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.cancelTabClosure(tabId);
    }

    /**
     * Calls {@link TabModel#commitTabClosure(int)} for the tab or for each tab in the list of
     * closed tabs.
     */
    @SuppressWarnings("unchecked")
    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        UndoActionData undoActionData = assumeNonNull((UndoActionData) actionData);
        List<Tab> closedTabs = undoActionData.closedTabs;
        if (!closedTabs.isEmpty()) {
            for (Tab closedTab : closedTabs) {
                commitTabClosure(closedTab.getId());
            }
        }
    }

    private void commitTabClosure(int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.commitTabClosure(tabId);
    }
}
