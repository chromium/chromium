// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_tab_switcher;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.android.material.tabs.TabLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility_tab_switcher.AccessibilityTabModelAdapter.AccessibilityTabModelAdapterListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.widget.ChromeImageView;

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
    private ImageView mStandardButtonIcon;
    private ImageView mIncognitoButtonIcon;

    private ColorStateList mTabIconDarkColor;
    private ColorStateList mTabIconLightColor;
    private ColorStateList mTabIconSelectedDarkColor;
    private ColorStateList mTabIconSelectedLightColor;

    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver =
            new TabModelSelectorObserver() {
                @Override
                public void onChange() {
                    getAdapter().notifyDataSetChanged();
                    updateVisibilityForLayoutOrStackButton();
                }

                @Override
                public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                    getAdapter().notifyDataSetChanged();
                }
            };

    // TODO(bauerb): Use View#isAttachedToWindow() as soon as we are guaranteed
    // to run against API version 19.
    private boolean mIsAttachedToWindow;

    public AccessibilityTabModelWrapper(Context context) {
        super(context);
    }

    public AccessibilityTabModelWrapper(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public AccessibilityTabModelWrapper(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    /**
     * Initialize android views after creation.
     *
     * @param listener A {@link AccessibilityTabModelAdapterListener} to pass tab events back to the
     *                 parent.
     */
    public void setup(AccessibilityTabModelAdapterListener listener) {
        mTabIconDarkColor = AppCompatResources.getColorStateList(
                getContext(), R.color.default_icon_color_tint_list);
        mTabIconSelectedDarkColor = ColorStateList.valueOf(
                SemanticColorUtils.getDefaultControlColorActive(getContext()));
        mTabIconLightColor =
                AppCompatResources.getColorStateList(getContext(), R.color.white_alpha_70);
        mTabIconSelectedLightColor = AppCompatResources.getColorStateList(
                getContext(), R.color.default_icon_color_white_tint_list);
        // Setting scaleY here to make sure the icons are not flipped due to the scaleY of its
        // container layout.
        mStandardButtonIcon = new ChromeImageView(getContext());
        mStandardButtonIcon.setImageResource(R.drawable.btn_normal_tabs);
        mStandardButtonIcon.setScaleY(-1.0f);
        mStandardButtonIcon.setContentDescription(
                getResources().getString(R.string.accessibility_tab_switcher_standard_stack));
        mIncognitoButtonIcon = new ChromeImageView(getContext());
        mIncognitoButtonIcon.setImageResource(R.drawable.incognito_simple);
        mIncognitoButtonIcon.setScaleY(-1.0f);
        mIncognitoButtonIcon.setContentDescription(
                getResources().getString(R.string.accessibility_tab_switcher_incognito_stack));

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
            setBackgroundColor(getContext().getColor(R.color.default_bg_color_dark));
            mStackButtonWrapper.setSelectedTabIndicatorColor(
                    mTabIconSelectedLightColor.getDefaultColor());
            ApiCompatibilityUtils.setImageTintList(mStandardButtonIcon, mTabIconLightColor);
            ApiCompatibilityUtils.setImageTintList(
                    mIncognitoButtonIcon, mTabIconSelectedLightColor);
        } else {
            setBackgroundColor(SemanticColorUtils.getDefaultBgColor(getContext()));
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
                                R.string.accessibility_tab_switcher_incognito_stack)
                        : getContext().getString(
                                R.string.accessibility_tab_switcher_standard_stack));

        getAdapter().setTabModel(mTabModelSelector.getModel(incognitoSelected));
    }

    private AccessibilityTabModelAdapter getAdapter() {
        return (AccessibilityTabModelAdapter) mAccessibilityView.getAdapter();
    }

    /**
     * Scroll to and focus a tab.
     * @param tabId The id of the tab.
     */
    void scrollToTabAndFocus(int tabId) {
        final int index = TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), tabId);
        mAccessibilityView.smoothScrollToPosition(index);
        getAdapter().focusTabWithId(tabId);
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
                ? R.string.accessibility_tab_switcher_incognito_stack_selected
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
