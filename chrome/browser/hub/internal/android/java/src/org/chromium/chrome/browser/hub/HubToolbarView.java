// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.widget.TextViewCompat;

import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayout.OnTabSelectedListener;
import com.google.android.material.tabs.TabLayout.Tab;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;

import java.util.List;

/** Toolbar for the Hub. May contain a single or multiple rows, of which this view is the parent. */
public class HubToolbarView extends LinearLayout {
    private Button mActionButton;
    private TabLayout mPaneSwitcher;
    private FrameLayout mMenuButtonContainer;
    private OnTabSelectedListener mOnTabSelectedListener;
    private boolean mBlockTabSelectionCallback;

    /** Default {@link LinearLayout} constructor called by inflation. */
    public HubToolbarView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mActionButton = findViewById(R.id.toolbar_action_button);
        mPaneSwitcher = findViewById(R.id.pane_switcher);
        mMenuButtonContainer = findViewById(R.id.menu_button_container);
    }

    void setMenuButtonVisible(boolean visible) {
        mMenuButtonContainer.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    void setActionButton(@Nullable FullButtonData buttonData, boolean showText) {
        ApplyButtonData.apply(buttonData, mActionButton);
        if (!showText) {
            mActionButton.setText(null);
        }
    }

    void setPaneSwitcherButtonData(
            @Nullable List<FullButtonData> buttonDataList, int selectedIndex) {
        mPaneSwitcher.removeOnTabSelectedListener(mOnTabSelectedListener);
        mPaneSwitcher.removeAllTabs();

        if (buttonDataList == null || buttonDataList.size() <= 1) {
            mPaneSwitcher.setVisibility(View.GONE);
            mOnTabSelectedListener = null;
        } else {
            Context context = getContext();
            for (FullButtonData buttonData : buttonDataList) {
                Tab tab = mPaneSwitcher.newTab();

                // TODO(crbug.com/40286849): Conditionally use text instead.
                Drawable drawable = buttonData.resolveIcon(context);
                tab.setIcon(drawable);
                tab.setContentDescription(buttonData.resolveContentDescription(context));
                mPaneSwitcher.addTab(tab);
            }
            mPaneSwitcher.setVisibility(View.VISIBLE);
            mOnTabSelectedListener = makeTabSelectedListener(buttonDataList);
            mPaneSwitcher.addOnTabSelectedListener(mOnTabSelectedListener);
        }

        setPaneSwitcherIndex(selectedIndex);
    }

    void setPaneSwitcherIndex(int index) {
        @Nullable Tab tab = mPaneSwitcher.getTabAt(index);
        if (tab == null) return;

        // Setting the selected tab should never trigger the callback.
        mBlockTabSelectionCallback = true;
        tab.select();
        mBlockTabSelectionCallback = false;
    }

    void setColorScheme(@HubColorScheme int colorScheme) {
        Context context = getContext();
        setBackgroundColor(HubColors.getBackgroundColor(context, colorScheme));
        ColorStateList iconColor = HubColors.getIconColor(context, colorScheme);
        @ColorInt int selectedIconColor = HubColors.getSelectedIconColor(context, colorScheme);
        TextViewCompat.setCompoundDrawableTintList(mActionButton, iconColor);
        mPaneSwitcher.setTabIconTint(
                HubColors.getSelectableIconList(selectedIconColor, iconColor.getDefaultColor()));
        mPaneSwitcher.setSelectedTabIndicatorColor(selectedIconColor);

        // TODO(crbug.com/40948541): Updating the app menu color here is more correct and
        // should be done for code health.
    }

    void setButtonLookupConsumer(Callback<PaneButtonLookup> lookupConsumer) {
        lookupConsumer.onResult(this::getButtonView);
    }

    private View getButtonView(int index) {
        @Nullable Tab tab = mPaneSwitcher.getTabAt(index);
        return tab == null ? null : tab.view;
    }

    private OnTabSelectedListener makeTabSelectedListener(
            @NonNull List<FullButtonData> buttonDataList) {
        return new OnTabSelectedListener() {
            @Override
            public void onTabSelected(Tab tab) {
                if (!mBlockTabSelectionCallback) {
                    buttonDataList.get(tab.getPosition()).getOnPressRunnable().run();
                }
            }

            @Override
            public void onTabUnselected(Tab tab) {}

            @Override
            public void onTabReselected(Tab tab) {}
        };
    }
}
