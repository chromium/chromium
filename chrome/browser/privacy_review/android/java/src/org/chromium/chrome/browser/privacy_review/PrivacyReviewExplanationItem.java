// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_review;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

/** A custom view for an item (icon + text) of a setting explanation for the privacy review. */
public class PrivacyReviewExplanationItem extends LinearLayout {
    public PrivacyReviewExplanationItem(Context context, AttributeSet attrs) {
        super(context, attrs);

        View view = LayoutInflater.from(context).inflate(
                R.layout.privacy_review_explanation_item, this);

        TypedArray styledAttrs = context.obtainStyledAttributes(
                attrs, R.styleable.PrivacyReviewExplanationItem, 0, 0);

        TextView summary = (TextView) view.findViewById(R.id.summary);
        summary.setText(styledAttrs.getText(R.styleable.PrivacyReviewExplanationItem_summaryText));

        summary.setCompoundDrawablesRelativeWithIntrinsicBounds(
                styledAttrs.getDrawable(R.styleable.PrivacyReviewExplanationItem_iconImage), null,
                null, null);

        styledAttrs.recycle();
    }
}
