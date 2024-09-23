// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.app.Activity;
import android.content.Context;
import android.graphics.Point;
import android.os.Handler;
import android.os.SystemClock;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
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
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.GestureEventType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.TokenHolder;

/**
 * Responsible for checking for and displaying Paint Previews that are associated with a
 * {@link Tab} by overlaying the content view.
 */
public class TabbedPaintPreview implements UserData {
    public static final Class<TabbedPaintPreview> USER_DATA_KEY = TabbedPaintPreview.class;
    private static final int CROSS_FADE_DURATION_MS = 500;
    private static final int SCROLL_DELAY_MS = 10;

    private Tab mTab;
    private TabObserver mTabObserver;
    private TabViewProvider mTabbedPaintPreviewViewProvider;
    private PaintPreviewTabService mPaintPreviewTabService;
    private PlayerManager mPlayerManager;
    private BrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate;
    private Runnable mProgressSimulatorNeededCallback;
    private Callback<Boolean> mProgressPreventionCallback;

    private boolean mIsAttachedToTab;
    private boolean mFadingOut;
    private int mPersistentToolbarToken = TokenHolder.INVALID_TOKEN;

    private static PaintPreviewTabService sPaintPreviewTabServiceForTesting;
    private boolean mWasEverShown;

    public static TabbedPaintPreview get(Tab tab) {
        if (tab.getUserDataHost().getUserData(USER_DATA_KEY) == null) {
            tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabbedPaintPreview(tab));
        }
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    private TabbedPaintPreview(Tab tab) {
        mTab = tab;
        mTabbedPaintPreviewViewProvider = new TabbedPaintPreviewViewProvider();
        mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
        mTabObserver =
                new EmptyTabObserver() {
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

                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        // Intentionally do nothing to prevent automatic observer removal on
                        // detachment.
                    }
                };
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
        TraceEvent.begin("TabbedPaintPreview.maybeShow");

        boolean allowedToShow = PaintPreviewTabService.tabAllowedForPaintPreview(mTab);
        if (!allowedToShow) {
            TraceEvent.end("TabbedPaintPreview.maybeShow");
            return false;
        }

        // Check if a capture exists. This is a quick check using a cache.
        boolean hasCapture = getService().hasCaptureForTab(mTab.getId());
        if (!hasCapture) {
            TraceEvent.end("TabbedPaintPreview.maybeShow");
            return false;
        }

        mTab.addObserver(mTabObserver);
        PaintPreviewCompositorUtils.warmupCompositor();

        mPlayerManager =
                new PlayerManager(
                        mTab.getUrl(),
                        mTab.getContext(),
                        getService(),
                        String.valueOf(mTab.getId()),
                        listener,
                        ChromeColors.getPrimaryBackgroundColor(mTab.getContext(), false),
                        /* ignoreInitialScrollOffset= */ false);

        // TODO(crbug.com/40190158): Consider deferring/post tasking. Locally this appears to be
        // slow.
        TraceEvent.begin("TabbedPaintPreview.maybeShow addTabViewProvider");
        mTab.getTabViewManager().addTabViewProvider(mTabbedPaintPreviewViewProvider);
        TraceEvent.end("TabbedPaintPreview.maybeShow addTabViewProvider");
        mIsAttachedToTab = true;
        mWasEverShown = true;

