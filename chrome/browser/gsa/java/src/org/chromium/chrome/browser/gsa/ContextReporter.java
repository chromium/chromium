// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Reports context to GSA for search quality.
 */
public class ContextReporter {
    private static final String TAG = "GSA";

    /**
     * Interface for a selection context reporter used by contextual search.
     */
    public interface SelectionReporter {
        /**
         * Enable selection context reporting.
         * @param callback Callback invoked when reporting selected context to GSA.
         */
        void enable(Callback<GSAContextDisplaySelection> callback);

        /**
         * Disable selection context reporting. The reporting callback doesn't get invoked
         * while disabled.
         */
        void disable();
    }

    // Values for UMA histogram.
    public static final int STATUS_SUCCESS = 0;
    public static final int STATUS_GSA_NOT_AVAILABLE = 1;
    public static final int STATUS_SYNC_NOT_INITIALIZED = 2;
    public static final int STATUS_SYNC_NOT_SYNCING_URLS = 3;
    public static final int STATUS_SYNC_NOT_KEYSTORE_PASSPHRASE = 4;
    public static final int STATUS_SYNC_OTHER = 5;
    public static final int STATUS_SVELTE_DEVICE = 6;
    public static final int STATUS_NO_TAB = 7;
    public static final int STATUS_INCOGNITO = 8;
    public static final int STATUS_INVALID_SCHEME = 9;
    public static final int STATUS_TAB_ID_MISMATCH = 10;
    public static final int STATUS_DUP_TITLE_CHANGE = 11;
    public static final int STATUS_CONNECTION_FAILED = 12;
    public static final int STATUS_SYNC_NOT_READY_AT_REPORT_TIME = 13;
    public static final int STATUS_NOT_SIGNED_IN = 14;
    public static final int STATUS_GSA_ACCOUNT_MISSING = 15;
    public static final int STATUS_GSA_ACCOUNT_MISMATCH = 16;
    public static final int STATUS_RESULT_IS_NULL = 17;
    public static final int STATUS_RESULT_FAILED = 18;
    public static final int STATUS_SUCCESS_WITH_SELECTION = 19;
    public static final int STATUS_DUP_ENTRY = 20;
    // This should always stay last and have the highest number.
    private static final int STATUS_BOUNDARY = 21;

    private final @NonNull Supplier<Tab> mCurrentTabSupplier;
    private final @NonNull Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final @NonNull GSAContextReportDelegate mDelegate;
    private final @Nullable SelectionReporter mSelectionReporter;
    private TabModelSelectorTabObserver mSelectorTabObserver;
    private TabModelSelectorTabModelObserver mModelObserver;
    private boolean mLastContextWasTitleChange;
    private GURL mLastUrl;
    private String mLastTitle;
    private final AtomicBoolean mContextInUse;

    /**
     * Creates a ContextReporter for an Activity.
     * @param currentTabSupplier Supplier of the current tab.
     * @param tabModelSelectorSupplier Supplier of tab model selector.
     * @param selectionReporter Controller enabling/disabling selection context reporting.
     * @param delegate Delegate used to communicate with GSA.
     */
    public ContextReporter(@NonNull Supplier<Tab> currentTabSupplier,
            @NonNull Supplier<TabModelSelector> tabModelSelectorSupplier,
            @Nullable SelectionReporter selectionReporter,
            @NonNull GSAContextReportDelegate delegate) {
        mCurrentTabSupplier = currentTabSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mSelectionReporter = selectionReporter;
        mDelegate = delegate;
        mContextInUse = new AtomicBoolean(false);
        Log.d(TAG, "Created a new ContextReporter");
    }

