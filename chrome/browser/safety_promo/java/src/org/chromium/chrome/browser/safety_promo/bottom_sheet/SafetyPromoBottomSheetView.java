// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View.OnClickListener;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_promo.R;
import org.chromium.ui.widget.ButtonCompat;

/** View for the Safety Promo "Show me how" bottom sheet. */
@NullMarked
public class SafetyPromoBottomSheetView extends LinearLayout {
    private TextView mTitleView;
    private ButtonCompat mGotItButton;

    public SafetyPromoBottomSheetView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTitleView = findViewById(R.id.safety_promo_bottom_sheet_title);
        mGotItButton = findViewById(R.id.safety_promo_bottom_sheet_got_it_button);
    }

    /** Sets the title of the sheet. */
    void setTitle(@StringRes int stringResId) {
        mTitleView.setText(stringResId);
    }

    /** Sets the click listener of the "Got it" button. */
    void setGotItButtonOnClickListener(@Nullable OnClickListener listener) {
        mGotItButton.setOnClickListener(listener);
    }
}
