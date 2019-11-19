// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.FeatureUtilities;

/**
 * The Button used for switching tabs. Currently this class is only being used for the bottom
 * toolbar tab switcher button.
 */
public class TabSwitcherButtonView extends ImageView {
    /**
     * A drawable for the tab switcher icon.
     */
    private TabSwitcherDrawable mTabSwitcherButtonDrawable;

    /** The tab switcher button text label. */
    private TextView mLabel;

    /** The wrapper View that contains the tab switcher button and the label. */
    private View mWrapper;

    public TabSwitcherButtonView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * @param wrapper The wrapping View of this button.
     */
    public void setWrapperView(ViewGroup wrapper) {
        mWrapper = wrapper;
        mLabel = mWrapper.findViewById(R.id.tab_switcher_button_label);
        if (FeatureUtilities.isLabeledBottomToolbarEnabled()) mLabel.setVisibility(View.VISIBLE);
    }

    @Override
    public void setOnClickListener(OnClickListener listener) {
        if (mWrapper != null) {
            mWrapper.setOnClickListener(listener);
            setClickable(false);
        }
        super.setOnClickListener(listener);
    }

    @Override
    public void setOnLongClickListener(OnLongClickListener listener) {
        if (mWrapper != null) {
            mWrapper.setOnLongClickListener(listener);
            setClickable(false);
        }
        super.setOnLongClickListener(listener);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTabSwitcherButtonDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(getContext(), false);
        setImageDrawable(mTabSwitcherButtonDrawable);
    }

    /**
     * @param numberOfTabs The number of open tabs.
     */
    public void updateTabCountVisuals(int numberOfTabs) {
        setEnabled(numberOfTabs >= 1);
        setContentDescription(getResources().getQuantityString(
                R.plurals.accessibility_toolbar_btn_tabswitcher_toggle, numberOfTabs,
                numberOfTabs));
        mTabSwitcherButtonDrawable.updateForTabCount(numberOfTabs, false);
    }

    /**
     * @param tint The {@ColorStateList} used to tint the button.
     */
    public void setTint(ColorStateList tint) {
        mTabSwitcherButtonDrawable.setTint(tint);
        if (mLabel != null) mLabel.setTextColor(tint);
    }
}
