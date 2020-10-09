// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.graphics.Point;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.UserData;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabService;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.paintpreview.player.PlayerManager;
import org.chromium.ui.util.TokenHolder;

/**
 * Responsible for checking for and displaying Paint Previews that are associated with a
 * {@link Tab} by overlaying the content view.
 */
public class TabbedPaintPreview implements UserData {
    public static final Class<TabbedPaintPreview> USER_DATA_KEY = TabbedPaintPreview.class;
    private static final int CROSS_FADE_DURATION_MS = 500;

    private Tab mTab;
    private TabObserver mTabObserver;
    private TabViewProvider mTabbedPainPreviewViewProvider;
    private PaintPreviewTabService mPaintPreviewTabService;
    private PlayerManager mPlayerManager;
    private BrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate;
    private Runnable mProgressSimulatorNeededCallback;
    private Callback<Boolean> mProgressPreventionCallback;

    private boolean mIsAttachedToTab;
    private boolean mFadingOut;
    private int mPersistentToolbarToken = TokenHolder.INVALID_TOKEN;

    private static PaintPreviewTabService sPaintPreviewTabServiceForTesting;

    public static TabbedPaintPreview get(Tab tab) {
        if (tab.getUserDataHost().getUserData(USER_DATA_KEY) == null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabbedPaintPreview(tab));
        }
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private TabbedPaintPreview(Tab tab) {
        mTab = tab;
        mTabbedPainPreviewViewProvider = new TabbedPaintPreviewViewProvider();
        mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onHidden(Tab tab, @TabHidingType int hidingType) {
                releasePersistentToolbar();
                setProgressPreventionNeeded(false);
            }

            @Override
            public void onShown(Tab tab, int type) {
                if (!isShowing()) return;

                showToolbarPersistent();
                setProgressPreventionNeeded(true);
            }
        };
        mTab.addObserver(mTabObserver);
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

    void capture(Callback<Boolean> successCallback) {
        getService().captureTab(mTab, successCallback);
    }

    /**
     * Shows a Paint Preview for the provided tab if it exists.
     * @param listener An interface used for notifying events originated from the player.
     * @return Whether a capture for this tab exists and an attempt for displaying it has started.
     */
    public boolean maybeShow(@NonNull PlayerManager.Listener listener) {
        if (mIsAttachedToTab) return true;

        // Check if a capture exists. This is a quick check using a cache.
        boolean hasCapture = getService().hasCaptureForTab(mTab.getId());
        if (!hasCapture) return false;

        PaintPreviewCompositorUtils.warmupCompositor();
        mPlayerManager = new PlayerManager(mTab.getUrl(), mTab.getContext(), getService(),
                String.valueOf(mTab.getId()), listener,
                ChromeColors.getPrimaryBackgroundColor(mTab.getContext().getResources(), false),
                /*ignoreInitialScrollOffset=*/false);
        mTab.getTabViewManager().addTabViewProvider(mTabbedPainPreviewViewProvider);
        mIsAttachedToTab = true;
        return true;
    }

    public void remove(boolean animate) {
        remove(true, animate);
    }

    /**
     * Removes the view containing the Paint Preview from the most recently shown {@link Tab}. Does
     * nothing if there is no view showing.
     */
    public void remove(boolean matchScroll, boolean animate) {
        PaintPreviewCompositorUtils.stopWarmCompositor();
        if (mTab == null || mPlayerManager == null || mFadingOut) return;

        mFadingOut = true;
        if (matchScroll) {
            Point scrollPosition = mPlayerManager.getScrollPosition();
            if (mTab.getWebContents() != null && scrollPosition != null) {
                mTab.getWebContents().getEventForwarder().scrollTo(
                        scrollPosition.x, scrollPosition.y);
            }
        }
        mPlayerManager.setAcceptUserInput(false);
        mTabbedPainPreviewViewProvider.getView()
                .animate()
                .alpha(0f)
                .setDuration(animate ? CROSS_FADE_DURATION_MS : 0)
                .setListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (mTab != null) {
                            mTab.getTabViewManager().removeTabViewProvider(
                                    mTabbedPainPreviewViewProvider);
                        }
                        if (mPlayerManager != null) {
                            mPlayerManager.destroy();
                            mPlayerManager = null;
                        }
                        mIsAttachedToTab = false;
                        mFadingOut = false;
                    }
                });
        if (mProgressSimulatorNeededCallback != null) mProgressSimulatorNeededCallback.run();
    }

    public boolean isShowing() {
        if (mTab == null) return false;

        return mTab.getTabViewManager().isShowing(mTabbedPainPreviewViewProvider);
    }

    public boolean isAttached() {
        return mIsAttachedToTab;
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

    /**
     * @param progressPrevention Whether progress updates shown in the progress bar should be
     *                           suppressed.
     */
    private void setProgressPreventionNeeded(boolean progressPrevention) {
        if (mProgressPreventionCallback == null) return;

        mProgressPreventionCallback.onResult(progressPrevention);
    }

    @Override
    public void destroy() {
        mTab.removeObserver(mTabObserver);
        mTab = null;
    }

    private PaintPreviewTabService getService() {
        if (sPaintPreviewTabServiceForTesting == null) return mPaintPreviewTabService;

        return sPaintPreviewTabServiceForTesting;
    }

    @VisibleForTesting
    static void overridePaintPreviewTabServiceForTesting(PaintPreviewTabService service) {
        sPaintPreviewTabServiceForTesting = service;
    }

    @VisibleForTesting
    View getViewForTesting() {
        return mTabbedPainPreviewViewProvider.getView();
    }

    private class TabbedPaintPreviewViewProvider implements TabViewProvider {
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
            setProgressPreventionNeeded(false);
        }
    }
}
