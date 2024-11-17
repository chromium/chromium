// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.content.res.Resources;
import android.os.Handler;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewMetrics.ExitCause;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewMetrics.PaintPreviewMetricsObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Used for displaying a paint preview representation of a tab on startup. */
public class StartupPaintPreview implements PlayerManager.Listener {
    private Tab mTab;
    private StartupPaintPreviewMetrics mMetricsHelper;
    private TabbedPaintPreview mTabbedPaintPreview;
    private Runnable mOnDismissed;
    private SnackbarManager.SnackbarController mSnackbarController;
    private TabObserver mStartupTabObserver;

    private @State int mState;
    private boolean mFirstMeaningfulPaintHappened;
    private boolean mDidStartRestore;
    private int mSnackbarShownCount;
    private boolean mShowingSnackbar;
    private long mActivityCreationTimestampMs;
    private Supplier<Boolean> mShouldRecordFirstPaint;
    private Supplier<Boolean> mIsOfflinePage;

    private static final int DEFAULT_INITIAL_REMOVE_DELAY_MS = 0;
    private static final int SNACKBAR_DURATION_MS = 8 * 1000;

    @IntDef({
        State.READY,
        State.SHOWING,
        State.REMOVED,
        State.NO_CAPTURE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        /** Ready to be shown. */
        int READY = 0;

        /**
         * The paint preview is currently showing. Showing in this case means visible or starting
         * up.
         */
        int SHOWING = 1;

        /** The paint preview has been removed. */
        int REMOVED = 2;

        /** There was no capture available for the current tab. */
        int NO_CAPTURE = 3;
    }

    public StartupPaintPreview(
            Tab tab,
            BrowserStateBrowserControlsVisibilityDelegate visibilityDelegate,
            Runnable progressSimulatorCallback,
            Callback<Boolean> progressPreventionCallback) {
        mTab = tab;
        mMetricsHelper = new StartupPaintPreviewMetrics();
        mTabbedPaintPreview = TabbedPaintPreview.get(mTab);
        mTabbedPaintPreview.setBrowserVisibilityDelegate(visibilityDelegate);
        mTabbedPaintPreview.setProgressbarUpdatePreventionCallback(progressPreventionCallback);
        mTabbedPaintPreview.setProgressSimulatorNeededCallback(progressSimulatorCallback);
        mStartupTabObserver = new StartupPaintPreviewTabObserver();
        mState = State.READY;
        mTab.addObserver(mStartupTabObserver);
    }

    /**
     * Shows a Paint Preview for the provided tab if it exists. Should only be called once.
     * @param onDismissed The callback for when the Paint Preview is dismissed.
     */
    public void show(@Nullable Runnable onDismissed) {
        assert mState != State.SHOWING;

        mOnDismissed = onDismissed;
        boolean hasCapture = false;
        if (mState == State.READY) {
            hasCapture = mTabbedPaintPreview.maybeShow(this);
            mMetricsHelper.recordHadCapture(hasCapture);
            mState = hasCapture ? State.SHOWING : State.NO_CAPTURE;
        }

        if (!hasCapture) {
            if (mOnDismissed != null) {
                mOnDismissed.run();
                mOnDismissed = null;
            }
            mTab.removeObserver(mStartupTabObserver);
        }
    }

    public void setActivityCreationTimestampMs(long activityCreationTimestampMs) {
        mActivityCreationTimestampMs = activityCreationTimestampMs;
    }

    public void setShouldRecordFirstPaint(Supplier<Boolean> shouldRecordFirstPaint) {
        mShouldRecordFirstPaint = shouldRecordFirstPaint;
    }

    public void setIsOfflinePage(Supplier<Boolean> isOfflinePage) {
        mIsOfflinePage = isOfflinePage;
    }

    public void addMetricsObserver(PaintPreviewMetricsObserver observer) {
        mMetricsHelper.addMetricsObserver(observer);
    }

    private void remove(@ExitCause int exitCause) {
        if (mOnDismissed != null) mOnDismissed.run();
        mOnDismissed = null;
        mTab.removeObserver(mStartupTabObserver);

        @State int oldState = mState;
        mState = State.REMOVED;
        if (oldState != State.SHOWING) return;

        boolean needsAnimation =
                exitCause == ExitCause.TAB_FINISHED_LOADING
                        || exitCause == ExitCause.SNACK_BAR_ACTION
                        || exitCause == ExitCause.PULL_TO_REFRESH;
        mTabbedPaintPreview.remove(needsAnimation);
        if (exitCause == ExitCause.TAB_FINISHED_LOADING) showUpgradeToast();
        dismissSnackbar();
        mMetricsHelper.recordExitMetrics(exitCause, mSnackbarShownCount);
    }

    private void showSnackbar() {
        if (mTab == null || mTab.getWindowAndroid() == null || mShowingSnackbar) return;

        SnackbarManager snackbarManager = SnackbarManagerProvider.from(mTab.getWindowAndroid());
        if (snackbarManager == null) return;

        if (mSnackbarController == null) {
            mSnackbarController =
                    new SnackbarManager.SnackbarController() {
                        @Override
                        public void onAction(Object actionData) {
                            mShowingSnackbar = false;
                            remove(ExitCause.SNACK_BAR_ACTION);
                        }

                        @Override
                        public void onDismissNoAction(Object actionData) {
                            mShowingSnackbar = false;
                        }
                    };
        }

        Resources resources = mTab.getContext().getResources();
        Snackbar snackbar =
                Snackbar.make(
                        resources.getString(
                                R.string.paint_preview_startup_upgrade_snackbar_message),
                        mSnackbarController,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_PAINT_PREVIEW_UPGRADE_NOTIFICATION);
        snackbar.setAction(
                resources.getString(R.string.paint_preview_startup_upgrade_snackbar_action), null);
        snackbar.setDuration(SNACKBAR_DURATION_MS);
        SnackbarManagerProvider.from(mTab.getWindowAndroid()).showSnackbar(snackbar);
        mShowingSnackbar = true;
        mSnackbarShownCount++;
    }

    private void dismissSnackbar() {
        if (mSnackbarController == null || mTab == null || mTab.getWindowAndroid() == null) return;

        SnackbarManager snackbarManager = SnackbarManagerProvider.from(mTab.getWindowAndroid());
        if (snackbarManager == null) return;

        snackbarManager.dismissSnackbars(mSnackbarController);
    }

    private void showUpgradeToast() {
        if (mTab == null || mTab.isHidden()) return;

        Toast.makeText(
                        mTab.getContext(),
                        R.string.paint_preview_startup_auto_upgrade_toast,
                        Toast.LENGTH_SHORT)
                .show();
    }

    /**
     * Triggered via {@link PageLoadMetrics.Observer} when First Meaningful Paint happens.
     *
     * @param webContents the webContents that triggered the event.
     */
    public void onWebContentsFirstMeaningfulPaint(WebContents webContents) {
        // If there is no observer or tab this will never handle the event so it should be
        // treated as a success.
        if (mTab == null || mTab.getWebContents() != webContents) return;

        mFirstMeaningfulPaintHappened = true;
        mMetricsHelper.onTabLoadFinished();

        if (mState != State.SHOWING) return;

        // Delay removing paint preview after didFirstVisuallyNonEmptyPaint and no user
        // interaction by |delayMs|. This is to account for 'heavy' pages that take a while
        // to finish painting and avoid having flickers when switching from paint preview
        // to the live page.
        new Handler()
                .postDelayed(
                        () -> {
                            remove(ExitCause.TAB_FINISHED_LOADING);
                        },
                        DEFAULT_INITIAL_REMOVE_DELAY_MS);
    }

    @Override
    public void onCompositorError(int status) {
        // Errors after removal are just disconnect handlers running.
        if (mState != State.SHOWING) return;

        mMetricsHelper.onCompositorFailure(status);
        remove(ExitCause.COMPOSITOR_FAILURE);
    }

    @Override
    public void onViewReady() {
        if (mFirstMeaningfulPaintHappened) {
            remove(ExitCause.TAB_FINISHED_LOADING);
            return;
        }
        mMetricsHelper.onShown();
    }

    @Override
    public void onFirstPaint() {
        if (mState != State.SHOWING) return;

        mMetricsHelper.onFirstPaint(mActivityCreationTimestampMs, mShouldRecordFirstPaint);
    }

    @Override
    public void onUserInteraction() {}

    @Override
    public void onUserFrustration() {
        showSnackbar();
    }

    @Override
    public void onPullToRefresh() {
        remove(ExitCause.PULL_TO_REFRESH);
    }

    @Override
    public void onLinkClick(GURL url) {
        if (mTab == null || !url.isValid() || url.isEmpty()) return;

        mTab.loadUrl(new LoadUrlParams(url.getSpec()));
        remove(ExitCause.LINK_CLICKED);
    }

    @Override
    public boolean isAccessibilityEnabled() {
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled();
    }

    @Override
    public void onAccessibilityNotSupported() {
        // Ignore accessibility failures if accessibility is not enabled.
        if (!isAccessibilityEnabled()) return;

        remove(ExitCause.ACCESSIBILITY_NOT_SUPPORTED);
    }

    TabObserver getTabObserverForTesting() {
        return mStartupTabObserver;
    }

    private class StartupPaintPreviewTabObserver extends EmptyTabObserver {
        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            // onWebContentsFirstMeaningfulPaint won't be called if we're loading an offline page,
            // hence the preview won't get removed.
            // We need to listen to onPageLoadFinished and remove the preview if an offline page is
            // being displayed.
            if (mIsOfflinePage == null || !mIsOfflinePage.get()) return;

            remove(ExitCause.OFFLINE_AVAILABLE);
        }

        @Override
        public void onRestoreStarted(Tab tab) {
            mDidStartRestore = true;
        }

        @Override
        public void onDidStartNavigationInPrimaryMainFrame(
                Tab tab, NavigationHandle navigationHandle) {
            // If we haven't started to restore, this is the navigation call to start the
            // restoration. We shouldn't remove the paint preview player.
            if (!mDidStartRestore) return;

            remove(ExitCause.NAVIGATION_STARTED);
        }

        @Override
        public void onHidden(Tab tab, @TabHidingType int hidingType) {
            dismissSnackbar();

            remove(StartupPaintPreviewMetrics.ExitCause.TAB_HIDDEN);
        }

        @Override
        public void onDestroyed(Tab tab) {
            remove(ExitCause.TAB_DESTROYED);
        }
    }
}
