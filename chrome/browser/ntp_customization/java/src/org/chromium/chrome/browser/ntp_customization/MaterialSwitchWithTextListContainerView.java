// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CompoundButton;
import android.widget.LinearLayout;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

import java.util.List;

/** The view which holds a list of items in the "New tab page cards" bottom sheet. */
@NullMarked
public class MaterialSwitchWithTextListContainerView extends LinearLayout
        implements ListContainerView {

    public MaterialSwitchWithTextListContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Adds list items views to this container view.
     *
     * @param delegate The delegate contains the content for each list item view.
     */
    @Override
    public void renderAllListItems(ListContainerViewDelegate delegate) {
        List<Integer> types = delegate.getListItems();

        for (int i = 0; i < types.size(); i++) {
            Integer type = types.get(i);
            MaterialSwitchWithText listItemView = (MaterialSwitchWithText) createListItemView();

            listItemView.setText(delegate.getListItemTitle(type, getContext()));
            listItemView.setBackground(
                    AppCompatResources.getDrawable(
                            getContext(), NtpCustomizationUtils.getBackground(types.size(), i)));
            listItemView.setChecked(delegate.isListItemChecked(type));
            listItemView.setOnCheckedChangeListener(delegate.getOnCheckedChangeListener(type));

            addView(listItemView);
        }
    }

    /** Returns a view representing a single list item in this container. */
    @VisibleForTesting
    View createListItemView() {
        return LayoutInflater.from(getContext())
                .inflate(R.layout.ntp_customization_ntp_cards_list_item_layout, this, false);
    }

    /** Enables/disables interactivity of the module switches. */
    public void setAllModuleSwitchesEnabled(boolean isEnabled) {
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child instanceof MaterialSwitchWithText childSwitch) {
                childSwitch.setEnabled(isEnabled);
            }
        }
    }

    /**
     * Clear {@link CompoundButton.OnCheckedChangeListener} of every list item view inside this
     * container view.
     */
    @Override
    public void destroy() {
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child instanceof MaterialSwitchWithText childSwitch) {
                childSwitch.setOnCheckedChangeListener(null);
            }
        }

        removeAllViews();
    }
}
