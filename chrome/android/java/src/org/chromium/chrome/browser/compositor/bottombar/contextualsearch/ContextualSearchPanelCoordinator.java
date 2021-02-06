// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.graphics.Rect;

import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.content_public.browser.WebContents;

/**
 * Coordinator for the {@link BottomSheet} and {@link ThinWebView} based Contextual Search panel.
 */
public class ContextualSearchPanelCoordinator implements ContextualSearchPanelInterface {
    public ContextualSearchPanelCoordinator() {}

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
    public void setDidSearchInvolvePromo() {}

    @Override
    public void onSearchTermResolved(String searchTerm, String thumbnailUrl, String quickActionUri,
            int quickActionCategory, int cardTagEnum) {}

    @Override
    public void setCaption(String caption) {}

    @Override
    public void setManagementDelegate(ContextualSearchManagementDelegate delegate) {}

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
        return null;
    }

    @Override
    public Rect getPanelRect() {
        return null;
    }

    @Override
    public void setIsPanelHelpActive(boolean isActive) {}

    @Override
    public void requestPanelShow(int reason) {}

    @Override
    public void loadUrlInPanel(String url) {}

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
        return false;
    }

    @Override
    public boolean isContentShowing() {
        return false;
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
}
