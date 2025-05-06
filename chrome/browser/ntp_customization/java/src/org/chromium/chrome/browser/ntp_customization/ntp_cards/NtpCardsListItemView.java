// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.CompoundButton;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

/**
 * The list item view within a {@link NtpCardsListContainerView} in the "New tab page cards" bottom
 * sheet.
 */
@NullMarked
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
     * Sets the content of the title besides the material switch.
     *
     * @param title The string besides the material switch.
     */
    void setTitle(String title) {
        mMaterialSwitchWithText.setText(title);
    }

    /**
     * Sets the switch to On if is checked and Off otherwise.
     *
     * @param checked The switch view should be set to On or Off.
     */
    void setChecked(boolean checked) {
        mMaterialSwitchWithText.setChecked(checked);
    }

    /**
     * Sets the OnCheckedChangeListener of the material switch.
     *
     * @see CompoundButton#setOnCheckedChangeListener(CompoundButton.OnCheckedChangeListener).
     */
    public void setOnCheckedChangeListener(
            CompoundButton.@Nullable OnCheckedChangeListener listener) {
        mMaterialSwitchWithText.setOnCheckedChangeListener(listener);
    }

    void setMaterialSwitchWithTextForTesting(MaterialSwitchWithText switchWithText) {
        mMaterialSwitchWithText = switchWithText;
    }
}
