// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.support.annotation.DrawableRes;
import android.support.annotation.NonNull;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

/**
 * The view for a list item inside of {@link NtpCardsContainerView} on the "New tab page cards"
 * bottom sheet.
 */
public class NtpCardsListItemView extends LinearLayout {
    private MaterialSwitchWithText mMaterialSwitchWithText;

    public NtpCardsListItemView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMaterialSwitchWithText = findViewById(R.id.ntp_cards_list_item_text_with_switch);
    }

    /**
     * Set the title for the Textview besides the material switch in this {@link
     * NtpCardsListItemView}
     *
     * @param title The string besides the material switch
     */
    void setTitle(@NonNull String title) {
        mMaterialSwitchWithText.setText(title);
    }

    /**
     * Set the background of this {@link NtpCardsListItemView}
     *
     * @param resId The resource ID of the background drawable
     */
    void setBackground(@DrawableRes int resId) {
        this.setBackground(AppCompatResources.getDrawable(getContext(), resId));
    }
}
