// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.content.Context;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * A preference representing one browsing data type in ClearBrowsingDataFragment.
 * This class allows clickable links inside the checkbox summary.
 */
public class ClearBrowsingDataCheckBoxPreference extends ChromeBaseCheckBoxPreference {
    private View mView;
    private Runnable mLinkClickDelegate;
    private boolean mHasClickableSpans;

    /** Constructor for inflating from XML. */
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

        final TextView textView = mView.findViewById(android.R.id.summary);

        // Create custom onTouch listener to be able to respond to click events inside the summary.
        textView.setOnTouchListener(
                (View v, MotionEvent event) -> {
                    if (!mHasClickableSpans) {
                        return false;
                    }
                    // Find out which character was touched.
                    int offset = textView.getOffsetForPosition(event.getX(), event.getY());
                    // Check if this character contains a span.
                    CharSequence text = textView.getText();
                    // TODO(crbug.com/40549355): On some devices the SpannableString is not applied
                    // correctly.
                    boolean isSpanned = text instanceof Spanned;
                    if (!isSpanned) {
                        return false;
                    }
                    ClickableSpan[] types =
                            ((Spanned) text).getSpans(offset, offset, ClickableSpan.class);

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
        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            super.setSummary(summaryString.replaceAll("</?link>", ""));
            return;
        }
        // Linkify <link></link> span.
        final SpannableString summaryWithLink =
                SpanApplier.applySpans(
                        summaryString,
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new NoUnderlineClickableSpan(
                                        getContext(),
                                        (widget) -> {
                                            if (mLinkClickDelegate != null) {
                                                mLinkClickDelegate.run();
                                            }
                                        })));

        mHasClickableSpans = true;
        super.setSummary(summaryWithLink);
    }
}
