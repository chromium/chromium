// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.accessibility;

import android.content.Context;
import android.content.res.ColorStateList;
import android.support.design.widget.TabLayout;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.AppCompatImageView;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ListView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.widget.accessibility.AccessibilityTabModelAdapter.AccessibilityTabModelAdapterListener;

/**
 * A wrapper around the Android views in the Accessibility tab switcher. This
 * will show two {@link ListView}s, one for each
 * {@link org.chromium.chrome.browser.tabmodel.TabModel} to
 * represent.
 */
public class AccessibilityTabModelWrapper extends LinearLayout {
    private AccessibilityTabModelListView mAccessibilityView;
    private View mLayout;
    private TabLayout mStackButtonWrapper;
    private TabLayout.Tab mStandardButton;
    private TabLayout.Tab mIncognitoButton;
    private AppCompatImageView mStandardButtonIcon;
    private AppCompatImageView mIncognitoButtonIcon;

    private ColorStateList mTabIconDarkColor;
    private ColorStateList mTabIconLightColor;
    private ColorStateList mTabIconSelectedDarkColor;
    private ColorStateList mTabIconSelectedLightColor;

    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver =
            new EmptyTabModelSelectorObserver() {
        @Override
        public void onChange() {
            getAdapter().notifyDataSetChanged();
            updateVisibilityForLayoutOrStackButton();
        }

        @Override
        public void onNewTabCreated(Tab tab) {
            getAdapter().notifyDataSetChanged();
        }
    };

    // TODO(bauerb): Use View#isAttachedToWindow() as soon as we are guaranteed
    // to run against API version 19.
    private boolean mIsAttachedToWindow;

    private class ButtonOnClickListener implements View.OnClickListener {
        private final boolean mIncognito;

        public ButtonOnClickListener(boolean incognito) {
            mIncognito = incognito;
        }

        @Override
        public void onClick(View v) {
            setSelectedModel(mIncognito);
        }
    }

    public AccessibilityTabModelWrapper(Context context) {
        super(context);
    }

