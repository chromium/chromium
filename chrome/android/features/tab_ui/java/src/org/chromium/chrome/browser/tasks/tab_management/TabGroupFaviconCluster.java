// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.ui.UiUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Parent view of the up to four corner favicon images/counts. */
@NullMarked
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

    /**
     * Convenience method that builds a list of urls for a cluster from sync data.
     * TODO(crbug.com/394154545): Move to a better location.
     *
     * @param savedTabGroup The sync data that contains the underling URLs.
     * @return A list of URLs with an appropriate size for the cluster logic.
     */
    public static List<GURL> buildUrlListFromSyncGroup(SavedTabGroup savedTabGroup) {
        List<SavedTabGroupTab> savedTabs = savedTabGroup.savedTabs;
        int numberOfTabs = savedTabs.size();
        int urlCount = Math.min(TabGroupFaviconCluster.CORNER_COUNT, numberOfTabs);
        List<GURL> urlList = new ArrayList<>();
        for (int i = 0; i < urlCount; i++) {
            urlList.add(savedTabs.get(i).url);
        }
        return urlList;
    }

    /**
     * Convenience method that builds a list of urls for a cluster from a tab group model filter.
     * TODO(crbug.com/394154545): Move to a better location.
     *
     * @param tabGroupId The id for the tab group.
     * @param filter The tab group model filter for the tab group.
     * @return A list of URLs with an appropriate size for the cluster logic.
     */
    public static List<GURL> buildUrlListFromFilter(Token tabGroupId, TabGroupModelFilter filter) {
        List<Tab> savedTabs = filter.getTabsInGroup(tabGroupId);
        int numberOfTabs = savedTabs.size();
        int urlCount = Math.min(TabGroupFaviconCluster.CORNER_COUNT, numberOfTabs);
        List<GURL> urlList = new ArrayList<>();
        for (int i = 0; i < urlCount; i++) {
            urlList.add(savedTabs.get(i).getUrl());
        }
        return urlList;
    }

    /**
     * A wrapping resolver that helps count outstanding resolution calls. The callback is invoked
     * when all favicons are fetched.
     */
    private static class TrackingFaviconResolver implements FaviconResolver {
        public int outstandingResolveCalls;
        private final FaviconResolver mDelegateFaviconResolver;
        private @Nullable Runnable mRunOnCompletion;

        /* package */ TrackingFaviconResolver(FaviconResolver delegateFaviconResolver) {
            outstandingResolveCalls = 0;
            mDelegateFaviconResolver = delegateFaviconResolver;
        }

        /* package */ void runOnCompletion(Runnable runnable) {
            assert mRunOnCompletion == null;
            if (outstandingResolveCalls > 0) {
                mRunOnCompletion = runnable;
            } else {
                new Handler().post(runnable);
            }
        }

        @Override
        public void resolve(GURL tabUrl, Callback<Drawable> callback) {
            outstandingResolveCalls++;
            mDelegateFaviconResolver.resolve(
                    tabUrl,
                    (drawable) -> {
                        assert outstandingResolveCalls > 0;
                        outstandingResolveCalls--;
                        callback.onResult(drawable);

                        if (outstandingResolveCalls <= 0 && mRunOnCompletion != null) {
                            mRunOnCompletion.run();
                            mRunOnCompletion = null;
                        }
                    });
        }
    }

    /**
     * An alternative way to use this class. Instead of embedding inside a view, call this to
     * generate an image in an async fashion.
     *
     * @param savedTabGroup Contains urls to fetch favicons from.
     * @param context Used to load resources.
     * @param faviconResolver Favicon fetching mechanism.
     * @param callback Invoked when the bitmap is ready or has failed and null is provided.
     */
    public static void createBitmapFrom(
            SavedTabGroup savedTabGroup,
            Context context,
            FaviconResolver faviconResolver,
            Callback<@Nullable Bitmap> callback) {
        TrackingFaviconResolver trackingFaviconResolver =
                new TrackingFaviconResolver(faviconResolver);

        int numberOfTabs = savedTabGroup.savedTabs.size();
        List<GURL> urlList = buildUrlListFromSyncGroup(savedTabGroup);

        ClusterData clusterData = new ClusterData(trackingFaviconResolver, numberOfTabs, urlList);
        ViewGroup parent = new FrameLayout(context);
        TabGroupFaviconCluster cluster =
                (TabGroupFaviconCluster)
                        LayoutInflater.from(context)
                                .inflate(R.layout.tab_group_favicon_cluster, parent, false);

        int size = context.getResources().getDimensionPixelSize(R.dimen.tab_group_cluster_size);
        cluster.measure(
                View.MeasureSpec.makeMeasureSpec(size, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(size, View.MeasureSpec.EXACTLY));
        cluster.layout(0, 0, cluster.getMeasuredWidth(), cluster.getMeasuredHeight());
        cluster.updateCornersForClusterData(clusterData);

        Runnable onFaviconCompletion =
                () -> {
                    Bitmap bitmap =
                            UiUtils.generateScaledScreenshot(
                                    cluster, size, Bitmap.Config.ARGB_8888);
                    callback.onResult(bitmap);
                };
        trackingFaviconResolver.runOnCompletion(onFaviconCompletion);
    }

    /** Constructor for inflation. */
    public TabGroupFaviconCluster(Context context, @Nullable AttributeSet attrs) {
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
