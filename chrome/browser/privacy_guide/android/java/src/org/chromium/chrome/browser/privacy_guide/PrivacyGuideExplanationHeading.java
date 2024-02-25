// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

/** A custom view for a heading of a setting explanation for the privacy guide. */
public class PrivacyGuideExplanationHeading extends LinearLayout {
    public PrivacyGuideExplanationHeading(Context context, AttributeSet attrs) {
        super(context, attrs);

        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.privacy_guide_explanation_heading, this);

        TypedArray styledAttrs =
                context.obtainStyledAttributes(
                        attrs, R.styleable.PrivacyGuideExplanationHeading, 0, 0);

        TextView title = (TextView) view.findViewById(R.id.title);
        title.setText(styledAttrs.getText(R.styleable.PrivacyGuideExplanationHeading_titleText));

        styledAttrs.recycle();
    }
}
