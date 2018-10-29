// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar;

import java.util.List;

/**
 * Handles toolbar functionality for the Photo Picker class.
 */
public class PhotoPickerToolbar extends SelectableListToolbar<PickerBitmap> {
    public PhotoPickerToolbar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setNavigationIcon(R.drawable.btn_close);
        setNavigationContentDescription(R.string.close);

        TextView up = (TextView) mNumberRollView.findViewById(R.id.up);
        TextView down = (TextView) mNumberRollView.findViewById(R.id.down);
        ApiCompatibilityUtils.setTextAppearance(up, R.style.BlackHeadline);
        ApiCompatibilityUtils.setTextAppearance(down, R.style.BlackHeadline);
    }

    @Override
    protected void setNavigationButton(int navigationButton) {}

    @Override
    protected void showSelectionView(
            List<PickerBitmap> selectedItems, boolean wasSelectionEnabled) {
        switchToNumberRollView(selectedItems, wasSelectionEnabled);
    }

    @Override
    public void onSelectionStateChange(List<PickerBitmap> selectedItems) {
        super.onSelectionStateChange(selectedItems);

        Button done = (Button) findViewById(R.id.done);
        done.setEnabled(selectedItems.size() > 0);
    }
}
