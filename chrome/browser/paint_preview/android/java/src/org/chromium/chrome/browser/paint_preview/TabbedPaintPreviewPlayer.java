// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.res.Resources;
import android.graphics.Point;
import android.os.Handler;
import android.os.SystemClock;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.UserData;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.paint_preview.TabbedPaintPreviewMetricsHelper.ExitCause;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.util.concurrent.Callable;

/**
 * Responsible for checking for and displaying Paint Previews that are associated with a
 * {@link Tab} by overlaying the content view.
 */
public class TabbedPaintPreviewPlayer implements TabViewProvider, UserData {
    public static final Class<TabbedPaintPreviewPlayer> USER_DATA_KEY =
            TabbedPaintPreviewPlayer.class;

    private static final int SNACKBAR_DURATION_MS = 8 * 1000;
    private static final int CROSS_FADE_DURATION_MS = 500;
    private static final int DEFAULT_INITIAL_REMOVE_DELAY_MS = 0;
    private static final String INITIAL_REMOVE_DELAY_PARAM = "initial_remove_delay_ms";

    private Tab mTab;
    private PaintPreviewTabService mPaintPreviewTabService;
    private PlayerManager mPlayerManager;
    private Runnable mOnDismissed;
    private Boolean mInitializing;
    private boolean mFirstMeaningfulPaintHappened;
    private TabbedPaintPreviewObserver mObserver;
    private long mLastShownSnackBarTime;
    private boolean mDidStartRestore;
    private boolean mFadingOut;
    private int mSnackbarShownCount;
    private TabbedPaintPreviewMetricsHelper mMetricsHelper;
    private BrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate;
    private int mPersistentToolbarToken = TokenHolder.INVALID_TOKEN;
    private SnackbarManager.SnackbarController mSnackbarController;
    private Runnable mProgressSimulatorNeededCallback;
    private Callback<Boolean> mProgressPreventionCallback;

    public static TabbedPaintPreviewPlayer get(Tab tab) {
        if (tab.getUserDataHost().getUserData(USER_DATA_KEY) == null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabbedPaintPreviewPlayer(tab));
        }
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    class TabbedPaintPreviewObserver extends EmptyTabObserver {
        public void onFirstMeaningfulPaint() {
            mMetricsHelper.onTabLoadFinished();

            if (!isShowingAndNeedsBadge()) return;

            long delayMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP, INITIAL_REMOVE_DELAY_PARAM,
                    DEFAULT_INITIAL_REMOVE_DELAY_MS);
            // Delay removing paint preview after didFirstVisuallyNonEmptyPaint and no user
            // interaction by |delayMs|. This is to account for 'heavy' pages that take a while
            // to finish painting and avoid having flickers when switching from paint preview
            // to the live page.
            new Handler().postDelayed(() -> {
                if (!isShowingAndNeedsBadge()) return;

                removePaintPreview(ExitCause.TAB_FINISHED_LOADING);
            }, delayMs);
        }

        @Override
        public void onRestoreStarted(Tab tab) {
            mDidStartRestore = true;
        }

        @Override
        public void onDidStartNavigation(Tab tab, NavigationHandle navigationHandle) {
            if (mPlayerManager == null || !isShowingAndNeedsBadge()) return;

            // Ignore navigations from subframes. We should only remove the paint preview
            // player when the user navigates to a new page.
            if (!navigationHandle.isInMainFrame()) return;

            // If we haven't started to restore, this is the navigation call to start the
            // restoration. We shouldn't remove the paint preview player.
            if (!mDidStartRestore) return;

            removePaintPreview(ExitCause.NAVIGATION_STARTED);
        }

        @Override
        public void onHidden(Tab tab, @TabHidingType int hidingType) {
            releasePersistentToolbar();
            dismissSnackbar();
            setProgressPreventionNeeded(false);

            if (mPlayerManager == null || !isShowingAndNeedsBadge()) return;

            // If the tab is hidden as a result of pausing the activity we shouldn't remove it.
            if (hidingType == TabHidingType.ACTIVITY_HIDDEN) return;

            removePaintPreview(ExitCause.TAB_HIDDEN);
        }

