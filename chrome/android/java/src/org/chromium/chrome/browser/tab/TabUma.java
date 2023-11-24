// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.os.SystemClock;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.net.NetError;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Centralizes UMA data collection for Tab management.
 * This will drive our memory optimization efforts, specially tab restoring and
 * eviction.
 * All calls must be made from the UI thread.
 */
public class TabUma extends EmptyTabObserver implements UserData {
    private static final Class<TabUma> USER_DATA_KEY = TabUma.class;

    // TabStatus defined in tools/metrics/histograms/histograms.xml.
    static final int TAB_STATUS_MEMORY_RESIDENT = 0;
    static final int TAB_STATUS_RELOAD_EVICTED = 1;
    static final int TAB_STATUS_RELOAD_COLD_START_FG = 6;
    static final int TAB_STATUS_RELOAD_COLD_START_BG = 7;
    static final int TAB_STATUS_LAZY_LOAD_FOR_BG_TAB = 8;
    static final int TAB_STATUS_LIM = 9;

    // TAB_STATE_* are for TabStateTransferTime and TabTransferTarget histograms.
    // TabState defined in tools/metrics/histograms/histograms.xml.
    private static final int TAB_STATE_INITIAL = 0;
    private static final int TAB_STATE_ACTIVE = 1;
    private static final int TAB_STATE_INACTIVE = 2;
    private static final int TAB_STATE_CLOSED = 4;

    // Counter of tab shows (as per onShow()) for all tabs.
    private static long sAllTabsShowCount;

    private @TabCreationState int mTabCreationState;

    // Timestamp when this tab was last shown.
    private long mLastShownTimestamp = -1;

    // Timestamp of the beginning of the current tab restore.
    private long mRestoreStartedAtMillis = -1;

    private long mLastTabStateChangeMillis = -1;
    private int mLastTabState = TAB_STATE_INITIAL;

