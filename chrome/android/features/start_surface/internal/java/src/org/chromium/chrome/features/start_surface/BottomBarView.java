// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;
import android.support.design.widget.TabLayout;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.start_surface.R;
import org.chromium.ui.widget.ChromeImageView;

/** The bottom bar view. */
// TODO(crbug.com/982018): Support dark mode.
class BottomBarView extends FrameLayout {

    private TabLayout mTabLayout;
    private TabLayout.Tab mHomeTab;
    private TabLayout.Tab mExploreTab;
    private ChromeImageView mHomeButton;
    private TextView mHomeButtonLabel;
    private ChromeImageView mExploreButton;
    private TextView mExploreButtonLabel;
    private StartSurfaceProperties.BottomBarClickListener mOnClickListener;

    public BottomBarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTabLayout = (TabLayout) findViewById(R.id.bottom_tab_layout);
        mHomeTab = mTabLayout.getTabAt(0);
        mExploreTab = mTabLayout.getTabAt(1);

        mTabLayout.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                if (mOnClickListener == null) return;
                if (tab == mHomeTab) {
                    mOnClickListener.onHomeButtonClicked();
                    return;
                }
                if (tab == mExploreTab) {
                    mOnClickListener.onExploreButtonClicked();
                    return;
                }
                assert false : "Unsupported Tab.";
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {}

            @Override
            public void onTabReselected(TabLayout.Tab tab) {}
        });
    }

    /**
     * Set the visibility of this bottom bar.
     * @param shown Whether set the visibility to visible or not.
     */
    public void setVisibility(boolean shown) {
        setVisibility(shown ? View.VISIBLE : View.INVISIBLE);
    }

    /**
     * Set the {@link StartSurfaceProperties.BottomBarClickListener}.
     * @param listener Listen clicks.
     */
    public void setOnClickListener(StartSurfaceProperties.BottomBarClickListener listener) {
        mOnClickListener = listener;
    }

    /**
     * Select the Tab at the specified position if needed.
     * @param index The specified position.
     */
    public void selectTabAt(int index) {
        assert index >= 0 && index < mTabLayout.getTabCount();

        if (index == mTabLayout.getSelectedTabPosition()) return;
        mTabLayout.getTabAt(index).select();
    }
}
