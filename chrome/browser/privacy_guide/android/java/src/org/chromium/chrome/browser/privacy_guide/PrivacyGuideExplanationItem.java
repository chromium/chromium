// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * A custom view for an item (icon + text) of a setting explanation for the privacy guide.
 */
public class PrivacyGuideExplanationItem extends LinearLayout {
    public PrivacyGuideExplanationItem(Context context, AttributeSet attrs) {
        super(context, attrs);

        View view =
                LayoutInflater.from(context).inflate(R.layout.privacy_guide_explanation_item, this);

        TypedArray a = context.obtainStyledAttributes(
                attrs, R.styleable.PrivacyGuideExplanationItem, 0, 0);

        TextView summary = (TextView) view.findViewById(R.id.summary);
        summary.setText(a.getText(R.styleable.PrivacyGuideExplanationItem_summaryText));

        Drawable icon = a.getDrawable(R.styleable.PrivacyGuideExplanationItem_iconImage);
        ColorStateList tint = a.getColorStateList(R.styleable.PrivacyGuideExplanationItem_iconTint);
        if (icon != null && tint != null) {
            icon.setColorFilter(tint.getDefaultColor(), PorterDuff.Mode.SRC_IN);
        }
        summary.setCompoundDrawablesRelativeWithIntrinsicBounds(icon, null, null, null);

        a.recycle();
    }
}
