// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sections;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import com.google.android.material.tabs.TabLayout;

import org.chromium.chrome.browser.feed.R;

/** View for the feed header that sticks to the top of the screen upon scroll. */
public class StickySectionHeaderView extends SectionHeaderView {
    private @Nullable View mOptionsPanel;

    public StickySectionHeaderView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the sticky header’s visibility and update the tab state.
     * The sticky header will be visible only when users scroll the real header above the toolbar.
     */
    @Override
    void setStickyHeaderVisible(boolean isVisible) {
        updateTabState();
        this.setVisibility(isVisible ? VISIBLE : GONE);
    }

    /**
     * This method is to set/update the sticky header's margin.
     * @param marginValue the sticky header's margin
     */
    @Override
    void updateStickyHeaderMargin(int marginValue) {
        ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) this.getLayoutParams();
        params.setMargins(params.leftMargin, marginValue, params.rightMargin, params.bottomMargin);
    }

    /** This method update the tabs state, recalculating the unread indicator position. */
    private void updateTabState() {
        TabLayout tabLayout = findViewById(R.id.tab_list_view);
        for (int i = 0; i < tabLayout.getTabCount(); i++) {
            applyTabState(tabLayout.getTabAt(i));
        }
    }

    @Override
    void setOptionsPanel(View optionsView) {}

    @Override
    void setStickyHeaderOptionsPanel(View optionsView) {
        if (mOptionsPanel != null) {
            removeView(mOptionsPanel);
        }
        // Get the sticky header hairline index, and add the options view above the hairline so that
        // we can keep the hairline as the last child in the sticky header view.
        int index = indexOfChild(findViewById(R.id.sticky_header_hairline));
        addView(optionsView, index);
        mOptionsPanel = optionsView;
    }
}
