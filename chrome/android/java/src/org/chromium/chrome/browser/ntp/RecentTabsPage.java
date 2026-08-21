// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.text.TextUtils;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.native_page.BasicSmoothTransitionDelegate;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

/**
 * The native recent tabs page. Lists recently closed tabs, open windows and tabs from the user's
 * synced devices, and snapshot documents sent from Chrome to Mobile in an expandable list view.
 */
@NullMarked
public class RecentTabsPage
        implements NativePage,
                View.OnAttachStateChangeListener,
                InvalidationAwareThumbnailProvider,
                BrowserControlsStateProvider.Observer,
                TouchEnabledDelegate {
    private final Activity mActivity;
    private final @Nullable BrowserControlsStateProvider mBrowserControlsStateProvider;
    private String mUrl;
    private final String mTitle;
    private final RecentTabsCoordinator mCoordinator;

    /** Whether {@link #mView} is attached to the application window. */
    private boolean mIsAttachedToWindow;

    private @Nullable SmoothTransitionDelegate mSmoothTransitionDelegate;

    /**
     * Constructor returns an instance of RecentTabsPage.
     *
     * @param activity The activity this view belongs to.
     * @param recentTabsManager The RecentTabsManager which provides the model data.
     * @param navigationDelegate The {@link NativePageNavigationDelegate} for handling navigation.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} used to provide
     *     offset values.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     * @param edgeToEdgeSupplier Supplier for the {@link EdgeToEdgeController} for bottom insets.
     * @param url The URL the page is being opened with.
     */
    public RecentTabsPage(
            Activity activity,
            RecentTabsManager recentTabsManager,
            NativePageNavigationDelegate navigationDelegate,
            BrowserControlsStateProvider browserControlsStateProvider,
            NonNullObservableSupplier<Integer> tabStripHeightSupplier,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            String url) {
        mActivity = activity;
        mUrl = url;
        Resources resources = activity.getResources();

        mTitle = resources.getString(R.string.recent_tabs);
        mCoordinator =
                new RecentTabsCoordinator(
                        activity,
                        recentTabsManager,
                        navigationDelegate,
                        tabStripHeightSupplier,
                        edgeToEdgeSupplier,
                        /* parent= */ null);
        mCoordinator.getView().addOnAttachStateChangeListener(this);

        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mBrowserControlsStateProvider.addObserver(this);
            onBottomControlsHeightChanged(
                    mBrowserControlsStateProvider.getBottomControlsHeight(),
                    mBrowserControlsStateProvider.getBottomControlsMinHeight());
        } else {
            mBrowserControlsStateProvider = null;
        }

        updateForUrl(url);
    }

    // NativePage overrides

    @Override
    public String getUrl() {
        return mUrl;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return SemanticColorUtils.getDefaultBgColor(mActivity);
    }

    @Override
    public boolean needsToolbarShadow() {
        return true;
    }

    @Override
    public View getView() {
        return mCoordinator.getView();
    }

    @Override
    public String getHost() {
        return UrlConstants.RECENT_TABS_HOST;
    }

    @Override
    public SmoothTransitionDelegate enableSmoothTransition() {
        if (mSmoothTransitionDelegate == null) {
            mSmoothTransitionDelegate = new BasicSmoothTransitionDelegate(getView());
        }
        return mSmoothTransitionDelegate;
    }

    @Override
    public boolean supportsEdgeToEdge() {
        return true;
    }

    @Override
    public void destroy() {
        assert !mIsAttachedToWindow : "Destroy called before removed from window";
        mCoordinator.getView().removeOnAttachStateChangeListener(this);
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.removeObserver(this);
        }
        mCoordinator.destroy();
    }

    @Override
    public void updateForUrl(String url) {
        mUrl = url;
        GURL gurl = new GURL(url);
        String fragment = gurl.getRef();
        if (!TextUtils.isEmpty(fragment)) {
            mCoordinator.setTargetSessionTag(fragment);
        } else {
            mCoordinator.setTargetSessionTag(null);
        }
    }

    @Override
    public int getHeightOverlappedWithTopControls() {
        return mBrowserControlsStateProvider == null
                ? 0
                : mBrowserControlsStateProvider.getTopControlsHeight();
    }

    // View.OnAttachStateChangeListener
    @Override
    public void onViewAttachedToWindow(View view) {
        // Called when the user opens the RecentTabsPage or switches back to the RecentTabsPage from
        // another tab.
        mIsAttachedToWindow = true;

        // Work around a bug on Samsung devices where the recent tabs page does not appear after
        // toggling the Sync quick setting.  For some reason, the layout is being dropped on the
        // flow and we need to force a root level layout to get the UI to appear.
        ViewUtils.requestLayout(view.getRootView(), "RecentTabsPage.onViewAttachedToWindow");
    }

    @Override
    public void onViewDetachedFromWindow(View view) {
        // Called when the user navigates from the RecentTabsPage or switches to another tab.
        mIsAttachedToWindow = false;
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        return mCoordinator.shouldCaptureThumbnail();
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        mCoordinator.captureThumbnail(canvas);
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        updateMargins();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateMargins();
    }

    @Override
    public void onControlsPositionChanged(@ControlsPosition int controlsPosition) {
        updateMargins();
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            boolean topControlsMinHeightChanged,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        updateMargins();
    }

    private void updateMargins() {
        if (mBrowserControlsStateProvider == null) return;

        final int topControlsHeight = mBrowserControlsStateProvider.getTopControlsHeight();
        final int contentOffset = mBrowserControlsStateProvider.getContentOffset();
        int topMargin = mCoordinator.getRootViewTopMargin();

        // If the top controls are at the resting position or their height is decreasing, we want to
        // update the margin. We don't do this if the controls height is increasing because changing
        // the margin shrinks the view height to its final value, leaving a gap at the bottom until
        // the animation finishes.
        // On native pages, when controls position switches from bottom to top, contentOffset
        // is initialized to 0 while topControlsHeight increases to its resting height.
        // We want to ensure topMargin is updated when controls are top-anchored or resting.
        if (contentOffset >= topControlsHeight
                || mBrowserControlsStateProvider.getControlsPosition() == ControlsPosition.TOP) {
            topMargin = topControlsHeight;
        }

        // If the content offset is different from the margin, we use translationY to position the
        // view in line with the content offset. We only apply translationY when contentOffset >
        // topMargin
        // (e.g. during top banner animations) to prevent negative translation when contentOffset is
        // 0.
        int translationY = 0;
        if (contentOffset > topMargin) {
            translationY = contentOffset - topMargin;
        }
        final int bottomMargin = mBrowserControlsStateProvider.getBottomControlsHeight();
        mCoordinator.updateMargins(topMargin, bottomMargin, translationY);
    }

    @Nullable Callback<Integer> getTabStripHeightChangeCallbackForTesting() {
        return mCoordinator.getTabStripHeightChangeCallbackForTesting(); // IN-TEST
    }

    boolean performChildClickForTesting(int groupPosition, int childPosition) {
        return mCoordinator.performChildClickForTesting(groupPosition, childPosition); // IN-TEST
    }

    @Override
    public void setTouchEnabled(boolean enabled) {
        mCoordinator.setTouchEnabled(enabled);
    }
}