        TraceEvent.end("TabbedPaintPreview.maybeShow");
        return true;
    }

    public void remove(boolean animate) {
        remove(true, animate);
    }

    private void matchScrollAndScale(
            WebContents contents, Point scrollPosition, float scaleFactor) {
        if (contents == null || scaleFactor == 0f || scrollPosition == null) return;
        EventForwarder eventForwarder = contents.getEventForwarder();
        RenderCoordinates coordinates = RenderCoordinates.fromWebContents(contents);

        float scaleDelta = scaleFactor / coordinates.getPageScaleFactor();
        long timeMs = SystemClock.uptimeMillis();
        eventForwarder.onGestureEvent(GestureEventType.PINCH_BEGIN, timeMs, 0.f);
        eventForwarder.onGestureEvent(GestureEventType.PINCH_BY, timeMs, scaleDelta);
        eventForwarder.onGestureEvent(GestureEventType.PINCH_END, timeMs, 0.f);
        // Post the scroll so it occurs after the scale. This ensures positioning is correct.
        new Handler()
                .postDelayed(
                        () -> {
                            eventForwarder.scrollTo(scrollPosition.x, scrollPosition.y);
                        },
                        SCROLL_DELAY_MS);
    }

    /**
     * Removes the view containing the Paint Preview from the most recently shown {@link Tab}. Does
     * nothing if there is no view showing.
     */
    public void remove(boolean matchScroll, boolean animate) {
        PaintPreviewCompositorUtils.stopWarmCompositor();
        if (mTab == null || mPlayerManager == null || mFadingOut) return;
        TraceEvent.begin("TabbedPaintPreview.remove");

        mFadingOut = true;
        mPlayerManager.setAcceptUserInput(false);
        mTab.removeObserver(mTabObserver);
        Point scrollPosition = mPlayerManager.getScrollPosition();
        float scale = mPlayerManager.getScale();
        final boolean supportsAccessibility = mPlayerManager.supportsAccessibility();
        // Destroy early to free up resource, but don't null until faded out so view sticks around.
        mPlayerManager.destroy();
        if (matchScroll) {
            matchScrollAndScale(mTab.getWebContents(), scrollPosition, scale);
        }
        mTabbedPaintPreviewViewProvider
                .getView()
                .animate()
                .alpha(0f)
                .setDuration(animate ? CROSS_FADE_DURATION_MS : 0)
                .setListener(
                        new AnimatorListenerAdapter() {
                            @Override
                            public void onAnimationEnd(Animator animation) {
                                if (mTab != null) {
                                    mTab.getTabViewManager()
                                            .removeTabViewProvider(mTabbedPaintPreviewViewProvider);
                                }
                                if (mPlayerManager != null) {
                                    mPlayerManager = null;
                                }
                                // WebContentsAccessibilityImpl gets its focus stuck on the root ID.
                                // Clear focus here to solve this problem.
                                if (supportsAccessibility) clearFocus();

                                mIsAttachedToTab = false;
                                mFadingOut = false;
                            }
                        });
        // Ensure the progress update occur during the animation.
        setProgressPreventionNeeded(false);

        if (mProgressSimulatorNeededCallback != null) mProgressSimulatorNeededCallback.run();
        TraceEvent.end("TabbedPaintPreview.remove");
    }

    /** Clears focus and accessibility focus. */
    private void clearFocus() {
        WebContents webContents = mTab != null ? mTab.getWebContents() : null;
        if (webContents == null || webContents.isDestroyed()) return;

        // Clear input focus. This is required due to a bug where the root view is treated as
        // focused for input on exit causing talkback to attempt to return focus to the root view.
        // TODO(crbug.com/40760302): this approach could cause loss of focus in a menu, omnibox,
        // etc.
        // is there a less heavy-handed option here?
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        Activity activity = window != null ? window.getActivity().get() : null;
        View v = activity != null ? activity.getCurrentFocus() : null;
        if (v != null) v.clearFocus();

        // Clear accessibility focus.
        WebContentsAccessibility wcax = WebContentsAccessibility.fromWebContents(webContents);
        if (wcax != null) wcax.resetFocus();
    }

    public boolean isShowing() {
        if (mTab == null) return false;

        return mTab.getTabViewManager().isShowing(mTabbedPaintPreviewViewProvider);
    }

    public boolean isAttached() {
        return mIsAttachedToTab;
    }

    /** Persistently shows the toolbar and avoids hiding it on scrolling down. */
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

    static void overridePaintPreviewTabServiceForTesting(PaintPreviewTabService service) {
        sPaintPreviewTabServiceForTesting = service;
    }

    @VisibleForTesting
    boolean wasEverShown() {
        return mWasEverShown;
    }

    View getViewForTesting() {
        return mTabbedPaintPreviewViewProvider.getView();
    }

    PlayerManager getPlayerManagerForTesting() {
        return mPlayerManager;
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
        public @ColorInt int getBackgroundColor(Context context) {
            // TODO(crbug.com/337883538): should be replaced by the background of the preview image
            // rather that the primary background color.
            return ChromeColors.getPrimaryBackgroundColor(mTab.getContext(), false);
        }

        @Override
        public void onHidden() {
            releasePersistentToolbar();
            setProgressPreventionNeeded(false);
        }
    }
}
