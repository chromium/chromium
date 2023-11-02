// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tasks.tab_management;
import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.chrome.tab_ui.R;

/**
 * Contains store opening and closing times for physical store website.
 */
public class StoreHoursCardView extends FrameLayout {
    private TextView mStoreHoursBox;
    public StoreHoursCardView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the store hours.
     */
    public void setStoreHours(String storeHours) {
        mStoreHoursBox.setText(storeHours);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        LayoutInflater.from(getContext()).inflate(R.layout.store_hours_card, this);
        mStoreHoursBox = (TextView) findViewById(R.id.store_hours);
    }
}