// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils.buildMenuListItem;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.util.Pair;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.Clock;

/** Displays a horizontal row for a single tab group. */
public class TabGroupRowView extends LinearLayout {
    private ViewGroup mTabGroupStartIconParent;
    private View mColorView;
    private TextView mTitleTextView;
    private TextView mSubtitleTextView;
    private ListMenuButton mListMenuButton;
    private TabGroupTimeAgoResolver mTimeAgoResolver;

    /** Constructor for inflation. */
    public TabGroupRowView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTabGroupStartIconParent = findViewById(R.id.tab_group_start_icon);
        mColorView = findViewById(R.id.tab_group_color);
        mTitleTextView = findViewById(R.id.tab_group_title);
        mSubtitleTextView = findViewById(R.id.tab_group_subtitle);
        mListMenuButton = findViewById(R.id.more);
        mTimeAgoResolver = new TabGroupTimeAgoResolver(getResources(), Clock.systemUTC());

        for (int corner = Corner.TOP_LEFT; corner <= Corner.BOTTOM_LEFT; corner++) {
            TabGroupFaviconQuarter quarter = getTabGroupFaviconQuarter(corner);
            quarter.setCorner(corner, mTabGroupStartIconParent.getId());
        }
    }

    void setFavicon(Drawable favicon, int plusCount, @Corner int corner) {
        getTabGroupFaviconQuarter(corner).setIconOrText(favicon, plusCount);
    }

    void setTitleData(Pair<String, Integer> titleData) {
        String title = titleData.first;
        if (TextUtils.isEmpty(title)) {
            title = TabGroupTitleEditor.getDefaultTitle(getContext(), titleData.second);
        }
        mTitleTextView.setText(title);
        Resources resources = getResources();
        mListMenuButton.setContentDescription(
                resources.getString(R.string.tab_group_menu_accessibility_text, title));

        // Note that the subtitle will also be read for the row, as it just loops over visible text
        // children.
        mTitleTextView.setContentDescription(
                resources.getString(R.string.tab_group_row_accessibility_text, title));
    }

    void setCreationMillis(long creationMillis) {
        mSubtitleTextView.setText(mTimeAgoResolver.resolveTimeAgoText(creationMillis));
    }

    void setColorIndex(@TabGroupColorId int colorIndex) {
        @ColorInt
        int color =
                ColorPickerUtils.getTabGroupColorPickerItemColor(
                        getContext(), colorIndex, /* isIncognito= */ false);
        GradientDrawable drawable = (GradientDrawable) mColorView.getBackground();
        drawable.setColor(color);
    }

    void setMenuRunnables(
            @Nullable Runnable openRunnable,
            @Nullable Runnable deleteRunnable,
            @Nullable Runnable leaveRunnable) {
        setOnClickListener(openRunnable == null ? null : v -> openRunnable.run());
        mListMenuButton.setDelegate(() -> getListMenu(openRunnable, deleteRunnable, leaveRunnable));
    }

    void resetOnBind() {
        for (int corner = Corner.TOP_LEFT; corner <= Corner.BOTTOM_LEFT; corner++) {
            setFavicon(null, 0, corner);
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

    private TabGroupFaviconQuarter getTabGroupFaviconQuarter(@Corner int corner) {
        return (TabGroupFaviconQuarter) mTabGroupStartIconParent.getChildAt(corner);
    }

    void setTimeAgoResolverForTesting(TabGroupTimeAgoResolver timeAgoResolver) {
        mTimeAgoResolver = timeAgoResolver;
    }

    void resetSharedState(boolean isShared) {
        View sharedView = findViewById(R.id.tab_group_row_shared_image_tiles);
        assert sharedView != null;
        if (isShared) {
            sharedView.setVisibility(View.VISIBLE);
        } else {
            sharedView.setVisibility(View.GONE);
        }
    }
}
