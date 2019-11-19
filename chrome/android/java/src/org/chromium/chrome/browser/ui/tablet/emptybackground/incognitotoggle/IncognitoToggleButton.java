// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.tablet.emptybackground.incognitotoggle;

import android.content.Context;
import android.support.v7.content.res.AppCompatResources;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * A {@link View} that allows a user to toggle between incognito and normal {@link TabModel}s. This
 * class provides the basic functionality of drawing the button and toggling its state when the
 * TabModelSelector switches between normal and incognito modes. It can be subclassed (e.g. as is
 * done in IncognitoToggleButtonTablet) to add additional behaviors.
 */
public class IncognitoToggleButton extends ChromeImageButton {
    // TODO(crbug.com/843749): refactor this class so it doesn't need to hold a reference to
    // TabModelSelector.
    protected TabModelSelector mTabModelSelector;
    protected TabModelSelectorObserver mTabModelSelectorObserver;

    /**
     * Creates an instance of {@link IncognitoToggleButton}.
     * @param context The {@link Context} to create this {@link View} under.
     * @param attrs An {@link AttributeSet} that contains information on how to build this
     *         {@link View}.
     */
    public IncognitoToggleButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        setScaleType(ScaleType.CENTER);
    }

    /**
     * Sets the {@link TabModelSelector} that will be queried for information about the state of
     * the system.
     * @param selector A {@link TabModelSelector} that represents the state of the system.
     */
    public void setTabModelSelector(TabModelSelector selector) {
        mTabModelSelector = selector;
        if (selector != null) {
            updateButtonResource();

            mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
                @Override
                public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                    updateButtonResource();
                }
            };
            mTabModelSelector.addObserver(mTabModelSelectorObserver);
        }
    }

    /**
     * Set the image for the toggle button and set any necessary properties (e.g. tint).
     * @param isIncognitoSelected Whether incognito is currently selected.
     */
    protected void setImage(boolean isIncognitoSelected) {
        setImageResource(R.drawable.incognito_simple);
        ApiCompatibilityUtils.setImageTintList(this,
                AppCompatResources.getColorStateList(getContext(),
                        isIncognitoSelected ? R.color.white_mode_tint
                                            : R.color.standard_mode_tint));
    }

    private void updateButtonResource() {
        if (mTabModelSelector == null || mTabModelSelector.getCurrentModel() == null) return;

        @StringRes
        int resId = mTabModelSelector.isIncognitoSelected()
                ? R.string.accessibility_tabstrip_btn_incognito_toggle_incognito
                : R.string.accessibility_tabstrip_btn_incognito_toggle_standard;
        setContentDescription(getContext().getString(resId));
        setImage(mTabModelSelector.isIncognitoSelected());
    }
}