    /**
     * Starts reporting context.
     */
    public void enable() {
        reportUsageOfCurrentContextIfPossible(mCurrentTabSupplier.get(), false, null);

        TabModelSelector selector = mTabModelSelectorSupplier.get();
        assert selector != null;

        if (mSelectorTabObserver == null) {
            mSelectorTabObserver = new TabModelSelectorTabObserver(selector) {
                @Override
                public void onTitleUpdated(Tab tab) {
                    // Report usage declaring this as a title change.
                    reportUsageOfCurrentContextIfPossible(tab, true, null);
                }

                @Override
                public void onUrlUpdated(Tab tab) {
                    reportUsageOfCurrentContextIfPossible(tab, false, null);
                }
            };
        }
        if (mModelObserver == null) {
            assert !selector.getModels().isEmpty();
            mModelObserver = new TabModelSelectorTabModelObserver(selector) {
                @Override
                public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                    reportUsageOfCurrentContextIfPossible(tab, false, null);
                }
            };
        }
        if (mSelectionReporter != null) mSelectionReporter.enable(this::reportDisplaySelection);
    }

    /**
     * Stops reporting context. Called when the app goes to the background.
     */
    public void disable() {
        reportUsageEndedIfNecessary();

        if (mSelectorTabObserver != null) {
            mSelectorTabObserver.destroy();
            mSelectorTabObserver = null;
        }
        if (mModelObserver != null) {
            mModelObserver.destroy();
            mModelObserver = null;
        }
        if (mSelectionReporter != null) mSelectionReporter.disable();
    }

    /**
     * Reports that the given display selection has been established for the current tab.
     * @param displaySelection The information about the selection being displayed.
     */
    private void reportDisplaySelection(@Nullable GSAContextDisplaySelection displaySelection) {
        Tab currentTab = mCurrentTabSupplier.get();
        reportUsageOfCurrentContextIfPossible(currentTab, false, displaySelection);
    }

    private void reportUsageEndedIfNecessary() {
        if (mContextInUse.compareAndSet(true, false)) mDelegate.reportContextUsageEnded();
    }

    /**
     * Reports the usage of the current context.
     * @param tab Tab being used for the reporting.
     * @param isTitleChange {@code true} if the title is updated.
     * @param displaySelection The information about the selection being displayed.
     */
    private void reportUsageOfCurrentContextIfPossible(
            Tab tab, boolean isTitleChange, @Nullable GSAContextDisplaySelection displaySelection) {
        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null || currentTab.isIncognito()) {
            if (currentTab == null) {
                reportStatus(STATUS_NO_TAB);
                Log.d(TAG, "Not reporting, tab is null");
            } else {
                reportStatus(STATUS_INCOGNITO);
                Log.d(TAG, "Not reporting, tab is incognito");
            }
            reportUsageEndedIfNecessary();
            return;
        }

        GURL currentUrl = currentTab.getUrl();
        if (currentUrl.isEmpty() || !UrlUtilities.isHttpOrHttps(currentUrl)) {
            reportStatus(STATUS_INVALID_SCHEME);
            Log.d(TAG, "Not reporting, URL scheme is invalid");
            reportUsageEndedIfNecessary();
            return;
        }

        // Check whether this is a context change we would like to report.
        if (currentTab.getId() != tab.getId()) {
            reportStatus(STATUS_TAB_ID_MISMATCH);
            Log.d(TAG, "Not reporting, tab ID doesn't match");
            return;
        }
        if (isTitleChange && mLastContextWasTitleChange) {
            reportStatus(STATUS_DUP_TITLE_CHANGE);
            Log.d(TAG, "Not reporting, repeated title update");
            return;
        }
        if (currentTab.getUrl().equals(mLastUrl)
                && TextUtils.equals(currentTab.getTitle(), mLastTitle)
                && displaySelection == null) {
            reportStatus(STATUS_DUP_ENTRY);
            Log.d(TAG, "Not reporting, repeated url and title");
            return;
        }

        reportUsageEndedIfNecessary();

        mDelegate.reportContext(
                currentTab.getUrl().getSpec(), currentTab.getTitle(), displaySelection);
        mLastContextWasTitleChange = isTitleChange;
        mLastUrl = currentTab.getUrl();
        mLastTitle = currentTab.getTitle();
        mContextInUse.set(true);
    }

    /**
     * Records the given status via UMA.
     * Use one of the STATUS_* constants above.
     */
    public static void reportStatus(int status) {
        RecordHistogram.recordEnumeratedHistogram(
                "Search.IcingContextReportingStatus", status, STATUS_BOUNDARY);
    }
}
