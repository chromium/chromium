// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
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
import androidx.core.util.Pair;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.listmenu.ListMenuButton;

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
        String userTitle = titleData.first;
        if (TextUtils.isEmpty(userTitle)) {
            mTitleTextView.setText(
                    TabGroupTitleEditor.getDefaultTitle(getContext(), titleData.second));
        } else {
            mTitleTextView.setText(userTitle);
        }
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

    void setOpenRunnable(@Nullable Runnable openRunnable) {
        // TODO(b:324934166): Adjust the more button menu.
        setOnClickListener(openRunnable == null ? null : v -> openRunnable.run());
    }

    void setDeleteRunnable(@Nullable Runnable deleteRunnable) {
        // TODO(b:324934166): Adjust the more button menu.
    }

    private TabGroupFaviconQuarter getTabGroupFaviconQuarter(@Corner int corner) {
        return (TabGroupFaviconQuarter) mTabGroupStartIconParent.getChildAt(corner);
    }

    void setTimeAgoResolverForTesting(TabGroupTimeAgoResolver timeAgoResolver) {
        mTimeAgoResolver = timeAgoResolver;
    }
}
