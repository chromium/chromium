// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.support.v4.view.ViewCompat;
import android.support.v7.preference.PreferenceViewHolder;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * A preference representing one browsing data type in ClearBrowsingDataPreferences.
 * This class allows clickable links inside the checkbox summary.
 */
public class ClearBrowsingDataCheckBoxPreference extends ChromeBaseCheckBoxPreference {
    private View mView;
    private Runnable mLinkClickDelegate;
    private boolean mHasClickableSpans;

    /**
     * Constructor for inflating from XML.
     */
    public ClearBrowsingDataCheckBoxPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * @param linkClickDelegate A Runnable that is executed when a link inside the summary is
     *                          clicked.
     */
    public void setLinkClickDelegate(Runnable linkClickDelegate) {
        mLinkClickDelegate = linkClickDelegate;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mView = holder.itemView;
        setupLayout(mView);

        final TextView textView = (TextView) mView.findViewById(android.R.id.summary);

        // Create custom onTouch listener to be able to respond to click events inside the summary.
        textView.setOnTouchListener((View v, MotionEvent event) -> {
            if (!mHasClickableSpans) {
                return false;
            }
            // Find out which character was touched.
            int offset = textView.getOffsetForPosition(event.getX(), event.getY());
            // Check if this character contains a span.
            CharSequence text = textView.getText();
            // TODO(crbug.com/783866): On some devices the SpannableString is not applied correctly.
            boolean isSpanned = text instanceof Spanned;
            RecordHistogram.recordBooleanHistogram(
                    "History.ClearBrowsingData.SpannableStringAppliedCorrectly", isSpanned);
            if (!isSpanned) {
                return false;
            }
            ClickableSpan[] types = ((Spanned) text).getSpans(offset, offset, ClickableSpan.class);

            if (types.length > 0) {
                if (event.getAction() == MotionEvent.ACTION_UP) {
                    for (ClickableSpan type : types) {
                        type.onClick(textView);
                    }
                }
                return true;
            } else {
                return false;
            }
        });
    }

    /**
     * This method modifies the default CheckBoxPreference layout.
     * @param view The view of this preference.
     */
    private void setupLayout(View view) {
        // Adjust icon padding.
        int padding = getContext().getResources().getDimensionPixelSize(R.dimen.pref_icon_padding);
        ImageView icon = (ImageView) view.findViewById(android.R.id.icon);
        ViewCompat.setPaddingRelative(
                icon, padding, icon.getPaddingTop(), 0, icon.getPaddingBottom());
    }

    public void announceForAccessibility(CharSequence announcement) {
        if (mView != null) mView.announceForAccessibility(announcement);
    }

    @Override
    public void setSummary(CharSequence summary) {
        // If there is no link in the summary invoke the default behavior.
        String summaryString = summary.toString();
        if (!summaryString.contains("<link>") || !summaryString.contains("</link>")) {
            super.setSummary(summary);
            return;
        }
        // Talkback users can't select links inside the summary because it is already a target
        // that toggles the checkbox. Links will still be read out and users can manually
        // navigate to them.
        if (AccessibilityUtil.isAccessibilityEnabled()) {
            super.setSummary(summaryString.replaceAll("</?link>", ""));
            return;
        }
        // Linkify <link></link> span.
        final SpannableString summaryWithLink = SpanApplier.applySpans(summaryString,
                new SpanApplier.SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(getContext().getResources(), (widget) -> {
                            if (mLinkClickDelegate != null) mLinkClickDelegate.run();
                        })));

        mHasClickableSpans = true;
        super.setSummary(summaryWithLink);
    }
}
