// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.graphics.Rect;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * Coordinator for the {@link BottomSheet} and {@link ThinWebView} based Contextual Search panel.
 */
public class ContextualSearchPanelCoordinator implements ContextualSearchPanelInterface {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;

    private final ContextualSearchPanelMetrics mPanelMetrics;

    private WebContents mWebContents;
    private ContentView mWebContentView;

    private boolean mIsActive;

    /**
     * Construct a {@link ContextualSearchPanelCoordinator}.
     * @param context The Android {@link Context}.
     * @param windowAndroid The associated {@link WindowAndroid}.
     */
    public ContextualSearchPanelCoordinator(Context context, WindowAndroid windowAndroid) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mPanelMetrics = new ContextualSearchPanelMetrics();
    }

    private void createWebContents() {
        final Profile profile = Profile.getLastUsedRegularProfile();
        mWebContents = WebContentsFactory.createWebContents(profile, false);
        mWebContentView = ContentView.createContentView(mContext, null, mWebContents);
        final ViewAndroidDelegate delegate =
                ViewAndroidDelegate.createBasicDelegate(mWebContentView);
        mWebContents.initialize(VersionInfo.getProductVersion(), delegate, mWebContentView,
                mWindowAndroid, WebContents.createDefaultInternalsHolder());
        ContentUtils.setUserAgentOverride(mWebContents, /* overrideInNewTabs= */ false);
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
    public void setIsPromoActive(boolean show, boolean isMandatory) {}

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
            int quickActionCategory, int cardTagEnum, @Nullable List<String> inBarRelatedSearches,
            boolean showDefaultSearchInBar, @Nullable List<String> inContentRelatedSearches,
            boolean showDefaultSearchInContent) {}

    @Override
    public void onSearchTermResolved(String searchTerm, @Nullable String pronunciation,
            String thumbnailUrl, String quickActionUri, int quickActionCategory, int cardTagEnum,
            @Nullable List<String> inBarRelatedSearches, boolean showDefaultSearchInBar,
            int defaultQueryInBarTextMaxWidthPx, @Nullable List<String> inContentRelatedSearches,
            boolean showDefaultSearchInContent, int defaultQueryInContentTextMaxWidthPx) {}

    @Override
    public void setCaption(String caption) {}

    @Override
    public void ensureCaption() {}

    @Override
    public void hideCaption() {}

    @Override
    public void setManagementDelegate(ContextualSearchManagementDelegate delegate) {

    }

    @Override
    public void onContextualSearchPrefChanged(boolean isEnabled) {}

    @Override
    public void onPanelNavigatedToPrefetchedSearch(boolean didResolve) {}

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
        return false;
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
    public boolean isPanelOpened() {
        return false;
    }

    @Override
    public boolean isShowing() {
        return false;
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

    // ---------------------------------------------------------------------------------------------
    // endregion
}
