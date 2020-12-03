// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared;

import android.graphics.Canvas;
import android.view.View;

import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPageLayout;
import org.chromium.chrome.browser.shopping_tiles.NTPTabLayout.MenuDelegate;
import org.chromium.chrome.browser.shopping_tiles.NTPTabLayout.TabSelectionDelegate;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

/**
 * Provides a surface that displays a list of interest feeds.
 */
public interface FeedSurfaceProvider {
    /**
     * Destroys the provider.
     */
    void destroy();

    /**
     * @return The {@link TouchEnabledDelegate} for handling whether touch events are allowed.
     */
    ContextMenuManager.TouchEnabledDelegate getTouchEnabledDelegate();

    /**
     * @return The {@link ScrollDelegate} for this surface.
     */
    NewTabPageLayout.ScrollDelegate getScrollDelegate();

    /**
     * @return The {@link UiConfig} about the view used in this surface.
     */
    UiConfig getUiConfig();

    /**
     * @return The android {@link View} that the surface is supposed to show.
     */
    View getView();

    /**
     * @return Whether a new thumbnail should be captured.
     */
    boolean shouldCaptureThumbnail();

    /**
     * Captures the contents of this provider into the specified output.
     */
    void captureThumbnail(Canvas canvas);

    TabSelectionDelegate getTabLayoutDelegate();

    MenuDelegate getTabLayoutMenuDelegate();
}
