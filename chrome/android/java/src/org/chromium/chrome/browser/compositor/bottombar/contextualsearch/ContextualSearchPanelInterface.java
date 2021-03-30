// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.graphics.Rect;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.content_public.browser.WebContents;

/**
 * An interface that encapsulates all the methods that {@link ContextualSearchManager} needs to
 * communicate with the {@link ContextualSearchPanel} to make it easier to swap out the
 * {@link OverlayPanel} based implementation.
 */
public interface ContextualSearchPanelInterface {
    void destroy();

    /** {@link ContextualSearchPanel} methods */
    boolean didTouchContent();
    void setIsPromoActive(boolean show, boolean isMandatory);
    boolean wasPromoInteractive();
    void destroyContent();
    void setSearchTerm(String searchTerm);
    void setDidSearchInvolvePromo();
    void onSearchTermResolved(String searchTerm, String thumbnailUrl, String quickActionUri,
            int quickActionCategory, @CardTag int cardTagEnum, @Nullable String[] relatedSearches);
    void setCaption(String caption);
    void ensureCaption();
    void onContextualSearchPrefChanged(boolean isEnabled);
    void setManagementDelegate(ContextualSearchManagementDelegate delegate);
    void onPanelNavigatedToPrefetchedSearch(boolean didResolve);
    void setWasSearchContentViewSeen();
    void maximizePanelThenPromoteToTab(@StateChangeReason int reason);
    void updateBasePageSelectionYPx(float y);
    void setContextDetails(String selection, String end);
    ContextualSearchBarControl getSearchBarControl();
    ContextualSearchPanelMetrics getPanelMetrics();
    Rect getPanelRect();
    void setIsPanelHelpActive(boolean isActive);
    void clearRelatedSearches();

    /** {@link OverlayPanel} methods */
    void requestPanelShow(@StateChangeReason int reason);
    void loadUrlInPanel(String url);
    void updateBrowserControlsState();
    void updateBrowserControlsState(int current, boolean animate);
    void removeLastHistoryEntry(String historyUrl, long urlTimeMs);
    void onLoadUrlFailed();
    boolean isActive();
    boolean isContentShowing();
    boolean isProcessingPendingNavigation();
    boolean isPeeking();
    WebContents getWebContents();

    /** {@link OverlayPanelBase} methods */
    boolean isPanelOpened();
    boolean isShowing();
    void closePanel(@StateChangeReason int reason, boolean animate);
    void peekPanel(@StateChangeReason int reason);
    void expandPanel(@StateChangeReason int reason);
    void maximizePanel(@StateChangeReason int reason);
    void showPanel(@StateChangeReason int reason);
    @PanelState
    int getPanelState();
}
