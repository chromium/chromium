// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.thinwebview.ThinWebViewConstraints;
import org.chromium.components.thinwebview.ThinWebViewFactory;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * Coordinator for the {@link BottomSheet} and {@link ThinWebView} based Contextual Search panel.
 */
public class ContextualSearchPanelCoordinator implements ContextualSearchPanelInterface {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Integer> mTabHeightSupplier;
    private final int mToolbarHeightPx;
    private final float mFullHeightFraction;

    private final ContextualSearchPanelMetrics mPanelMetrics;
    private final IntentRequestTracker mIntentRequestTracker;

    private ContextualSearchSheetContent mSheetContent;
    private ViewGroup mSheetContentView;
    private ThinWebView mThinWebView;
    private WebContents mWebContents;
    private ContentView mWebContentView;
    private BottomSheetObserver mBottomSheetObserver;

    private ContextualSearchManagementDelegate mManagementDelegate;

    private boolean mIsActive;

    /**
     * Construct a {@link ContextualSearchPanelCoordinator}.
     * @param context The Android {@link Context}.
     * @param windowAndroid The associated {@link WindowAndroid}.
     * @param bottomSheetController The {@link BottomSheetController} that will manage the sheet.
     * @param tabHeightSupplier The {@link Supplier} for the tab height.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ContextualSearchPanelCoordinator(Context context, WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController, Supplier<Integer> tabHeightSupplier,
            IntentRequestTracker intentRequestTracker) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mPanelMetrics = new ContextualSearchPanelMetrics();
        mBottomSheetController = bottomSheetController;
        mTabHeightSupplier = tabHeightSupplier;

        final Resources resources = mContext.getResources();
        mToolbarHeightPx = resources.getDimensionPixelSize(
                org.chromium.chrome.R.dimen.sheet_tab_toolbar_height);
        mFullHeightFraction = ResourcesCompat.getFloat(resources,
                org.chromium.chrome.R.dimen.contextual_search_sheet_full_height_fraction);
        mIntentRequestTracker = intentRequestTracker;
    }

    private void createWebContents() {
        final Profile profile = Profile.getLastUsedRegularProfile();
        mWebContents = WebContentsFactory.createWebContents(profile, false, false);
        mWebContentView = ContentView.createContentView(mContext, null, mWebContents);
        final ViewAndroidDelegate delegate =
                ViewAndroidDelegate.createBasicDelegate(mWebContentView);
        mWebContents.initialize(VersionInfo.getProductVersion(), delegate, mWebContentView,
                mWindowAndroid, WebContents.createDefaultInternalsHolder());
        ContentUtils.setUserAgentOverride(mWebContents, /* overrideInNewTabs= */ false);
    }

    private void destroyWebContents() {
        mSheetContent = null;

        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
            mWebContentView = null;
        }

        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

    private void createSheetContent() {
        assert mWebContentView != null;
        if (mWebContentView.getParent() != null) {
            ((ViewGroup) mWebContentView.getParent()).removeView(mWebContentView);
        }

        final int maxHeight = (int) (mTabHeightSupplier.get() * mFullHeightFraction);
        mThinWebView = ThinWebViewFactory.create(
                mContext, new ThinWebViewConstraints(), mIntentRequestTracker);
        mThinWebView.getView().setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, maxHeight - mToolbarHeightPx));
        mThinWebView.attachWebContents(mWebContents, mWebContentView, null);

        mSheetContentView = new FrameLayout(mContext);
        mSheetContentView.addView(mThinWebView.getView());
        mSheetContentView.setPadding(0, mToolbarHeightPx, 0, 0);
        mSheetContent = new ContextualSearchSheetContent(mSheetContentView, mFullHeightFraction);
    }

    // region ContextualSearchPanelInterface implementation
    // ---------------------------------------------------------------------------------------------

    @Override
    public void destroy() {}

    @Override
    public boolean didTouchContent() {
        return false;
    }

    @Override
    public void setIsPromoActive(boolean show) {}

    @Override
    public boolean wasPromoInteractive() {
        return false;
    }

    @Override
    public void destroyContent() {}

    @Override
    public void setSearchTerm(String searchTerm) {}

    @Override
    public void setSearchTerm(String searchTerm, @Nullable String pronunciation) {}

    @Override
    public void setDidSearchInvolvePromo() {}

    @VisibleForTesting
    @Override
    public void onSearchTermResolved(String searchTerm, String thumbnailUrl, String quickActionUri,
            int quickActionCategory, int cardTagEnum, @Nullable List<String> inBarRelatedSearches) {
    }

    @Override
    public void onSearchTermResolved(String searchTerm, @Nullable String pronunciation,
            String thumbnailUrl, String quickActionUri, int quickActionCategory, int cardTagEnum,
            @Nullable List<String> inBarRelatedSearches) {}

    @Override
    public void setCaption(String caption) {}

    @Override
    public void ensureCaption() {}

    @Override
    public void hideCaption() {}

    @Override
    public void setManagementDelegate(ContextualSearchManagementDelegate delegate) {
        mManagementDelegate = delegate;
    }

    @Override
    public void onContextualSearchPrefChanged(boolean isEnabled) {}

    @Override
    public void setWasSearchContentViewSeen() {}

    @Override
    public void maximizePanelThenPromoteToTab(int reason) {}

    @Override
    public void updateBasePageSelectionYPx(float y) {}

    @Override
    public void setContextDetails(String selection, String end) {}

    @Override
    public ContextualSearchBarControl getSearchBarControl() {
        return null;
    }

    @Override
    public ContextualSearchPanelMetrics getPanelMetrics() {
        return mPanelMetrics;
    }

    @Override
    public Rect getPanelRect() {
        return null;
    }

    @Override
    public void clearRelatedSearches() {}

    @Override
    public void requestPanelShow(int reason) {
        if (mWebContents == null) {
            createWebContents();
            createSheetContent();
            mBottomSheetObserver = new EmptyBottomSheetObserver() {
                @Override
                public void onSheetOpened(int reason) {
                    mManagementDelegate.getOverlayContentDelegate().onVisibilityChanged(true);
                }

                @Override
                public void onSheetStateChanged(int newState, int reason) {
                    if (newState == SheetState.HIDDEN) {
                        mIsActive = false;
                        destroyWebContents();
                    }
                }
            };
            // TODO(sinansahin): It's not guaranteed that we'll be observing the BottomSheet with
            // the contents we provide. We should probably use the return value from
            // BottomSheetController#requestShowContent to decide whether we want to observe the
            // BottomSheet.
            mBottomSheetController.addObserver(mBottomSheetObserver);
        }

        mIsActive = true;
        mBottomSheetController.requestShowContent(mSheetContent, true);
    }

    @Override
    public void loadUrlInPanel(String url) {
        mWebContents.getNavigationController().loadUrl(new LoadUrlParams(url));
    }

    @Override
    public void updateBrowserControlsState() {}

    @Override
    public void updateBrowserControlsState(int current, boolean animate) {}

    @Override
    public void removeLastHistoryEntry(String historyUrl, long urlTimeMs) {}

    @Override
    public void onLoadUrlFailed() {}

    @Override
    public boolean isActive() {
        return mIsActive;
    }

    @Override
    public boolean isContentShowing() {
        // TODO(sinansahin): Replace with real impl. True for the other methods.
        return true;
    }

    @Override
    public boolean isProcessingPendingNavigation() {
        return false;
    }

    @Override
    public boolean isPeeking() {
        return mBottomSheetController.getSheetState() == SheetState.PEEK;
    }

    @Override
    public WebContents getWebContents() {
        return mWebContents;
    }

    @Override
    public ViewGroup getContainerView() {
        return null;
    }

    @Override
    public void setCanHideAndroidBrowserControls(boolean canHideAndroidBrowserControls) {}

    @Override
    public boolean isPanelOpened() {
        return mBottomSheetController.isSheetOpen();
    }

    @Override
    public boolean isShowing() {
        return mBottomSheetController.getCurrentOffset() > 0;
    }

    @Override
    public void closePanel(int reason, boolean animate) {}

    @Override
    public void peekPanel(int reason) {}

    @Override
    public void expandPanel(int reason) {}

    @Override
    public void maximizePanel(int reason) {}

    @Override
    public void showPanel(int reason) {}

    @Override
    public @PanelState int getPanelState() {
        return PanelState.UNDEFINED;
    }

    @Override
    @VisibleForTesting
    public boolean getCanHideAndroidBrowserControls() {
        return false;
    }

    // ---------------------------------------------------------------------------------------------
    // endregion
}
