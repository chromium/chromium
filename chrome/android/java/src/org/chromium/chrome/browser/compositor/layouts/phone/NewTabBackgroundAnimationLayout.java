// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Layout for the new tab background animation. */
public class NewTabBackgroundAnimationLayout extends FrameLayout {
    private ImageView mLinkIcon;

    public NewTabBackgroundAnimationLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public NewTabBackgroundAnimationLayout(Context context) {
        super(context);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mLinkIcon = findViewById(R.id.new_tab_background_animation_link_icon);
        setLinkIconTint(SemanticColorUtils.getDefaultIconColor(getContext()));
    }

    private void setLinkIconTint(@ColorInt int color) {
        mLinkIcon.setImageTintList(ColorStateList.valueOf(color));
    }
}
