// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.core.widget.TextViewCompat;

import org.chromium.ui.widget.ButtonCompat;

/** Holds the current pane's {@link View}. */
public class HubPaneHostView extends FrameLayout {
    private FrameLayout mPaneFrame;
    private ButtonCompat mActionButton;

    /** Default {@link FrameLayout} constructor called by inflation. */
    public HubPaneHostView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mPaneFrame = findViewById(R.id.pane_frame);
        mActionButton = findViewById(R.id.host_action_button);
    }

    void setRootView(@Nullable View rootView) {
        mPaneFrame.removeAllViews();
        if (rootView != null) {
            mPaneFrame.addView(rootView);
        }
    }

    void setActionButtonData(@Nullable FullButtonData buttonData) {
        ApplyButtonData.apply(buttonData, mActionButton);
    }

    void setColorScheme(@HubColorScheme int colorScheme) {
        Context context = getContext();

        ColorStateList iconColor = HubColors.getIconColor(context, colorScheme);
        TextViewCompat.setCompoundDrawableTintList(mActionButton, iconColor);

        ColorStateList backgroundColor =
                HubColors.getSecondaryContainerColorStateList(context, colorScheme);
        mActionButton.setButtonColor(backgroundColor);

        @StyleRes int textAppearance = HubColors.getTextAppearanceMedium(colorScheme);
        mActionButton.setTextAppearance(textAppearance);
    }
}
