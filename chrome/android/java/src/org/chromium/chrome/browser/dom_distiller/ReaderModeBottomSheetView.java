// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;

/** The view for the reader mode bottom sheet. */
@NullMarked
public class ReaderModeBottomSheetView extends LinearLayout {
    private View mTitle;

    /**
     * @param context The android context.
     * @param attrs The android attributes.
     */
    public ReaderModeBottomSheetView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitle = findViewById(R.id.title);
    }

    /**
     * @return The height of the title view when peeked. Handlebar height is added automatically by
     *     the framework when showHandlebar() is true.
     */
    public int getPeekHeight() {
        return mTitle.getHeight();
    }
}
