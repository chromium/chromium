// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.Space;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.PluralsRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesView;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.Clock;
import java.util.Objects;

/** Displays a horizontal row for a single tab group. */
@NullMarked
public class TabGroupRowView extends LinearLayout {

    /** Represents the title data for the tab group row. */
    public static class TabGroupRowViewTitleData {
        public final @Nullable String title;
        public final int numTabs;
        public final @PluralsRes int rowAccessibilityTextResId;

        /**
         * @param title The title string to display. If empty, a default title will be used.
         * @param numTabs The number of tabs in the group.
         * @param rowAccessibilityTextResId The resource ID for the accessibility string that
         *     describes the row.
         */
        public TabGroupRowViewTitleData(
                @Nullable String title, int numTabs, @PluralsRes int rowAccessibilityTextResId) {
            this.title = title;
            this.numTabs = numTabs;
            this.rowAccessibilityTextResId = rowAccessibilityTextResId;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof TabGroupRowViewTitleData that)) return false;
            return numTabs == that.numTabs
                    && rowAccessibilityTextResId == that.rowAccessibilityTextResId
                    && Objects.equals(title, that.title);
        }

        @Override
        public int hashCode() {
            return Objects.hash(title, numTabs, rowAccessibilityTextResId);
        }
    }

    private TabGroupFaviconCluster mTabGroupFaviconCluster;
    private View mColorView;
    private TextView mTitleTextView;
    private TextView mSubtitleTextView;
    private Space mTextSpace;
    private FrameLayout mImageTilesContainer;
    private ListMenuButton mListMenuButton;

    /** Constructor for inflation. */
    public TabGroupRowView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTabGroupFaviconCluster = findViewById(R.id.tab_group_favicon_cluster);
        mColorView = findViewById(R.id.tab_group_color);
        mTitleTextView = findViewById(R.id.tab_group_title);
        mTextSpace = findViewById(R.id.tab_group_text_space);
        mSubtitleTextView = findViewById(R.id.tab_group_subtitle);
        mImageTilesContainer = findViewById(R.id.image_tiles_container);
        mListMenuButton = findViewById(R.id.tab_group_menu);
    }

    void setupForContainment() {
        Resources res = getContext().getResources();
        ViewGroup.LayoutParams params = getLayoutParams();
        params.height = res.getDimensionPixelSize(R.dimen.tab_group_row_height_containment);
        setLayoutParams(params);
        FrameLayout.MarginLayoutParams clusterParams =
                (FrameLayout.MarginLayoutParams) mTabGroupFaviconCluster.getLayoutParams();
        clusterParams.setMarginStart(
                res.getDimensionPixelSize(R.dimen.tab_group_list_first_element_margin_containment));
        mTabGroupFaviconCluster.setLayoutParams(clusterParams);
    }

    void updateCornersForClusterData(ClusterData clusterData) {
        mTabGroupFaviconCluster.updateCornersForClusterData(clusterData);
    }

    void setDisplayAsShared(boolean isShared) {
        mImageTilesContainer.setVisibility(isShared ? View.VISIBLE : View.GONE);
    }

    void setTitleData(TabGroupRowViewTitleData titleData) {
        String title = titleData.title;
        if (TextUtils.isEmpty(title)) {
            title = TabGroupTitleUtils.getDefaultTitle(getContext(), titleData.numTabs);
        }
        mTitleTextView.setText(title);
        Resources resources = getResources();
        mListMenuButton.setContentDescription(
                resources.getString(R.string.tab_group_menu_accessibility_text, title));

        // Note that the subtitle will also be read for the row, as it just loops over visible text
        // children.
        mTitleTextView.setContentDescription(
                resources.getQuantityString(
                        titleData.rowAccessibilityTextResId,
                        titleData.numTabs,
                        title,
                        titleData.numTabs));
    }

    void setTimestampEvent(TabGroupTimeAgo event) {
        mSubtitleTextView.setVisibility(VISIBLE);
        mTextSpace.setVisibility(VISIBLE);
        TabGroupTimeAgoTextResolver timeAgoResolver =
                new TabGroupTimeAgoTextResolver(getResources(), Clock.systemUTC());
        mSubtitleTextView.setText(
                timeAgoResolver.resolveTimeAgoText(event.timestampMs, event.eventType));
    }

    void setColorIndex(@TabGroupColorId int colorIndex) {
        @ColorInt
        int color =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        getContext(), colorIndex, /* isIncognito= */ false);
        GradientDrawable drawable = (GradientDrawable) mColorView.getBackground();
        drawable.setColor(color);
    }

    void setMenuRunnables(
            @Nullable Runnable openRunnable,
            @Nullable Runnable deleteRunnable,
            @Nullable Runnable leaveRunnable) {
        mListMenuButton.setDelegate(() -> getListMenu(openRunnable, deleteRunnable, leaveRunnable));
        boolean shouldMenuBeVisible =
                openRunnable != null || deleteRunnable != null || leaveRunnable != null;
        mListMenuButton.setVisibility(shouldMenuBeVisible ? VISIBLE : GONE);
    }

    void setSharedImageTilesView(@Nullable SharedImageTilesView sharedImageTilesView) {
        mImageTilesContainer.removeAllViews();
        if (sharedImageTilesView != null) {
            TabUiUtils.attachSharedImageTilesViewToFrameLayout(
                    sharedImageTilesView, mImageTilesContainer);
        }
    }

    private ListMenu getListMenu(
            @Nullable Runnable openRunnable,
            @Nullable Runnable deleteRunnable,
            @Nullable Runnable leaveRunnable) {
        ModelList listItems = new ModelList();
        if (openRunnable != null) {
            listItems.add(buildMenuListItem(R.string.open_tab_group_menu_item, 0, 0));
        }
        if (deleteRunnable != null) {
            listItems.add(buildMenuListItem(R.string.delete_tab_group_menu_item, 0, 0));
        }
        if (leaveRunnable != null) {
            listItems.add(buildMenuListItem(R.string.leave_tab_group_menu_item, 0, 0));
        }
        return BrowserUiListMenuUtils.getBasicListMenu(
                getContext(),
                listItems,
                (item) -> onItemSelected(item, openRunnable, deleteRunnable, leaveRunnable));
    }

    private void onItemSelected(
            PropertyModel item,
            @Nullable Runnable openRunnable,
            @Nullable Runnable deleteRunnable,
            @Nullable Runnable leaveRunnable) {
        @StringRes int textId = item.get(ListMenuItemProperties.TITLE_ID);
        if (textId == R.string.open_tab_group_menu_item && openRunnable != null) {
            openRunnable.run();
        } else if (textId == R.string.delete_tab_group_menu_item && deleteRunnable != null) {
            deleteRunnable.run();
        } else if (textId == R.string.leave_tab_group_menu_item && leaveRunnable != null) {
            leaveRunnable.run();
        }
    }

    public void setRowClickRunnable(@Nullable Runnable runnable) {
        setOnClickListener(runnable == null ? null : v -> runnable.run());
    }
}