    public AccessibilityTabModelWrapper(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public AccessibilityTabModelWrapper(Context context, AttributeSet attrs,
            int defStyle) {
        super(context, attrs, defStyle);
    }

    /**
     * Initialize android views after creation.
     *
     * @param listener A {@link AccessibilityTabModelAdapterListener} to pass tab events back to the
     *                 parent.
     */
    public void setup(AccessibilityTabModelAdapterListener listener) {
        mTabIconDarkColor =
                AppCompatResources.getColorStateList(getContext(), R.color.dark_mode_tint);
        mTabIconSelectedDarkColor =
                AppCompatResources.getColorStateList(getContext(), R.color.light_active_color);
        mTabIconLightColor =
                AppCompatResources.getColorStateList(getContext(), R.color.white_alpha_70);
        mTabIconSelectedLightColor =
                AppCompatResources.getColorStateList(getContext(), R.color.white_mode_tint);
        // Setting scaleY here to make sure the icons are not flipped due to the scaleY of its
        // container layout.
        mStandardButtonIcon = new AppCompatImageView(getContext());
        mStandardButtonIcon.setImageResource(R.drawable.btn_normal_tabs);
        mStandardButtonIcon.setScaleY(-1.0f);
        mStandardButtonIcon.setContentDescription(
                getResources().getString(R.string.accessibility_tab_switcher_standard_stack));
        mIncognitoButtonIcon = new AppCompatImageView(getContext());
        mIncognitoButtonIcon.setImageResource(R.drawable.btn_incognito_tabs);
        mIncognitoButtonIcon.setScaleY(-1.0f);
        mIncognitoButtonIcon.setContentDescription(getResources().getString(
                ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_STRINGS)
                        ? R.string.accessibility_tab_switcher_private_stack
                        : R.string.accessibility_tab_switcher_incognito_stack));

        setDividerDrawable(null);
        ((ListView) findViewById(R.id.list_view)).setDivider(null);

        mLayout = findViewById(R.id.tab_wrapper);
        mStackButtonWrapper = findViewById(R.id.tab_layout);
        mStandardButton = mStackButtonWrapper.newTab().setCustomView(mStandardButtonIcon);
        mStackButtonWrapper.addTab(mStandardButton);
        mIncognitoButton = mStackButtonWrapper.newTab().setCustomView(mIncognitoButtonIcon);
        mStackButtonWrapper.addTab(mIncognitoButton);
        mStackButtonWrapper.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                setSelectedModel(mIncognitoButton.isSelected());
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {}

            @Override
            public void onTabReselected(TabLayout.Tab tab) {}
        });

        mAccessibilityView = (AccessibilityTabModelListView) findViewById(R.id.list_view);

        AccessibilityTabModelAdapter adapter = getAdapter();

        adapter.setListener(listener);
    }

    /**
     * @param modelSelector A {@link TabModelSelector} to provide information
     *            about open tabs.
     */
    public void setTabModelSelector(TabModelSelector modelSelector) {
        if (mIsAttachedToWindow) {
            mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        }
        mTabModelSelector = modelSelector;
        if (mIsAttachedToWindow) {
            modelSelector.addObserver(mTabModelSelectorObserver);
        }
        setStateBasedOnModel();
    }

    /**
     * Set the bottom model selector buttons and list view contents based on the
     * TabModelSelector.
     */
    public void setStateBasedOnModel() {
        if (mTabModelSelector == null) return;

        boolean incognitoSelected = mTabModelSelector.isIncognitoSelected();

        updateVisibilityForLayoutOrStackButton();
        if (incognitoSelected) {
            setBackgroundColor(ApiCompatibilityUtils.getColor(
                    getResources(), R.color.incognito_modern_primary_color));
            mStackButtonWrapper.setSelectedTabIndicatorColor(
                    mTabIconSelectedLightColor.getDefaultColor());
            ApiCompatibilityUtils.setImageTintList(mStandardButtonIcon, mTabIconLightColor);
            ApiCompatibilityUtils.setImageTintList(
                    mIncognitoButtonIcon, mTabIconSelectedLightColor);
        } else {
            setBackgroundColor(
                    ApiCompatibilityUtils.getColor(getResources(), R.color.modern_primary_color));
            mStackButtonWrapper.setSelectedTabIndicatorColor(
                    mTabIconSelectedDarkColor.getDefaultColor());
            ApiCompatibilityUtils.setImageTintList(mStandardButtonIcon, mTabIconSelectedDarkColor);
            ApiCompatibilityUtils.setImageTintList(mIncognitoButtonIcon, mTabIconDarkColor);
        }
        // Ensure the tab in tab layout is correctly selected when tab switcher is
        // first opened.
        if (incognitoSelected && !mIncognitoButton.isSelected()) {
            mIncognitoButton.select();
        } else if (!incognitoSelected && !mStandardButton.isSelected()) {
            mStandardButton.select();
        }

        mAccessibilityView.setContentDescription(incognitoSelected
                        ? getContext().getString(
                                  ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_STRINGS)
                                          ? R.string.accessibility_tab_switcher_private_stack
                                          : R.string.accessibility_tab_switcher_incognito_stack)
                        : getContext().getString(
                                  R.string.accessibility_tab_switcher_standard_stack));

        getAdapter().setTabModel(mTabModelSelector.getModel(incognitoSelected));
    }

    private AccessibilityTabModelAdapter getAdapter() {
        return (AccessibilityTabModelAdapter) mAccessibilityView.getAdapter();
    }

    /**
     * Set either standard or incognito tab model as currently selected.
     * @param incognitoSelected Whether the incognito tab model is selected.
     */
    private void setSelectedModel(boolean incognitoSelected) {
        if (mTabModelSelector == null
                || incognitoSelected == mTabModelSelector.isIncognitoSelected()) {
            return;
        }

        mTabModelSelector.commitAllTabClosures();
        mTabModelSelector.selectModel(incognitoSelected);
        setStateBasedOnModel();

        int stackAnnouncementId = incognitoSelected
                ? (ChromeFeatureList.isEnabled(ChromeFeatureList.INCOGNITO_STRINGS)
                                  ? R.string.accessibility_tab_switcher_private_stack_selected
                                  : R.string.accessibility_tab_switcher_incognito_stack_selected)
                : R.string.accessibility_tab_switcher_standard_stack_selected;
        AccessibilityTabModelWrapper.this.announceForAccessibility(
                getResources().getString(stackAnnouncementId));
    }

    private void updateVisibilityForLayoutOrStackButton() {
        boolean incognitoEnabled =
                mTabModelSelector.getModel(true).getComprehensiveModel().getCount() > 0;
        mLayout.setVisibility(incognitoEnabled ? View.VISIBLE : View.GONE);
    }

    @Override
    protected void onAttachedToWindow() {
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
        mIsAttachedToWindow = true;
        super.onAttachedToWindow();
    }

    @Override
    protected void onDetachedFromWindow() {
        mIsAttachedToWindow = false;
        super.onDetachedFromWindow();
    }

    @VisibleForTesting
    public TabLayout.Tab getIncognitoTabsButton() {
        return mIncognitoButton;
    }

    @VisibleForTesting
    public TabLayout.Tab getStandardTabsButton() {
        return mStandardButton;
    }
}
