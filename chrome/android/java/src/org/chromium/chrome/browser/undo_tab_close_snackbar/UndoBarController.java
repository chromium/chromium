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
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/**
 * A controller that listens to and visually represents cancelable tab closures. This is an abstract
 * class that is implemented for both regular {@link Tab}s and groups based on them as well as tabs
 * combined with {@link SavedTabGroup}s.
 */
@NullMarked
public abstract class UndoBarController
        implements SnackbarManager.SnackbarController, UndoBarThrottle {
    /** Tracking state of an undo bar held by the {@link SnackbarManager}. */
    private static final class ShownBar {
        /**
         * The ids of the bar's tabs that are not yet undone or committed outside of the bar, in
         * snapshot order. The bar's own undo or dismissal acts only on these tabs.
         */
        final Set<@TabId Integer> mRemaining;

        /** Whether any of the bar's tabs was undone outside of the bar. */
        boolean mAnyUndoneExternally;

        ShownBar(UndoClosureSnapshot snapshot) {
            mRemaining = new LinkedHashSet<>();
            for (@TabId int tabId : snapshot.tabIds()) {
                mRemaining.add(tabId);
            }
        }
    }

    private final TokenHolder mThrottle = new TokenHolder(this::maybeProcessEvents);
    protected final TabModelSelector mTabModelSelector;
    protected final SnackbarManager.SnackbarManageable mSnackbarManageable;
    protected final Context mContext;

    /**
     * Undo bar requests waiting to be shown; drained immediately unless undo bars are throttled.
     */
    private final List<UndoClosureSnapshot> mEventQueue = new ArrayList<>();

    /**
     * Undo bars currently held by the {@link SnackbarManager} (showing or queued there), mapped to
     * their tracking state. Keys use identity equality, as {@link UndoBarContent} does not override
     * {@link Object#equals(Object)}. An entry is removed before the bar's own action or dismissal
     * is handled, or once all of its tabs are resolved outside of the bar. Insertion-ordered so
     * that bars resolved together are dismissed in the order they were shown.
     */
    private final Map<UndoBarContent, ShownBar> mShownBars = new LinkedHashMap<>();

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
    private void dropFromQueue(Collection<@TabId Integer> tabIds) {
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
     * Notes that the given tabs were undone or committed outside of the undo bars. The tabs are
     * first dropped from the queued, not yet shown snapshots. Any shown bar whose tabs are now all
     * resolved is dismissed without committing; it records the external undo histogram if any of
     * its tabs was undone outside of the bar, and the dismissal histogram otherwise. A bar with
     * tabs remaining keeps showing, and its own undo or dismissal then acts only on the remaining
     * tabs.
     *
     * @param tabIds The collection of tab identifiers that were resolved.
     * @param undone Whether the tabs were undone (true) or committed (false).
     */
    protected void onTabsResolvedExternally(Collection<@TabId Integer> tabIds, boolean undone) {
        dropFromQueue(tabIds);

        List<UndoBarContent> resolvedBars = new ArrayList<>();
        for (Iterator<Entry<UndoBarContent, ShownBar>> iterator = mShownBars.entrySet().iterator();
                iterator.hasNext(); ) {
            Entry<UndoBarContent, ShownBar> entry = iterator.next();
            ShownBar shownBar = entry.getValue();
            if (shownBar.mRemaining.removeAll(tabIds) && undone) {
                shownBar.mAnyUndoneExternally = true;
            }
            if (!shownBar.mRemaining.isEmpty()) continue;

            iterator.remove();
            UndoBarContent barContent = entry.getKey();
            if (shownBar.mAnyUndoneExternally) {
                RecordHistogram.recordSparseHistogram(
                        "Tab.CloseUndoBar.UndoneExternally", barContent.umaType);
            } else {
                RecordHistogram.recordSparseHistogram(
                        "Tab.CloseUndoBar.Dismissed", barContent.umaType);
            }
            resolvedBars.add(barContent);
        }

        // Dismiss after iterating, as the dismissal re-enters onDismissNoAction(), which returns
        // early for untracked bars (no histogram, no commit).
        SnackbarManager snackbarManager = mSnackbarManageable.getSnackbarManager();
        for (UndoBarContent barContent : resolvedBars) {
            snackbarManager.dismissSnackbars(this, barContent);
        }
    }

    /**
     * Shows an undo bar for the snapshot. The bar ends in one of three ways: undo is tapped ({@link
     * #onAction}, which calls {@link TabModel#cancelTabClosure(int)} for the bar's remaining tabs,
     * i.e. those not yet resolved outside of the bar); it is dismissed or times out ({@link
     * #onDismissNoAction}, which calls {@link TabModel#commitTabClosure(int)} for the remaining
     * tabs); or it is dismissed after all of its tabs were resolved outside of the bar, in which
     * case nothing is committed; it records the external undo histogram if any of its tabs was
     * undone outside of the bar, and the dismissal histogram otherwise.
     *
     * @param snapshot An immutable snapshot of the closed tabs and groups.
     */
    private void showUndoBar(UndoClosureSnapshot snapshot) {
        if (snapshot.isEmpty()) return;

        UndoBarContent barContent = UndoBarContentFactory.create(mContext, snapshot);

        // showSnackbar() silently drops the bar when it cannot be shown; only track shown bars.
        if (mSnackbarManageable.getSnackbarManager().canShowSnackbar()) {
            mShownBars.put(barContent, new ShownBar(snapshot));
        }
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
     * Records the undo histogram and calls {@link TabModel#cancelTabClosure(int)} for each of the
     * bar's remaining tabs, i.e. those not already undone or committed outside of the bar. Does
     * nothing if the bar is not tracked, which cannot happen in practice: {@link
     * SnackbarManager#canShowSnackbar()} gates both the tracking and {@link
     * SnackbarManager#showSnackbar}, and a bar whose tabs were all resolved outside of it is
     * dismissed at once. Saved tab group sync IDs are ignored by this method.
     *
     * @param actionData The {@link UndoBarContent} payload.
     */
    @Override
    public void onAction(@Nullable Object actionData) {
        UndoBarContent barContent = assumeNonNull((UndoBarContent) actionData);
        // Stop tracking first so the resulting tab model events are not treated as external.
        ShownBar shownBar = mShownBars.remove(barContent);
        if (shownBar == null) return;
        RecordHistogram.recordSparseHistogram("Tab.CloseUndoBar.Undone", barContent.umaType);
        // Tabs resolved outside of the bar may have been closed again under a new pending closure
        // that this bar does not own, so only act on the remaining tabs.
        for (@TabId int tabId : shownBar.mRemaining) {
            cancelTabClosure(tabId);
        }
    }

    private void cancelTabClosure(@TabId int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.cancelTabClosure(tabId);
    }

    /**
     * Records the dismissal histogram and calls {@link TabModel#commitTabClosure(int)} for each of
     * the bar's remaining tabs, i.e. those not already undone or committed outside of the bar.
     * Saved tab group sync IDs are ignored by this method. Does nothing if the bar was dismissed
     * because all of its tabs were resolved externally, in which case {@link
     * #onTabsResolvedExternally} already handled the histogram.
     *
     * @param actionData The {@link UndoBarContent} payload.
     */
    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        UndoBarContent barContent = assumeNonNull((UndoBarContent) actionData);
        ShownBar shownBar = mShownBars.remove(barContent);
        // Untracked means onTabsResolvedExternally() dismissed it; never commit from that path.
        if (shownBar == null) return;
        RecordHistogram.recordSparseHistogram("Tab.CloseUndoBar.Dismissed", barContent.umaType);
        // The entry was removed above, so the resulting commit events cannot modify mRemaining.
        for (@TabId int tabId : shownBar.mRemaining) {
            commitTabClosure(tabId);
        }
    }

    private void commitTabClosure(@TabId int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.commitTabClosure(tabId);
    }
}
