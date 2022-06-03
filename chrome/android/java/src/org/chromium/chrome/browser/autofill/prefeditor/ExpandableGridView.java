// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.prefeditor;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.GridView;

/**
 * This class is a customized GridView which draws items in multiple lines automatically.
 */
public class ExpandableGridView extends GridView {
    /** Constructor for when the gridview is inflated from XML. */
    public ExpandableGridView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // GridView does not work well in a ScrollView when it uses WRAP_CONTENT.
        // Instead, force it to use AT_MOST.
        // https://stackoverflow.com/questions/4523609/grid-of-images-inside-scrollview
        int heightSpec;
        if (getLayoutParams().height == LayoutParams.WRAP_CONTENT) {
            heightSpec = MeasureSpec.makeMeasureSpec(
                    Integer.MAX_VALUE & View.MEASURED_SIZE_MASK, MeasureSpec.AT_MOST);
        } else {
            heightSpec = heightMeasureSpec;
        }

        super.onMeasure(widthMeasureSpec, heightSpec);
    }
}
