// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import android.content.Context;
import android.support.annotation.VisibleForTesting;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CompoundButton;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.ntp_customization.BottomSheetListContainerView;
import org.chromium.chrome.browser.ntp_customization.ListContainerViewDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationMetricsUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.R;

import java.util.List;

/** The view holding {@link NtpCardsListItemView} in the "New tab page cards" bottom sheet. */
@NullMarked
public class NtpCardsListContainerView extends BottomSheetListContainerView {
    public NtpCardsListContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Adds list items views to this container view.
     *
     * @param delegate The delegate contains the content for each list item view.
     */
    @Override
    public void renderAllListItems(ListContainerViewDelegate delegate) {
        HomeModulesConfigManager homeModulesConfigManager = HomeModulesConfigManager.getInstance();
        List<Integer> types = delegate.getListItems();

        for (int i = 0; i < types.size(); i++) {
            Integer type = types.get(i);
            NtpCardsListItemView listItemView = (NtpCardsListItemView) createListItemView();

            listItemView.setTitle(delegate.getListItemTitle(type, mContext));
            listItemView.setBackground(
                    AppCompatResources.getDrawable(
                            getContext(), NtpCustomizationUtils.getBackground(types.size(), i)));
            setUpSwitch(homeModulesConfigManager, listItemView, type);

            addView(listItemView);
        }
    }

    /** Returns a {@link NtpCardsListItemView}. */
    @Override
    @VisibleForTesting
    protected View createListItemView() {
        return LayoutInflater.from(getContext())
                .inflate(R.layout.ntp_customization_ntp_cards_list_item, this, false);
    }

    /**
     * Sets up the current checked state of the switch in the list item view and sets up a {@link
     * CompoundButton.OnCheckedChangeListener} to handle user selection to show or hide the magic
     * stack module.
     *
     * @param homeModulesConfigManager The manager class handles showing and hiding each magic stack
     *     module.
     * @param listItemView The list item view that contains the switch.
     * @param type The type of the magic stack module that this switch controls for showing and
     *     hiding.
     */
    @VisibleForTesting
    void setUpSwitch(
            HomeModulesConfigManager homeModulesConfigManager,
            NtpCardsListItemView listItemView,
            int type) {
        listItemView.setChecked(homeModulesConfigManager.getPrefModuleTypeEnabled(type));
        listItemView.setOnCheckedChangeListener(
                (button, newValue) -> {
                    homeModulesConfigManager.setPrefModuleTypeEnabled(type, newValue);
                    NtpCustomizationMetricsUtils.recordModuleToggledInBottomSheet(type, newValue);
                });
    }

    /**
     * Clear {@link CompoundButton.OnCheckedChangeListener} of every list item view inside this
     * container view.
     */
    @Override
    protected void destroy() {
        for (int i = 0; i < getChildCount(); i++) {
            NtpCardsListItemView child = (NtpCardsListItemView) getChildAt(i);
            child.setOnCheckedChangeListener(null);
        }
    }
}
