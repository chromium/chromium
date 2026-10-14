// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.undo_tab_close_snackbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
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

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.ListIterator;

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

    /**
     * Undo bar requests waiting to be shown; drained immediately unless undo bars are throttled.
     */
    private final List<UndoClosureSnapshot> mEventQueue = new ArrayList<>();

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

    /**
     * Queues an undo bar to be shown for the given snapshot.
     *
     * @param snapshot The immutable snapshot of the closed tabs and groups.
     */
    protected void queueUndoBar(UndoClosureSnapshot snapshot) {
        mEventQueue.add(snapshot);
        maybeProcessEvents();
    }

    /**
     * Drops the specified tabs from any queued snapshots. Snapshots that become empty are removed.
     *
     * @param tabIds The collection of tab identifiers to remove from the queue.
     */
    protected void dropFromQueue(Collection<Integer> tabIds) {
        for (ListIterator<UndoClosureSnapshot> iterator = mEventQueue.listIterator();
                iterator.hasNext(); ) {
            UndoClosureSnapshot snapshot = iterator.next();
            snapshot = snapshot.withoutTabs(tabIds);
            if (snapshot.isEmpty()) {
                iterator.remove();
            } else {
                iterator.set(snapshot);
            }
        }
    }

    /**
     * Updates all queued snapshots to reflect that the given tab group has been restored.
     *
     * @param gid The identifier of the restored tab group.
     */
    protected void onTabGroupRestored(Token gid) {
        for (ListIterator<UndoClosureSnapshot> iterator = mEventQueue.listIterator();
                iterator.hasNext(); ) {
            UndoClosureSnapshot snapshot = iterator.next();
            iterator.set(snapshot.withGroupRestored(gid));
        }
    }

    /** Processes the queue of snapshots, showing an undo bar for each unthrottled snapshot. */
    protected void maybeProcessEvents() {
        if (mThrottle.hasTokens()) return;

        while (!mEventQueue.isEmpty()) {
            showUndoBar(mEventQueue.remove(0));
        }
    }

    /** Removes all queued snapshots. */
    protected void clearEventQueue() {
        mEventQueue.clear();
    }

    /**
     * Shows an undo close all bar. Based on user actions, this will cause a call to either {@link
     * TabModel#commitTabClosure(int)} or {@link TabModel#cancelTabClosure(int)} to be called for
     * the tabs in the snapshot. This will happen unless {@code
     * SnackbarManager#removeFromStackForData(Object)} is called.
     *
     * @param snapshot An immutable snapshot of the closed tabs and groups.
     */
    private void showUndoBar(UndoClosureSnapshot snapshot) {
        if (snapshot.isEmpty()) return;

        UndoBarContent barContent = UndoBarContentFactory.create(mContext, snapshot);

        mSnackbarManageable
                .getSnackbarManager()
                .showSnackbar(
                        Snackbar.make(
                                        barContent.content,
                                        this,
                                        Snackbar.TYPE_ACTION,
                                        barContent.umaType)
                                .setDuration(
                                        snapshot.isAllTabs
                                                ? SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS
                                                : SnackbarManager.DEFAULT_SNACKBAR_DURATION_MS)
                                .setTemplateText(barContent.template)
                                .setAction(mContext.getString(R.string.undo), barContent));
    }

    /**
     * Records the undo histogram and calls {@link TabModel#cancelTabClosure(int)} for each tab in
     * {@link UndoClosureSnapshot#tabIds()}. Saved tab group sync IDs are ignored by this method.
     *
     * @param actionData The {@link UndoBarContent} payload.
     */
    @Override
    public void onAction(@Nullable Object actionData) {
        UndoBarContent barContent = assumeNonNull((UndoBarContent) actionData);
        RecordHistogram.recordSparseHistogram("Tab.CloseUndoBar.Undone", barContent.umaType);
        for (int tabId : barContent.snapshot.tabIds()) {
            cancelTabClosure(tabId);
        }
    }

    private void cancelTabClosure(int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.cancelTabClosure(tabId);
    }

    /**
     * Records the dismissal histogram and calls {@link TabModel#commitTabClosure(int)} for each tab
     * in {@link UndoClosureSnapshot#tabIds()}. Saved tab group sync IDs are ignored by this method.
     *
     * @param actionData The {@link UndoBarContent} payload.
     */
    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        UndoBarContent barContent = assumeNonNull((UndoBarContent) actionData);
        RecordHistogram.recordSparseHistogram("Tab.CloseUndoBar.Dismissed", barContent.umaType);
        for (int tabId : barContent.snapshot.tabIds()) {
            commitTabClosure(tabId);
        }
    }

    private void commitTabClosure(int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.commitTabClosure(tabId);
    }
}
