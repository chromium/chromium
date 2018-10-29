// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.chromium.ui.base.LocalizationUtils.isLayoutRtl;

import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.support.annotation.StringRes;
import android.support.design.widget.TabLayout;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.view.ViewPager;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.LinearLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;

/**
 * The Accessory sitting above the keyboard and below the content area. It is used for autofill
 * suggestions and manual entry points assisting the user in filling forms.
 */
class KeyboardAccessoryView extends LinearLayout {
    private RecyclerView mActionsView;
    private TabLayout mTabLayout;
    private TabLayout.TabLayoutOnPageChangeListener mPageChangeListener;

    private static class HorizontalDividerItemDecoration extends RecyclerView.ItemDecoration {
        private final int mHorizontalMargin;
        HorizontalDividerItemDecoration(int horizontalMargin) {
            this.mHorizontalMargin = horizontalMargin;
        }
        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            outRect.right = mHorizontalMargin;
        }
    }

    /**
     * Constructor for inflating from XML.
     */
    public KeyboardAccessoryView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);

        mActionsView = findViewById(R.id.actions_view);
        initializeHorizontalRecyclerView(mActionsView);

        mTabLayout = findViewById(R.id.tabs);

        // Apply RTL layout changes to the views children:
        ApiCompatibilityUtils.setLayoutDirection(mActionsView,
                isLayoutRtl() ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);

        // Set listener's to touch/click events so they are not propagated to the page below.
        setOnTouchListener((view, motionEvent) -> {
            performClick(); // Setting a touch listener requires this call which is a NoOp.
            return true; // Return that the motionEvent was consumed and needs no further handling.
        });
        setOnClickListener(view -> {});
        setClickable(false); // Disables the "Double-tap to activate" Talkback reading.
        setSoundEffectsEnabled(false);
    }

    void setVisible(boolean visible) {
        if (visible) {
            show();
        } else {
            hide();
        }
    }

    public void setBottomOffset(int bottomOffset) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        params.setMargins(params.leftMargin, params.topMargin, params.rightMargin, bottomOffset);
        setLayoutParams(params);
    }

    void setActionsAdapter(RecyclerView.Adapter adapter) {
        mActionsView.setAdapter(adapter);
    }

    /**
     * Creates a new tab and appends it to the end of the tab layout at the start of the bar.
     * @param icon The icon to be displayed in the tab bar.
     * @param contentDescription The contentDescription to be used for the tab icon.
     */
    void addTabAt(int position, Drawable icon, CharSequence contentDescription) {
        if (mTabLayout == null) return; // Inflation not done yet. Will be invoked again afterwards.
        TabLayout.Tab tab = mTabLayout.newTab();
        tab.setIcon(icon.mutate()); // mutate() needed to change the active tint.
        DrawableCompat.setTint(tab.getIcon(), getResources().getColor(R.color.default_icon_color));
        tab.setContentDescription(contentDescription);
        mTabLayout.addTab(tab, position, false);
    }

    void removeTabAt(int position) {
        if (mTabLayout == null) return; // Inflation not done yet. Will be invoked again afterwards.
        TabLayout.Tab tab = mTabLayout.getTabAt(position);
        if (tab == null) return; // The tab was already removed.
        mTabLayout.removeTab(tab);
    }

    /**
     * Removes all tabs.
     */
    void clearTabs() {
        if (mTabLayout == null) return; // Inflation not done yet. Will be invoked again afterwards.
        mTabLayout.removeAllTabs();
    }

    ViewPager.OnPageChangeListener getPageChangeListener() {
        if (mPageChangeListener == null) {
            mPageChangeListener = new TabLayout.TabLayoutOnPageChangeListener(mTabLayout);
        }
        return mPageChangeListener;
    }

    void setTabSelectionAdapter(TabLayout.OnTabSelectedListener tabSelectionCallbacks) {
        mTabLayout.clearOnTabSelectedListeners();
        mTabLayout.addOnTabSelectedListener(tabSelectionCallbacks);
    }

    void setActiveTabColor(Integer activeTab) {
        for (int i = mTabLayout.getTabCount() - 1; i >= 0; i--) {
            TabLayout.Tab t = mTabLayout.getTabAt(i);
            if (t == null || t.getIcon() == null) continue;
            int activeStateColor = (activeTab == null || i != activeTab)
                    ? R.color.default_icon_color
                    : R.color.default_icon_color_blue;
            DrawableCompat.setTint(t.getIcon(), getResources().getColor(activeStateColor));
        }
    }

    public void setTabDescription(int i, String description) {
        TabLayout.Tab tab = mTabLayout.getTabAt(i);
        if (tab != null) tab.setContentDescription(description);
    }

    public void setTabDescription(int i, @StringRes int messageId) {
        TabLayout.Tab tab = mTabLayout.getTabAt(i);
        if (tab != null) tab.setContentDescription(messageId);
    }

    private void show() {
        bringToFront(); // Needs to overlay every component and the bottom sheet - like a keyboard.
        setVisibility(View.VISIBLE);
        announceForAccessibility(getContentDescription());
    }

    private void hide() {
        setVisibility(View.GONE);
    }

    private void initializeHorizontalRecyclerView(RecyclerView recyclerView) {
        // Set horizontal layout.
        recyclerView.setLayoutManager(
                new LinearLayoutManager(getContext(), LinearLayoutManager.HORIZONTAL, false));

        int pad = getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_padding);
        // Create margins between every element.
        recyclerView.addItemDecoration(new HorizontalDividerItemDecoration(pad));

        // Remove all animations - the accessory shouldn't be visibly built anyway.
        recyclerView.setItemAnimator(null);

        recyclerView.setPadding(isLayoutRtl() ? 0 : pad, 0, isLayoutRtl() ? pad : 0, 0);
    }
}
