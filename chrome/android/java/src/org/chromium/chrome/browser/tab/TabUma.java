// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.os.SystemClock;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;

import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.net.NetError;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

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

    // TabBackgroundLoadStatus defined in tools/metrics/histograms/histograms.xml.
    static final int TAB_BACKGROUND_LOAD_SHOWN = 0;
    static final int TAB_BACKGROUND_LOAD_LOST = 1;
    static final int TAB_BACKGROUND_LOAD_SKIPPED = 2;
    static final int TAB_BACKGROUND_LOAD_LIM = 3;

    // The enum values for the Tab.RestoreResult histogram. The unusual order is to
    // keep compatibility with the previous instance of the histogram that was using
    // a boolean.
    //
    // Defined in tools/metrics/histograms/histograms.xml.
    private static final int TAB_RESTORE_RESULT_FAILURE_OTHER = 0;
    private static final int TAB_RESTORE_RESULT_SUCCESS = 1;
    private static final int TAB_RESTORE_RESULT_FAILURE_NETWORK_CONNECTIVITY = 2;
    private static final int TAB_RESTORE_RESULT_COUNT = 3;

    // TAB_STATE_* are for TabStateTransferTime and TabTransferTarget histograms.
    // TabState defined in tools/metrics/histograms/histograms.xml.
    private static final int TAB_STATE_INITIAL = 0;
    private static final int TAB_STATE_ACTIVE = 1;
    private static final int TAB_STATE_INACTIVE = 2;
    private static final int TAB_STATE_DETACHED = 3;
    private static final int TAB_STATE_CLOSED = 4;
    private static final int TAB_STATE_MAX = TAB_STATE_CLOSED;

    // Counter of tab shows (as per onShow()) for all tabs.
    private static long sAllTabsShowCount;

    /**
     * State in which the tab was created. This can be used in metric accounting - e.g. to
     * distinguish reasons for a tab to be restored upon first display.
     */
    @IntDef({TabCreationState.LIVE_IN_FOREGROUND, TabCreationState.LIVE_IN_BACKGROUND,
            TabCreationState.FROZEN_ON_RESTORE, TabCreationState.FROZEN_FOR_LAZY_LOAD,
            TabCreationState.FROZEN_ON_RESTORE_FAILED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabCreationState {
        int LIVE_IN_FOREGROUND = 0;
        int LIVE_IN_BACKGROUND = 1;
        int FROZEN_ON_RESTORE = 2;
        int FROZEN_FOR_LAZY_LOAD = 3;
        int FROZEN_ON_RESTORE_FAILED = 4;
    }

    private final @TabCreationState int mTabCreationState;

    // Timestamp when this tab was last shown.
    private long mLastShownTimestamp = -1;

    // Timestamp of the beginning of the current tab restore.
    private long mRestoreStartedAtMillis = -1;

    private long mLastTabStateChangeMillis = -1;
    private int mLastTabState = TAB_STATE_INITIAL;

    // The number of background tabs opened by long pressing on this tab and selecting
    // "Open in a new tab" from the context menu.
    private int mNumBackgroundTabsOpened;

    // Records histogram about which background tab opened from this tab the user switches to
    // first.
    private ChildBackgroundTabShowObserver mChildBackgroundTabShowObserver;

    private TabModelSelectorObserver mNewTabObserver;

    static TabUma create(Tab tab, @TabCreationState int creationState) {
        TabUma tabUma = get(tab);
        if (tabUma != null) tabUma.removeObservers(tab);

        return tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabUma(tab, creationState));
    }

    private static TabUma get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Constructs a new UMA tracker for a specific tab.
     * @param Tab The Tab being monitored for stats.
     * @param creationState In what state the tab was created.
     */
    private TabUma(Tab tab, @TabCreationState int creationState) {
        mTabCreationState = creationState;

        mLastTabStateChangeMillis = System.currentTimeMillis();
        switch (mTabCreationState) {
            case TabCreationState.FROZEN_ON_RESTORE_FAILED:
                // A previous TabUma should have reported an active tab state. Initialize but avoid
                // recording this as a state change.
                mLastTabState = TAB_STATE_ACTIVE;
            // Fall through
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
     * @param succeeded Whether or not the tab restore succeeded.
     * @param time The time taken to perform the tab restore.
     * @param perceivedTime The perceived time taken to perform the tab restore.
     * @param errorCode The error code, on failure (as denoted by the |succeeded| parameter).
     */
    private void recordTabRestoreResult(
            boolean succeeded, long time, long perceivedTime, @NetError int errorCode) {
        if (succeeded) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Tab.RestoreResult", TAB_RESTORE_RESULT_SUCCESS, TAB_RESTORE_RESULT_COUNT);
            RecordHistogram.recordCountHistogram("Tab.RestoreTime", (int) time);
            RecordHistogram.recordCountHistogram("Tab.PerceivedRestoreTime", (int) perceivedTime);
        } else {
            switch (errorCode) {
                case NetError.ERR_INTERNET_DISCONNECTED:
                case NetError.ERR_NAME_RESOLUTION_FAILED:
                case NetError.ERR_DNS_TIMED_OUT:
                    RecordHistogram.recordEnumeratedHistogram("Tab.RestoreResult",
                            TAB_RESTORE_RESULT_FAILURE_NETWORK_CONNECTIVITY,
                            TAB_RESTORE_RESULT_COUNT);
                    break;
                default:
                    RecordHistogram.recordEnumeratedHistogram("Tab.RestoreResult",
                            TAB_RESTORE_RESULT_FAILURE_OTHER, TAB_RESTORE_RESULT_COUNT);
            }
        }
    }

    /**
     * Record the tab state transition into histograms.
     * @param prevState Previous state of the tab.
     * @param newState New state of the tab.
     * @param delta Time elapsed from the last state transition in milliseconds.
     */
    private void recordTabStateTransition(int prevState, int newState, long delta) {
        if (prevState == TAB_STATE_ACTIVE && newState == TAB_STATE_INACTIVE) {
            RecordHistogram.recordLongTimesHistogram100(
                    "Tabs.StateTransfer.Time_Active_Inactive", delta);
        } else if (prevState == TAB_STATE_ACTIVE && newState == TAB_STATE_CLOSED) {
            RecordHistogram.recordLongTimesHistogram100(
                    "Tabs.StateTransfer.Time_Active_Closed", delta);
        }

        if (prevState == TAB_STATE_INITIAL) {
            RecordHistogram.recordEnumeratedHistogram("Tabs.StateTransfer.Target_Initial", newState,
                    TAB_STATE_MAX);
        } else if (prevState == TAB_STATE_ACTIVE) {
            RecordHistogram.recordEnumeratedHistogram("Tabs.StateTransfer.Target_Active", newState,
                    TAB_STATE_MAX);
        } else if (prevState == TAB_STATE_INACTIVE) {
            RecordHistogram.recordEnumeratedHistogram("Tabs.StateTransfer.Target_Inactive",
                    newState, TAB_STATE_MAX);
        }
    }

    /**
     * Records the number of background tabs which were opened from this tab's
     * current URL. Does not record anything if no background tabs were opened.
     */
    private void recordNumBackgroundTabsOpened() {
        if (mNumBackgroundTabsOpened > 0) {
            RecordHistogram.recordCount100Histogram(
                    "Tab.BackgroundTabsOpenedViaContextMenuCount", mNumBackgroundTabsOpened);
        }
        mNumBackgroundTabsOpened = 0;
        mChildBackgroundTabShowObserver = null;
    }

    /**
     * Updates saved TabState and its timestamp. Records the state transition into the histogram.
     * @param newState New state of the tab.
     */
    private void updateTabState(int newState) {
        if (mLastTabState == newState) {
            return;
        }
        long now = System.currentTimeMillis();
        recordTabStateTransition(mLastTabState, newState, now - mLastTabStateChangeMillis);
        mLastTabStateChangeMillis = now;
        mLastTabState = newState;
    }

    /**
     * @return The most recently used rank for this tab in the given TabModel.
     */
    private static int computeMRURank(Tab tab, TabModel model) {
        final long tabLastShow = get(tab).getLastShownTimestamp();
        int mruRank = 0;
        for (int i = 0; i < model.getCount(); i++) {
            Tab otherTab = model.getTabAt(i);
            if (otherTab != tab && TabUma.get(otherTab) != null
                    && TabUma.get(otherTab).getLastShownTimestamp() > tabLastShow) {
                mruRank++;
            }
        }
        return mruRank;
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int selectionType) {
        int rank = computeMRURank(tab, TabModelSelector.from(tab).getModel(tab.isIncognito()));
        long previousTimestampMillis = tab.getTimestampMillis();
        long now = SystemClock.elapsedRealtime();

        // Do not collect the tab switching data for the first switch to a tab after the cold start
        // and for the tab switches that were not user-originated (e.g. the user closes the last
        // incognito tab and the current normal mode tab is shown).
        if (mLastShownTimestamp != -1 && selectionType == TabSelectionType.FROM_USER) {
            long age = now - mLastShownTimestamp;
            RecordHistogram.recordCountHistogram("Tab.SwitchedToForegroundAge", (int) age);
            RecordHistogram.recordCountHistogram("Tab.SwitchedToForegroundMRURank", rank);
        }

        increaseTabShowCount();
        boolean isOnBrowserStartup = sAllTabsShowCount == 1;
        boolean performsLazyLoad = mTabCreationState == TabCreationState.FROZEN_FOR_LAZY_LOAD
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

        if (mLastShownTimestamp == -1) {
            // Record Tab.BackgroundLoadStatus.
            if (mTabCreationState == TabCreationState.LIVE_IN_BACKGROUND) {
                if (mRestoreStartedAtMillis == -1) {
                    RecordHistogram.recordEnumeratedHistogram("Tab.BackgroundLoadStatus",
                            TAB_BACKGROUND_LOAD_SHOWN, TAB_BACKGROUND_LOAD_LIM);
                } else {
                    RecordHistogram.recordEnumeratedHistogram("Tab.BackgroundLoadStatus",
                            TAB_BACKGROUND_LOAD_LOST, TAB_BACKGROUND_LOAD_LIM);

                    if (previousTimestampMillis > 0) {
                        RecordHistogram.recordMediumTimesHistogram(
                                "Tab.LostTabAgeWhenSwitchedToForeground",
                                System.currentTimeMillis() - previousTimestampMillis);
                    }
                }
            } else if (mTabCreationState == TabCreationState.FROZEN_FOR_LAZY_LOAD) {
                assert mRestoreStartedAtMillis == -1;
                RecordHistogram.recordEnumeratedHistogram("Tab.BackgroundLoadStatus",
                        TAB_BACKGROUND_LOAD_SKIPPED, TAB_BACKGROUND_LOAD_LIM);
            }

            // Register the observer for context menu-triggering event here to avoid the case
            // where this is created too early and we start missing out on metrics suddenly.
            mNewTabObserver = new EmptyTabModelSelectorObserver() {
                @Override
                public void onNewTabCreated(Tab newTab) {
                    if (newTab.getParentId() == tab.getId()
                            && newTab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND) {
                        onBackgroundTabOpenedFromContextMenu(newTab);
                    }
                }
            };
            TabModelSelector.from(tab).addObserver(mNewTabObserver);
        }

        // Record "tab age upon first display" metrics. previousTimestampMillis is persisted through
        // cold starts.
        if (mLastShownTimestamp == -1 && previousTimestampMillis > 0) {
            if (isOnBrowserStartup) {
                RecordHistogram.recordCountHistogram("Tabs.ForegroundTabAgeAtStartup",
                        (int) ((System.currentTimeMillis() - previousTimestampMillis)
                                / DateUtils.MINUTE_IN_MILLIS));
            } else if (selectionType == TabSelectionType.FROM_USER) {
                RecordHistogram.recordCountHistogram("Tab.AgeUponRestoreFromColdStart",
                        (int) ((System.currentTimeMillis() - previousTimestampMillis)
                                / DateUtils.MINUTE_IN_MILLIS));
            }
        }

        mLastShownTimestamp = now;

        updateTabState(TAB_STATE_ACTIVE);
    }

    // TabObserver

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        if (type == TabHidingType.ACTIVITY_HIDDEN) {
            recordNumBackgroundTabsOpened();
        } else {
            updateTabState(TAB_STATE_INACTIVE);
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        updateTabState(TAB_STATE_CLOSED);

        if (mTabCreationState == TabCreationState.LIVE_IN_BACKGROUND
                || mTabCreationState == TabCreationState.FROZEN_FOR_LAZY_LOAD) {
            RecordHistogram.recordBooleanHistogram(
                    "Tab.BackgroundTabShown", mLastShownTimestamp != -1);
        }

        recordNumBackgroundTabsOpened();
        removeObservers(tab);
    }

    private void removeObservers(Tab tab) {
        if (mNewTabObserver != null) TabModelSelector.from(tab).removeObserver(mNewTabObserver);
        tab.removeObserver(this);
    }

    @Override
    public void onRestoreStarted(Tab tab) {
        mRestoreStartedAtMillis = SystemClock.elapsedRealtime();
    }

    /** Called when the corresponding tab starts a page load. */
    @Override
    public void onPageLoadStarted(Tab tab, String url) {
        recordNumBackgroundTabsOpened();
    }

    /** Called when the correspoding tab completes a page load. */
    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        // Record only tab restores that the user became aware of. If the restore is triggered
        // speculatively and completes before the user switches to the tab, then this case is
        // reflected in Tab.StatusWhenSwitchedBackToForeground metric.
        if (mRestoreStartedAtMillis != -1 && mLastShownTimestamp >= mRestoreStartedAtMillis) {
            long now = SystemClock.elapsedRealtime();
            long restoreTime = now - mRestoreStartedAtMillis;
            long perceivedRestoreTime = now - mLastShownTimestamp;
            recordTabRestoreResult(true, restoreTime, perceivedRestoreTime, -1);
        }
        mRestoreStartedAtMillis = -1;
    }

    /** Called when the correspoding tab fails a page load. */
    @Override
    public void onPageLoadFailed(Tab tab, int errorCode) {
        if (mRestoreStartedAtMillis != -1 && mLastShownTimestamp >= mRestoreStartedAtMillis) {
            // Load time is ignored for failed loads.
            recordTabRestoreResult(false, -1, -1, errorCode);
        }
        mRestoreStartedAtMillis = -1;
    }

    /** Called when the renderer of the correspoding tab crashes. */
    @Override
    public void onCrash(Tab tab) {
        if (mRestoreStartedAtMillis != -1) {
            // TODO(ppi): Add a bucket in Tab.RestoreResult for restores failed due to
            //            renderer crashes and start to track that.
            mRestoreStartedAtMillis = -1;
        }
    }

    /**
     * Called when a user opens a background tab by long pressing and selecting "Open in a new tab"
     * from the context menu.
     * @param backgroundTab The background tab.
     */
    private void onBackgroundTabOpenedFromContextMenu(Tab backgroundTab) {
        ++mNumBackgroundTabsOpened;

        if (mChildBackgroundTabShowObserver == null) {
            mChildBackgroundTabShowObserver =
                    new ChildBackgroundTabShowObserver(backgroundTab.getParentId());
        }
        mChildBackgroundTabShowObserver.onBackgroundTabOpened(backgroundTab);
    }

    /**
     * @return The timestamp for when this tab was last shown.
     */
    private long getLastShownTimestamp() {
        return mLastShownTimestamp;
    }

    private static void increaseTabShowCount() {
        sAllTabsShowCount++;
    }
}