        @Override
        public void onShown(Tab tab, int type) {
            if (!isShowingAndNeedsBadge()) return;

            showToolbarPersistent();
            setProgressPreventionNeeded(true);
        }
    }

    private TabbedPaintPreviewPlayer(Tab tab) {
        mTab = tab;
        mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
        mMetricsHelper = new TabbedPaintPreviewMetricsHelper();
        mObserver = new TabbedPaintPreviewObserver();
        mTab.addObserver(mObserver);
    }

    public void setBrowserVisibilityDelegate(
            BrowserStateBrowserControlsVisibilityDelegate browserVisibilityDelegate) {
        mBrowserVisibilityDelegate = browserVisibilityDelegate;
    }

    public void setProgressSimulatorNeededCallback(Runnable callback) {
        mProgressSimulatorNeededCallback = callback;
    }

    public void setProgressbarUpdatePreventionCallback(Callback<Boolean> callback) {
        mProgressPreventionCallback = callback;
    }

    /**
     * Triggered via {@link PageLoadMetrics.Observer} when First Meaningful Paint happens.
     * @param webContents the webContents that triggered the event.
     * @return Whether the event was handled for the provided webContents.
     */
    public void onFirstMeaningfulPaint(WebContents webContents) {
        // If there is no observer or tab this will never handle the event so it should be
        // treated as a success.
        if (mObserver == null || mTab == null) return;

        if (mTab.getWebContents() != webContents) return;

        mFirstMeaningfulPaintHappened = true;
        mObserver.onFirstMeaningfulPaint();
    }

    /**
     * Shows a Paint Preview for the provided tab if it exists and has not been displayed for this
     * Tab before.
     * @param onDismissed The callback for when the Paint Preview is dismissed.
     * @param activityCreationTimestampMs The hosting activity's creation time in ms from
     * @param recordFirstPaint Callable to determine if first paint should be recorded.
     * {@link SystemClock#elapsedRealtime}.
     * @return Whether the Paint Preview started to initialize or is already initializating.
     * Note that if the Paint Preview is already showing, this will return false.
     */
    public boolean maybeShow(@Nullable Runnable onDismissed, long activityCreationTimestampMs,
            Callable<Boolean> recordFirstPaint) {
        if (mInitializing != null) return mInitializing;

        // Check if a capture exists. This is a quick check using a cache.
        boolean hasCapture = mPaintPreviewTabService.hasCaptureForTab(mTab.getId());
        mInitializing = hasCapture;
        mMetricsHelper.recordHadCapture(hasCapture);
        if (!hasCapture) return false;

        PaintPreviewCompositorUtils.warmupCompositor();
        mFirstMeaningfulPaintHappened = false;

        mPlayerManager = new PlayerManager(mTab.getUrl(), mTab.getContext(),
                mPaintPreviewTabService, String.valueOf(mTab.getId()), this::onLinkClicked,
                () -> removePaintPreview(ExitCause.PULL_TO_REFRESH),
                () -> {
                    mInitializing = false;
                    if (mFirstMeaningfulPaintHappened) {
                        removePaintPreview(ExitCause.TAB_FINISHED_LOADING);
                        return;
                    }
                    mMetricsHelper.onShown();
                },
                () -> {
                    if (!isShowingAndNeedsBadge()) return;

                    mMetricsHelper.onFirstPaint(activityCreationTimestampMs, recordFirstPaint);
                },
                null,
                ChromeColors.getPrimaryBackgroundColor(mTab.getContext().getResources(), false),
                (status) -> {
                    mMetricsHelper.onCompositorFailure(status);
                    removePaintPreview(ExitCause.COMPOSITOR_FAILURE);
                },
                /*ignoreInitialScrollOffset=*/false);
        mPlayerManager.setUserFrustrationCallback(this::showSnackbar);
        mOnDismissed = onDismissed;
        mTab.getTabViewManager().addTabViewProvider(this);
        return true;
    }

    /**
     * Removes the view containing the Paint Preview from the most recently shown {@link Tab}. Does
     * nothing if there is no view showing.
     */
    private void removePaintPreview(@ExitCause int exitCause) {
        PaintPreviewCompositorUtils.stopWarmCompositor();
        mInitializing = false;
        if (mTab == null || mPlayerManager == null || mFadingOut) return;

        mFadingOut = true;
        if (mOnDismissed != null) mOnDismissed.run();
        mOnDismissed = null;
        Point scrollPosition = mPlayerManager.getScrollPosition();
        if (mTab.getWebContents() != null && scrollPosition != null) {
            mTab.getWebContents().getEventForwarder().scrollTo(scrollPosition.x, scrollPosition.y);
        }
        mPlayerManager.setAcceptUserInput(false);
        boolean needsAnimation = exitCause == ExitCause.TAB_FINISHED_LOADING
                || exitCause == ExitCause.SNACK_BAR_ACTION
                || exitCause == ExitCause.PULL_TO_REFRESH;
        getView()
                .animate()
                .alpha(0f)
                .setDuration(needsAnimation ? CROSS_FADE_DURATION_MS : 0)
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (mTab != null) {
                            mTab.getTabViewManager().removeTabViewProvider(
                                    TabbedPaintPreviewPlayer.this);
                        }
                        if (mPlayerManager != null) {
                            mPlayerManager.destroy();
                            mPlayerManager = null;
                        }
                        mFadingOut = false;
                    }
                });
        if (exitCause == ExitCause.TAB_FINISHED_LOADING) showUpgradeToast();
        if (mProgressSimulatorNeededCallback != null) mProgressSimulatorNeededCallback.run();
        mMetricsHelper.recordExitMetrics(exitCause, mSnackbarShownCount);
    }

    private void showUpgradeToast() {
        if (mTab == null) return;

        Toast.makeText(mTab.getContext(), R.string.paint_preview_startup_auto_upgrade_toast,
                Toast.LENGTH_SHORT).show();
    }

    private void showSnackbar() {
        if (mTab == null || mTab.getWindowAndroid() == null) return;

        // If the Snackbar is already being displayed, return.
        if (System.currentTimeMillis() - mLastShownSnackBarTime < SNACKBAR_DURATION_MS) return;

        if (mSnackbarController == null) {
            mSnackbarController = new SnackbarManager.SnackbarController() {
                @Override
                public void onAction(Object actionData) {
                    removePaintPreview(ExitCause.SNACK_BAR_ACTION);
                }

                @Override
                public void onDismissNoAction(Object actionData) {}
            };
        }
        Resources resources = mTab.getContext().getResources();
        Snackbar snackbar = Snackbar.make(
                resources.getString(R.string.paint_preview_startup_upgrade_snackbar_message),
                mSnackbarController, Snackbar.TYPE_NOTIFICATION,
                Snackbar.UMA_PAINT_PREVIEW_UPGRADE_NOTIFICATION);
        snackbar.setAction(
                resources.getString(R.string.paint_preview_startup_upgrade_snackbar_action), null);
        snackbar.setDuration(SNACKBAR_DURATION_MS);
        SnackbarManagerProvider.from(mTab.getWindowAndroid()).showSnackbar(snackbar);
        mLastShownSnackBarTime = System.currentTimeMillis();
        mSnackbarShownCount++;
    }

    private void dismissSnackbar() {
        if (mSnackbarController == null || mTab == null || mTab.getWindowAndroid() == null) return;

        SnackbarManagerProvider.from(mTab.getWindowAndroid()).dismissSnackbars(mSnackbarController);
    }

    private void setProgressPreventionNeeded(boolean progressPrevention) {
        if (mProgressPreventionCallback == null) return;

        mProgressPreventionCallback.onResult(progressPrevention);
    }

    public boolean isShowingAndNeedsBadge() {
        if (mTab == null) return false;

        return mTab.getTabViewManager().isShowing(this);
    }

    private void onLinkClicked(GURL url) {
        if (mTab == null || !url.isValid() || url.isEmpty()) return;

        removePaintPreview(ExitCause.LINK_CLICKED);
        mTab.loadUrl(new LoadUrlParams(url.getSpec()));
    }

    /**
     * Persistently shows the toolbar and avoids hiding it on scrolling down.
     */
    private void showToolbarPersistent() {
        if (mBrowserVisibilityDelegate == null
                || mPersistentToolbarToken != TokenHolder.INVALID_TOKEN) {
            return;
        }

        mPersistentToolbarToken = mBrowserVisibilityDelegate.showControlsPersistent();
    }

    private void releasePersistentToolbar() {
        if (mBrowserVisibilityDelegate == null) return;

        mBrowserVisibilityDelegate.releasePersistentShowingToken(mPersistentToolbarToken);
        mPersistentToolbarToken = TokenHolder.INVALID_TOKEN;
    }

    @Override
    public int getTabViewProviderType() {
        return Type.PAINT_PREVIEW;
    }

    @Override
    public View getView() {
        return mPlayerManager == null ? null : mPlayerManager.getView();
    }

    @Override
    public void onShown() {
        showToolbarPersistent();
        setProgressPreventionNeeded(true);
    }

    @Override
    public void onHidden() {
        releasePersistentToolbar();
        dismissSnackbar();
        setProgressPreventionNeeded(false);
    }

    @Override
    public void destroy() {
        removePaintPreview(ExitCause.TAB_DESTROYED);
        mTab.removeObserver(mObserver);
        mTab = null;
    }
}
