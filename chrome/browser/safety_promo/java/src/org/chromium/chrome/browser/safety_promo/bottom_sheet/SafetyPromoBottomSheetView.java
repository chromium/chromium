// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.safety_promo.R;
import org.chromium.ui.widget.ButtonCompat;

import java.util.List;
import java.util.Locale;

/** View for the Safety Promo "Show me how" bottom sheet. */
@NullMarked
public class SafetyPromoBottomSheetView extends LinearLayout {
    private ScrollView mScrollView;
    private TextView mTitleView;
    private LinearLayout mInstructionsContainer;
    private ButtonCompat mGotItButton;

    public SafetyPromoBottomSheetView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mScrollView = findViewById(R.id.safety_promo_bottom_sheet_scroll_view);
        mTitleView = findViewById(R.id.safety_promo_bottom_sheet_title);
        mInstructionsContainer =
                findViewById(R.id.safety_promo_bottom_sheet_instructions_container);
        mGotItButton = findViewById(R.id.safety_promo_bottom_sheet_got_it_button);
    }

    /** Sets the title of the sheet. */
    void setTitle(@StringRes int stringResId) {
        mTitleView.setText(stringResId);
    }

    /** Sets one numbered row per step. */
    void setInstructions(List<Integer> stepResIds) {
        mInstructionsContainer.removeAllViews();
        LayoutInflater inflater = LayoutInflater.from(getContext());
        for (int i = 0; i < stepResIds.size(); i++) {
            View stepView =
                    inflater.inflate(
                            R.layout.safety_promo_bottom_sheet_instructions_step,
                            mInstructionsContainer,
                            /* attachToRoot= */ false);
            TextView numberView =
                    stepView.findViewById(R.id.safety_promo_bottom_sheet_instructions_step_number);
            numberView.setText(String.format(Locale.getDefault(), "%d", i + 1));
            TextView textView =
                    stepView.findViewById(R.id.safety_promo_bottom_sheet_instructions_step_text);
            textView.setText(stepResIds.get(i));
            mInstructionsContainer.addView(stepView);
        }
    }

    /** Sets the click listener of the "Got it" button. */
    void setGotItButtonOnClickListener(@Nullable OnClickListener listener) {
        mGotItButton.setOnClickListener(listener);
    }

    /** Returns how far the sheet content is scrolled. */
    int getVerticalScrollOffset() {
        return mScrollView.getScrollY();
    }
}
