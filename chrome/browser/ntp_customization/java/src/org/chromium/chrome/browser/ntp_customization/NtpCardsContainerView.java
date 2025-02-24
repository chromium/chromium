// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getTitleForModuleType;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;

import java.util.List;

/** The view containing {@link NtpCardsListItemView} on the "New tab page cards" bottom sheet. */
public class NtpCardsContainerView extends LinearLayout {
    public NtpCardsContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        HomeModulesConfigManager homeModulesConfigManager = HomeModulesConfigManager.getInstance();
        List<Integer> ntpCardsItems = homeModulesConfigManager.getModuleListShownInSettings();

        for (int i = 0; i < ntpCardsItems.size(); i++) {
            @ModuleDelegate.ModuleType int itemType = ntpCardsItems.get(i);

            NtpCardsListItemView listItemView =
                    (NtpCardsListItemView)
                            LayoutInflater.from(getContext())
                                    .inflate(
                                            R.layout.ntp_customization_ntp_cards_list_item,
                                            this,
                                            false);

            listItemView.setTitle(getTitleForModuleType(itemType, getResources()));
            setBackground(listItemView, ntpCardsItems.size(), i);
            addView(listItemView);
        }
    }

    private void setBackground(@NonNull NtpCardsListItemView view, int size, int index) {
        view.setBackground(getBackground(size, index));
    }

    @VisibleForTesting
    int getBackground(int size, int index) {
        if (size == 1) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_single;
        }

        if (index == 0) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_top;
        }

        if (index == size - 1) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom;
        }

        return R.drawable.ntp_customization_bottom_sheet_list_item_background_middle;
    }
}
