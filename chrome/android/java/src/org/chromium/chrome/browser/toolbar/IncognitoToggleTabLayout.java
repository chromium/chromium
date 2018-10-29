// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.design.widget.TabLayout;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageView;
import android.util.AttributeSet;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * TabLayout shown in the Horizontal Tab Switcher.
 */
public class IncognitoToggleTabLayout extends TabLayout {
    private TabLayout.Tab mStandardButton;
    private TabLayout.Tab mIncognitoButton;
    private AppCompatImageView mStandardButtonIcon;
    private AppCompatImageView mIncognitoButtonIcon;
    private TabSwitcherDrawable mTabSwitcherDrawable;

    private ColorStateList mTabIconDarkColor;
    private ColorStateList mTabIconLightColor;
    private ColorStateList mTabIconSelectedDarkColor;
    private ColorStateList mTabIconSelectedLightColor;

    private TabModelSelector mTabModelSelector;

    /**
     * Constructor for inflating from XML.
     */
    public IncognitoToggleTabLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTabIconDarkColor =
                AppCompatResources.getColorStateList(getContext(), R.color.dark_mode_tint);
        mTabIconSelectedDarkColor =
                AppCompatResources.getColorStateList(getContext(), R.color.light_active_color);
        mTabIconLightColor =
                AppCompatResources.getColorStateList(getContext(), R.color.white_alpha_70);
        mTabIconSelectedLightColor =
                AppCompatResources.getColorStateList(getContext(), R.color.white_mode_tint);

        mStandardButtonIcon = new AppCompatImageView(getContext());
        mTabSwitcherDrawable = TabSwitcherDrawable.createTabSwitcherDrawable(getContext(), false);
        mStandardButtonIcon.setImageDrawable(mTabSwitcherDrawable);
        mStandardButtonIcon.setContentDescription(
                getResources().getString(R.string.accessibility_tab_switcher_standard_stack));
        mIncognitoButtonIcon = new AppCompatImageView(getContext());
        mIncognitoButtonIcon.setImageResource(R.drawable.incognito_small);
        mIncognitoButtonIcon.setContentDescription(getResources().getString(
                ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_STRINGS)
                        ? R.string.accessibility_tab_switcher_private_stack
                        : R.string.accessibility_tab_switcher_incognito_stack));

        mStandardButton = newTab().setCustomView(mStandardButtonIcon);
        addTab(mStandardButton);
        mIncognitoButton = newTab().setCustomView(mIncognitoButtonIcon);
        addTab(mIncognitoButton);

        addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                setSelectedModel(mIncognitoButton.isSelected());
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {}

            @Override
            public void onTabReselected(TabLayout.Tab tab) {}
        });
    }

    /**
     * @param selector A {@link TabModelSelector} to provide information about open tabs.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (mTabModelSelector == null) return;
        mTabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                setStateBasedOnModel();
            }
        });
        setStateBasedOnModel();
    }

    /**
     * Update the visual state based on number of normal (non-incognito) tabs present.
     * @param tabCount The number of normal tabs.
     */
    public void updateTabCount(int tabCount) {
        mTabSwitcherDrawable.updateForTabCount(tabCount, false);
    }

    private void setStateBasedOnModel() {
        if (mTabModelSelector == null) return;
        final boolean isIncognitoSelected = mTabModelSelector.isIncognitoSelected();
        if (isIncognitoSelected) {
            setSelectedTabIndicatorColor(mTabIconSelectedLightColor.getDefaultColor());
            ApiCompatibilityUtils.setImageTintList(mStandardButtonIcon, mTabIconLightColor);
            mTabSwitcherDrawable.setTint(mTabIconLightColor);
            ApiCompatibilityUtils.setImageTintList(
                    mIncognitoButtonIcon, mTabIconSelectedLightColor);
        } else {
            setSelectedTabIndicatorColor(mTabIconSelectedDarkColor.getDefaultColor());
            ApiCompatibilityUtils.setImageTintList(mStandardButtonIcon, mTabIconSelectedDarkColor);
            mTabSwitcherDrawable.setTint(mTabIconSelectedDarkColor);
            ApiCompatibilityUtils.setImageTintList(mIncognitoButtonIcon, mTabIconDarkColor);
        }
        // Ensure the tab in tab layout is correctly selected when tab switcher is
        // first opened.
        if (isIncognitoSelected && !mIncognitoButton.isSelected()) {
            mIncognitoButton.select();
        } else if (!isIncognitoSelected && !mStandardButton.isSelected()) {
            mStandardButton.select();
        }
    }

    private void setSelectedModel(boolean incognitoSelected) {
        if (mTabModelSelector == null
                || incognitoSelected == mTabModelSelector.isIncognitoSelected()) {
            return;
        }

        mTabModelSelector.commitAllTabClosures();
        mTabModelSelector.selectModel(incognitoSelected);

        final int stackAnnouncementId = incognitoSelected
                ? (ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_STRINGS)
                                  ? R.string.accessibility_tab_switcher_private_stack_selected
                                  : R.string.accessibility_tab_switcher_incognito_stack_selected)
                : R.string.accessibility_tab_switcher_standard_stack_selected;
        announceForAccessibility(getResources().getString(stackAnnouncementId));
    }
}
