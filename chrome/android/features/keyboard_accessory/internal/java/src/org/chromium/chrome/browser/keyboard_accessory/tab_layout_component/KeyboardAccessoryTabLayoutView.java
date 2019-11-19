// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.tab_layout_component;

import android.content.Context;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.support.design.widget.TabLayout;
import android.util.AttributeSet;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.keyboard_accessory.R;

/**
 * A {@link TabLayout} containing the tabs in the keyboard accessory.
 */
class KeyboardAccessoryTabLayoutView extends TabLayout {
    /**
     * Constructor for inflating from XML.
     */
    public KeyboardAccessoryTabLayoutView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Creates a new tab and appends it to the end of the tab layout at the start of the bar.
     * @param icon The icon to be displayed in the tab bar.
     * @param contentDescription The contentDescription to be used for the tab icon.
     */
    void addTabAt(int position, Drawable icon, CharSequence contentDescription) {
        TabLayout.Tab tab = newTab();
        tab.setIcon(icon.mutate()); // mutate() needed to change the active tint.
        tab.getIcon().setColorFilter(
                ApiCompatibilityUtils.getColor(getResources(), R.color.default_icon_color),
                PorterDuff.Mode.SRC_IN);
        tab.setContentDescription(contentDescription);
        addTab(tab, position, false);
    }

    /**
     * Removes the tab at the given position if it exists. If this isn't possible, it usually means
     * the call was attempted before inflation (this is usually handled by the lazyMCP flushing all
     * changes after inflation -- no further action needed).
     * @param position The position of the tab to remove.
     */
    void tryToRemoveTabAt(int position) {
        TabLayout.Tab tab = getTabAt(position);
        if (tab == null) return; // The tab was already removed.
        removeTab(tab);
    }

    /**
     * This layout shouldn't have too many listeners so prefer this method over
     * {@link #addOnTabSelectedListener(OnTabSelectedListener)} in order to keep only the latest
     * listener.
     * @param tabSelectionCallbacks A {@link OnTabSelectedListener}.
     */
    void setTabSelectionAdapter(TabLayout.OnTabSelectedListener tabSelectionCallbacks) {
        clearOnTabSelectedListeners();
        addOnTabSelectedListener(tabSelectionCallbacks);
    }

    /**
     * Marks only the given tab with the active tab color and resets all others.
     * @param activeTab The active tab to change. If null, all tabs are reset.
     */
    void setActiveTabColor(@Nullable Integer activeTab) {
        for (int i = getTabCount() - 1; i >= 0; i--) {
            TabLayout.Tab t = getTabAt(i);
            if (t == null || t.getIcon() == null) continue;
            int activeState = android.R.attr.state_selected;
            if (activeTab == null || i != activeTab) { // This means unselected.
                activeState *= -1;
            } else if (!t.isSelected()) {
                // If the active tab was set by the model, reflect that in the TabLayout's state.
                // This triggers the tab observer but as the active tab doesn't change, it's a noop.
                t.select();
            }
            t.getIcon().setColorFilter(getTabTextColors().getColorForState(new int[] {activeState},
                                               getTabTextColors().getDefaultColor()),
                    PorterDuff.Mode.SRC_IN);
        }
    }

    /**
     * Sets a description for the tab at the given position if the tab exists. Noop otherwise.
     * @param i The index of the tab to add a description to.
     * @param description A {@link String} describing the tab, e.g. for accessibility.
     */
    void setTabDescription(int i, String description) {
        TabLayout.Tab tab = getTabAt(i);
        if (tab != null) tab.setContentDescription(description);
    }

    /**
     * Sets a description for the tab at the given position if the tab exists. Noop otherwise.
     * @param i The index of the tab to add a description to.
     * @param messageId A {@link StringRes} describing the tab, e.g. for accessibility.
     */
    void setTabDescription(int i, @StringRes int messageId) {
        TabLayout.Tab tab = getTabAt(i);
        if (tab != null) tab.setContentDescription(messageId);
    }
}
