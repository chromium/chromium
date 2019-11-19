// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * This interface represents a view that is capable of hosting a NativePage.
 */
public interface NativePageHost {
    /**
     * Load a non-native URL in an active tab. This should be used to either navigate away from
     * the current native page or load external content in a content area (i.e. a tab or web
     * contents).
     * @param urlParams The params describing the URL to be loaded.
     * @param incognito Whether the URL should be loaded in incognito mode.
     * @return {@link TabLoadStatus.FULL_PRERENDERED_PAGE_LOAD} or
     *         {@link TabLoadStatus.PARTIAL_PRERENDERED_PAGE_LOAD} if the page has been prerendered.
     *         {@link TabLoadStatus.DEFAULT_PAGE_LOAD} if it had not
     */
    int loadUrl(LoadUrlParams urlParams, boolean incognito);

    /**
     * Determine if the browser is currently in an incognito context.
     * @return True if the browser is incognito.
     */
    boolean isIncognito();

    /**
     * If the host is a tab, get the ID of its parent.
     * @return The ID of the parent tab or INVALID_TAB_ID.
     */
    int getParentId();

    /**
     * Get the currently active tab. This may be the tab that is displaying the native page or the
     * tab behind the bottom sheet (if enabled). If the bottom sheet is open and displaying the
     * NTP UI, then the active tab will be null.
     * @return The active tab.
     */
    @Nullable
    Tab getActiveTab();

    /** @return whether the hosted native page is currently visible. */
    boolean isVisible();

    /**
     * Creates a delegate object needed for history navigation logic.
     * @return {@link HistoryNavigationDelegate} implementation.
     */
    HistoryNavigationDelegate createHistoryNavigationDelegate();
}
