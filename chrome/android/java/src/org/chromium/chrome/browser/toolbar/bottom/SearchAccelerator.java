// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.ThemeColorProvider.TintObserver;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider;
import org.chromium.chrome.browser.toolbar.IncognitoStateProvider.IncognitoStateObserver;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * The search accelerator.
 */
class SearchAccelerator extends ChromeImageButton
        implements ThemeColorObserver, TintObserver, IncognitoStateObserver {
    /** The gray pill background behind the search icon. */
    private final Drawable mBackground;

    /** The {@link Resources} used to compute the background color. */
    private final Resources mResources;

    /** A provider that notifies components when the theme color changes.*/
    private ThemeColorProvider mThemeColorProvider;

    /** A provider that notifies when incognito mode is entered or exited. */
    private IncognitoStateProvider mIncognitoStateProvider;

    /** The search accelerator text label. */
    private TextView mLabel;

    /** The wrapper View that contains the search accelerator and the label. */
    private View mWrapper;

    public SearchAccelerator(Context context, AttributeSet attrs) {
        super(context, attrs);

        mResources = context.getResources();

        mBackground = ApiCompatibilityUtils.getDrawable(mResources, R.drawable.ntp_search_box);
        mBackground.mutate();
        setBackground(mBackground);
    }

    /**
     * @param wrapper The wrapping View of this button.
     */
    public void setWrapperView(ViewGroup wrapper) {
        mWrapper = wrapper;
        mLabel = mWrapper.findViewById(R.id.search_accelerator_label);
        if (FeatureUtilities.isLabeledBottomToolbarEnabled()) {
            mLabel.setVisibility(View.VISIBLE);
        } else {
            mWrapper.setBackground(null);
        }
    }

    @Override
    public void setOnClickListener(OnClickListener listener) {
        if (mWrapper != null) {
            mWrapper.setOnClickListener(listener);
        } else {
            super.setOnClickListener(listener);
        }
    }

    void setThemeColorProvider(ThemeColorProvider themeColorProvider) {
        mThemeColorProvider = themeColorProvider;
        mThemeColorProvider.addThemeColorObserver(this);
        mThemeColorProvider.addTintObserver(this);
    }

    void setIncognitoStateProvider(IncognitoStateProvider provider) {
        mIncognitoStateProvider = provider;
        mIncognitoStateProvider.addIncognitoStateObserverAndTrigger(this);
    }

    void destroy() {
        if (mThemeColorProvider != null) {
            mThemeColorProvider.removeThemeColorObserver(this);
            mThemeColorProvider.removeTintObserver(this);
            mThemeColorProvider = null;
        }

        if (mIncognitoStateProvider != null) {
            mIncognitoStateProvider.removeObserver(this);
            mIncognitoStateProvider = null;
        }
    }

    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        updateBackground();
    }

    @Override
    public void onTintChanged(ColorStateList tint, boolean useLight) {
        ApiCompatibilityUtils.setImageTintList(this, tint);
        if (mLabel != null) mLabel.setTextColor(tint);
    }

    @Override
    public void onIncognitoStateChanged(boolean isIncognito) {
        updateBackground();
    }

    private void updateBackground() {
        if (mThemeColorProvider == null || mIncognitoStateProvider == null) return;

        mBackground.setColorFilter(ToolbarColors.getTextBoxColorForToolbarBackgroundInNonNativePage(
                                           mResources, mThemeColorProvider.getThemeColor(),
                                           mIncognitoStateProvider.isIncognitoSelected()),
                PorterDuff.Mode.SRC_IN);
    }
}
