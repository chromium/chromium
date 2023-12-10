// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.graphics.Canvas;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

/** Provides a surface that displays a list of interest feeds. */
public interface FeedSurfaceProvider {
    /** Destroys the provider. */
    void destroy();

    /**
     * @return The {@link TouchEnabledDelegate} for handling whether touch events are allowed.
     */
    TouchEnabledDelegate getTouchEnabledDelegate();

    /**
     * @return The {@link FeedSurfaceScrollDelegate} for this surface.
     */
    FeedSurfaceScrollDelegate getScrollDelegate();

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

    /** Captures the contents of this provider into the specified output. */
    void captureThumbnail(Canvas canvas);

    /**
     * @return The surface's FeedReliabilityLogger which may be null.
     */
    @Nullable
    FeedReliabilityLogger getReliabilityLogger();

    /** Reloads the contents. */
    void reload();
}