    /**
     * Creates {@link TabUma} instance optionally. Creates one only when tab creation type
     * is non-null.
     */
    static void createForTab(Tab tab) {
        assert tab.getUserDataHost().getUserData(USER_DATA_KEY) == null;
        @TabCreationState Integer creationState = ((TabImpl) tab).getCreationState();
        if (creationState != null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabUma(tab, creationState));
        }
    }

    /**
     * Constructs a new UMA tracker for a specific tab.
     * @param tab Tab this UMA tracker is created for.
     * @param creationState In what state the tab was created.
     */
    private TabUma(Tab tab, @TabCreationState int creationState) {
        mLastTabStateChangeMillis = System.currentTimeMillis();
        mTabCreationState = creationState;
        switch (mTabCreationState) {
            case TabCreationState.LIVE_IN_FOREGROUND:
                updateTabState(TAB_STATE_ACTIVE);
                break;
            case TabCreationState.LIVE_IN_BACKGROUND: // Fall through
            case TabCreationState.FROZEN_ON_RESTORE: // Fall through
            case TabCreationState.FROZEN_FOR_LAZY_LOAD:
                updateTabState(TAB_STATE_INACTIVE);
        }
        tab.addObserver(this);
    }

    /**
     * Records the tab restore result into several UMA histograms.
     * @param perceivedTime The perceived time taken to perform the tab restore.
     * @param errorCode The error code, NetError.OK on success.
     */
    private void recordTabRestoreResult(long perceivedTime, @NetError int errorCode) {
        if (errorCode == NetError.OK) {
            RecordHistogram.recordCount1MHistogram("Tab.PerceivedRestoreTime", (int) perceivedTime);
        }
    }

    /**
     * Updates saved TabState and its timestamp.
     * @param newState New state of the tab.
     */
    private void updateTabState(int newState) {
        if (mLastTabState == newState) {
            return;
        }
        long now = System.currentTimeMillis();
        mLastTabStateChangeMillis = now;
        mLastTabState = newState;
    }

    // TabObserver

    @Override
    public void onShown(Tab tab, @TabSelectionType int selectionType) {
        long now = SystemClock.elapsedRealtime();

        // Do not collect the tab switching data for the first switch to a tab after the cold start
        // and for the tab switches that were not user-originated (e.g. the user closes the last
        // incognito tab and the current normal mode tab is shown).
        if (mLastShownTimestamp != -1 && selectionType == TabSelectionType.FROM_USER) {
            long age = now - mLastShownTimestamp;
            RecordHistogram.recordCount1MHistogram("Tab.SwitchedToForegroundAge", (int) age);
        }

        increaseTabShowCount();
        boolean isOnBrowserStartup = sAllTabsShowCount == 1;
        boolean performsLazyLoad =
                mTabCreationState == TabCreationState.FROZEN_FOR_LAZY_LOAD
                        && mLastShownTimestamp == -1;

        int status;
        if (mRestoreStartedAtMillis == -1 && !performsLazyLoad) {
            // The tab is *not* being restored or loaded lazily on first display.
            status = TAB_STATUS_MEMORY_RESIDENT;
        } else if (mLastShownTimestamp == -1) {
            // This is first display and the tab is being restored or loaded lazily.
            if (isOnBrowserStartup) {
                status = TAB_STATUS_RELOAD_COLD_START_FG;
            } else if (mTabCreationState == TabCreationState.FROZEN_ON_RESTORE) {
                status = TAB_STATUS_RELOAD_COLD_START_BG;
            } else if (mTabCreationState == TabCreationState.FROZEN_FOR_LAZY_LOAD) {
                status = TAB_STATUS_LAZY_LOAD_FOR_BG_TAB;
            } else {
                assert mTabCreationState == TabCreationState.LIVE_IN_FOREGROUND
                        || mTabCreationState == TabCreationState.LIVE_IN_BACKGROUND;
                status = TAB_STATUS_RELOAD_EVICTED;
            }
        } else {
            // The tab is being restored and this is *not* the first time the tab is shown.
            status = TAB_STATUS_RELOAD_EVICTED;
        }

        // Record only user-visible switches to existing tabs. Do not record displays of newly
        // created tabs (FROM_NEW) or selections of the previous tab that happen when we close the
        // tab opened from intent while exiting Chrome (FROM_CLOSE).
        if (selectionType == TabSelectionType.FROM_USER) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Tab.StatusWhenSwitchedBackToForeground", status, TAB_STATUS_LIM);
        }

        mLastShownTimestamp = now;

        updateTabState(TAB_STATE_ACTIVE);
    }

    private static TabModelSelector getTabModelSelector(Tab tab) {
        TabImpl tabImpl = (TabImpl) tab;
        return tabImpl.getActivity().getTabModelSelector();
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        if (type != TabHidingType.ACTIVITY_HIDDEN) {
            updateTabState(TAB_STATE_INACTIVE);
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        updateTabState(TAB_STATE_CLOSED);
        tab.removeObserver(this);
    }

    @Override
    public void onRestoreStarted(Tab tab) {
        mRestoreStartedAtMillis = SystemClock.elapsedRealtime();
    }

    @Override
    public void onRestoreFailed(Tab tab) {
        assert mRestoreStartedAtMillis == -1;
        if (mLastTabState == TAB_STATE_ACTIVE) {
            mTabCreationState = TabCreationState.LIVE_IN_FOREGROUND;
        } else {
            mTabCreationState = TabCreationState.LIVE_IN_BACKGROUND;
        }
    }

    /** Called when the corresponding tab completes a page load. */
    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
        // Record only tab restores that the user became aware of. If the restore is triggered
        // speculatively and completes before the user switches to the tab, then this case is
        // reflected in Tab.StatusWhenSwitchedBackToForeground metric.
        if (mRestoreStartedAtMillis != -1 && mLastShownTimestamp >= mRestoreStartedAtMillis) {
            long now = SystemClock.elapsedRealtime();
            long perceivedRestoreTime = now - mLastShownTimestamp;
            recordTabRestoreResult(perceivedRestoreTime, NetError.OK);
        }
        mRestoreStartedAtMillis = -1;
    }

    /** Called when the corresponding tab fails a page load. */
    @Override
    public void onPageLoadFailed(Tab tab, @NetError int errorCode) {
        if (mRestoreStartedAtMillis != -1 && mLastShownTimestamp >= mRestoreStartedAtMillis) {
            // Load time is ignored for failed loads.
            assert errorCode != NetError.OK;
            recordTabRestoreResult(-1, errorCode);
        }
        mRestoreStartedAtMillis = -1;
    }

    /** Called when the renderer of the corresponding tab crashes. */
    @Override
    public void onCrash(Tab tab) {
        mRestoreStartedAtMillis = -1;
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    private static void increaseTabShowCount() {
        sAllTabsShowCount++;
    }
}
