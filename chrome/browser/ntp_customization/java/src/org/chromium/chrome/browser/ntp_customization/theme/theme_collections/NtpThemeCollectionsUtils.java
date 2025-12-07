// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.ViewTreeObserver;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

/**
 * Utility class for managing the grid layout of theme collections, handling dynamic span count
 * adjustments based on orientation and screen width.
 */
@NullMarked
public class NtpThemeCollectionsUtils {
    /**
     * Registers a listener for orientation changes to update the grid layout span count.
     *
     * @param context The {@link Context} to register the listener.
     * @param callback The {@link Callback<Configuration>} to handle the configuration change.
     * @return The registered {@link ComponentCallbacks}.
     */
    public static ComponentCallbacks registerOrientationListener(
            Context context, Callback<Configuration> callback) {
        ComponentCallbacks componentCallbacks =
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration newConfig) {
                        callback.onResult(newConfig);
                    }

                    @Override
                    public void onLowMemory() {}
                };
        context.registerComponentCallbacks(componentCallbacks);
        return componentCallbacks;
    }

    /**
     * Adds a one-time global layout listener to the RecyclerView to update the span count of the
     * GridLayoutManager once the layout is complete.
     *
     * @param gridLayoutManager The GridLayoutManager to update.
     * @param recyclerView The RecyclerView to observe.
     */
    public static void updateSpanCountOnLayoutChange(
            GridLayoutManager gridLayoutManager,
            RecyclerView recyclerView,
            int itemMaxWidth,
            int spacing) {
        recyclerView
                .getViewTreeObserver()
                .addOnGlobalLayoutListener(
                        new ViewTreeObserver.OnGlobalLayoutListener() {
                            @Override
                            public void onGlobalLayout() {
                                // We only update the span count if the RecyclerView is visible and
                                // has a valid width. This is crucial for when the device is
                                // rotated while the RecyclerView is in a non-visible bottom sheet.
                                // In that case, we wait until it becomes visible to get the correct
                                // width.
                                if (!recyclerView.isShown() || recyclerView.getWidth() <= 0) {
                                    return;
                                }

                                updateSpanCount(
                                        gridLayoutManager,
                                        recyclerView.getMeasuredWidth(),
                                        itemMaxWidth,
                                        spacing);

                                recyclerView
                                        .getViewTreeObserver()
                                        .removeOnGlobalLayoutListener(this);
                            }
                        });
    }

    /**
     * Update the grid layout span count base on orientation.
     *
     * @param manager The {@link GridLayoutManager} used to update the span count.
     * @param width The available width for the grid.
     * @param itemMaxWidth The max width of each theme collection image.
     * @param spacing The space between 2 theme collection image.
     */
    @VisibleForTesting
    static void updateSpanCount(
            GridLayoutManager manager, int width, int itemMaxWidth, int spacing) {
        int maxItemSpace = itemMaxWidth + spacing;

        double spanCountLowerBound = ((double) (width + spacing)) / maxItemSpace;

        int spanCount = (int) Math.ceil(spanCountLowerBound);
        manager.setSpanCount(spanCount);
    }
}
