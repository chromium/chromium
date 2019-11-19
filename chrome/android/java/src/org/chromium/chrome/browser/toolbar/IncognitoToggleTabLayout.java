// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.design.widget.TabLayout;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.TabCountProvider.TabCountObserver;
import org.chromium.ui.widget.ChromeImageView;

/**
 * TabLayout shown in the Horizontal Tab Switcher.
 */
public class IncognitoToggleTabLayout extends TabLayout implements TabCountObserver {
    private TabLayout.Tab mStandardButton;
    private TabLayout.Tab mIncognitoButton;
    private ImageView mStandardButtonIcon;
    private ImageView mIncognitoButtonIcon;
    private TabSwitcherDrawable mTabSwitcherDrawable;

    private ColorStateList mTabIconDarkColor;
    private ColorStateList mTabIconLightColor;
    private ColorStateList mTabIconSelectedDarkColor;
    private ColorStateList mTabIconSelectedLightColor;

    private TabModelSelector mTabModelSelector;
    private TabCountProvider mTabCountProvider;
    private TabModelSelectorObserver mTabModelSelectorObserver;

    /**
     * Constructor for inflating from XML.
     */
    public IncognitoToggleTabLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        mTabIconDarkColor =
                AppCompatResources.getColorStateList(getContext(), R.color.standard_mode_tint);
        mTabIconSelectedDarkColor =
                AppCompatResources.getColorStateList(getContext(), R.color.light_active_color);
        mTabIconLightColor =
                AppCompatResources.getColorStateList(getContext(), R.color.white_alpha_70);
        mTabIconSelectedLightColor =
                AppCompatResources.getColorStateList(getContext(), R.color.white_mode_tint);

        mStandardButtonIcon = new ChromeImageView(getContext());
        mTabSwitcherDrawable = TabSwitcherDrawable.createTabSwitcherDrawable(getContext(), false);
        mStandardButtonIcon.setImageDrawable(mTabSwitcherDrawable);
        mStandardButtonIcon.setContentDescription(
                getResources().getString(R.string.accessibility_tab_switcher_standard_stack));
        mIncognitoButtonIcon = new ChromeImageView(getContext());
        mIncognitoButtonIcon.setImageResource(R.drawable.incognito_small);
        mIncognitoButtonIcon.setContentDescription(
                getResources().getString(R.string.accessibility_tab_switcher_incognito_stack));

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
        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                setStateBasedOnModel();
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        setStateBasedOnModel();

        assert mTabCountProvider != null;
        mTabSwitcherDrawable.updateForTabCount(mTabCountProvider.getTabCount(false), false);
    }

    public void setTabCountProvider(TabCountProvider tabCountProvider) {
        mTabCountProvider = tabCountProvider;
        mTabCountProvider.addObserverAndTrigger(this);
    }

    /**
     * Update the visual state based on number of normal (non-incognito) tabs present.
     * @param tabCount The number of normal tabs.
     */
    @Override
    public void onTabCountChanged(int tabCount, boolean isIncognito) {
        if (!isIncognito) mTabSwitcherDrawable.updateForTabCount(tabCount, isIncognito);
    }

    public void destroy() {
        if (mTabModelSelector != null) mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        if (mTabCountProvider != null) mTabCountProvider.removeObserver(this);
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
                ? R.string.accessibility_tab_switcher_incognito_stack_selected
                : R.string.accessibility_tab_switcher_standard_stack_selected;
        announceForAccessibility(getResources().getString(stackAnnouncementId));
    }
}
