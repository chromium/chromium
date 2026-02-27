// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import android.content.Context;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.R;

/**
 * The list item view within a {@link EducationalTipBottomSheetListContainerView} that is in a
 * bottom sheet.
 */
@NullMarked
public class EducationalTipSetupListBottomSheetListItemView extends ConstraintLayout {
    private ImageView mIcon;
    private TextView mTitle;
    private TextView mDescription;

    public EducationalTipSetupListBottomSheetListItemView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIcon = findViewById(R.id.list_item_icon);
        mTitle = findViewById(R.id.list_item_title);
        mDescription = findViewById(R.id.list_item_description);
    }

    void setIcon(int resId) {
        mIcon.setImageResource(resId);
    }

    void setTitle(String text) {
        mTitle.setText(text);
    }

    void setDescription(String text) {
        mDescription.setText(text);
    }

    /** Updates the UI to reflect a list item that has been completed. */
    void displayAsCompleted() {
        int disabledColor = getContext().getColor(R.color.default_text_color_disabled_list);
        mTitle.setTextColor(disabledColor);
        mTitle.setPaintFlags(mTitle.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);
        mDescription.setTextColor(disabledColor);
        mDescription.setPaintFlags(mDescription.getPaintFlags() | Paint.STRIKE_THRU_TEXT_FLAG);

        // Disable clicks on the item layout
        this.setOnClickListener(null);
        this.setClickable(false);
        findViewById(R.id.chevron).setVisibility(INVISIBLE);
    }
}
