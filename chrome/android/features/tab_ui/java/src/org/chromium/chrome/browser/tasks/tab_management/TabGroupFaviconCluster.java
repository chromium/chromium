// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.url.GURL;

import java.util.List;

/** Parent view of the up to four corner favicon images/counts. */
public class TabGroupFaviconCluster extends ConstraintLayout {

    /**
     * On the drawable side, the corner indexes go clockwise. On the tab side, we order them like
     * text, left to right and top to bottom. The bottom two corners are swapped between these two
     * schemes.
     */
    private static final int[] TAB_TO_CORNER_ORDER = {
        Corner.TOP_LEFT, Corner.TOP_RIGHT, Corner.BOTTOM_LEFT, Corner.BOTTOM_RIGHT,
    };

    /** The number of corners that callers should assume we support. */
    public static int CORNER_COUNT = TAB_TO_CORNER_ORDER.length;

    /**
     * Holds all the data needed to configure the favicon cluster. Currently no optimizations to
     * help update a single corner at a time. Instead everything must be set. But this approach
     * simplifies caller's work.
     */
    public static class ClusterData {
        public final FaviconResolver faviconResolver;
        public final int totalCount;
        public final List<GURL> firstUrls;

        public ClusterData(FaviconResolver faviconResolver, int totalCount, List<GURL> firstUrls) {
            this.faviconResolver = faviconResolver;
            this.totalCount = totalCount;

            // Verify our caller is doing reasonable things. Doesn't really matter if they pass 3 or
            // 4 urls for high total counts.
            if (totalCount < CORNER_COUNT) {
                assert firstUrls.size() == totalCount;
            } else {
                assert firstUrls.size() <= CORNER_COUNT;
            }

            this.firstUrls = firstUrls;
        }
    }

    /** Constructor for inflation. */
    public TabGroupFaviconCluster(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        for (int corner = Corner.TOP_LEFT; corner <= Corner.BOTTOM_LEFT; corner++) {
            TabGroupFaviconQuarter quarter = getTabGroupFaviconQuarter(corner);
            quarter.adjustPositionForCorner(corner, getId());
        }
    }

    void updateCornersForClusterData(ClusterData clusterData) {
        for (int i = 0; i < CORNER_COUNT; i++) {
            @Corner int corner = TAB_TO_CORNER_ORDER[i];
            TabGroupFaviconQuarter quarter = getTabGroupFaviconQuarter(corner);
            if (corner == Corner.BOTTOM_RIGHT && clusterData.totalCount > CORNER_COUNT) {
                // Add one because the plus count occludes one favicon.
                quarter.setPlusCount(clusterData.totalCount - CORNER_COUNT + 1);
            } else if (clusterData.firstUrls.size() > i) {
                GURL url = clusterData.firstUrls.get(i);
                clusterData.faviconResolver.resolve(url, quarter::setImage);
            } else {
                quarter.clear();
            }
        }
    }

    private TabGroupFaviconQuarter getTabGroupFaviconQuarter(@Corner int corner) {
        return (TabGroupFaviconQuarter) getChildAt(corner);
    }
}
