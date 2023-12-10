// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.text.Layout;
import android.text.TextPaint;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.autofill.EditableOption;

/**
 * Utility functions for PaymentRequestSection.
 * This class is not supposed to be instantiated.
 */
public class SectionUiUtils {
    /** Avoid instantiation by accident. */
    private SectionUiUtils() {}

    /**
     * Show the section summary in the view in a single line. This is called when there is no
     * selected item in the section.
     *
     * @param context The context.
     * @param section The section to summarize.
     * @param view    The view to display the summary.
     */
    public static void showSectionSummaryInTextViewInSingeLine(
            final Context context, final SectionInformation section, final TextView view) {
        int optionCount = section.getSize();
        if (optionCount == 0) {
            view.setText(null);
            return;
        }

        // Listen for layout event to check whether the string can be fit in a single line and
        // manually elide the string in the middle if necessary.
        if (view.getLayout() == null && optionCount > 1) {
            view.addOnLayoutChangeListener(
                    new View.OnLayoutChangeListener() {
                        @Override
                        public void onLayoutChange(
                                View v,
                                int left,
                                int top,
                                int right,
                                int bottom,
                                int oldLeft,
                                int oldTop,
                                int oldRight,
                                int oldBottom) {
                            if (section.getSelectedItem() != null) {
                                view.removeOnLayoutChangeListener(this);
                                return;
                            }

                            Layout layout = view.getLayout();
                            if (layout.getEllipsisCount(0) > 0) {
                                String summary =
                                        getSectionSummaryForPreviewInASingleLine(
                                                context, section, layout, view.getPaint());
                                view.setText(summary);
                            }
                        }
                    });
        }

        String summary =
                SectionUiUtils.getSectionSummaryForPreviewInASingleLine(
                        context, section, view.getLayout(), view.getPaint());
        view.setText(summary);
    }

    private static String getSectionSummaryForPreviewInASingleLine(
            Context context,
            SectionInformation section,
            @Nullable Layout layout,
            @Nullable TextPaint paint) {
        int optionCount = section.getSize();
        assert optionCount != 0;

        EditableOption option = section.getItem(0);
        String labelSeparator = context.getString(R.string.autofill_address_summary_separator);
        String optionSummary = option.getPreviewString(labelSeparator, -1);

        // If there is only one option in the section, return the summary of that option in a single
        // line, let the TextView automatically elide it at the end.
        int moreOptionCount = optionCount - 1;
        if (moreOptionCount == 0) return optionSummary;

        // If there are more than one options in the section, then the returned string pattern is
        // "<summary of the first option>... and N more". N+1 is the total number of options in the
        // section.
        int resId = section.getPreviewStringResourceId();
        assert resId > 0;
        String summary =
                context.getResources()
                        .getQuantityString(resId, moreOptionCount, optionSummary, moreOptionCount);
        if (paint == null || layout == null) {
            // If layout or paint is null, return the full summary string. Otherwise, check and
            // shrink "<summary of the first option>" so as to make the entire summary string fits
            // in a single line.
            return summary;
        }

        int ellipsizedWidth = layout.getEllipsizedWidth();
        // This while loop will terminate since each call of getPreviewString returns a string
        // strictly shorter than the previously returned one and the appending "... and N more" must
        // be able to fit in a single line.
        while (Layout.getDesiredWidth(summary, paint) > ellipsizedWidth) {
            optionSummary = option.getPreviewString(labelSeparator, optionSummary.length());
            summary =
                    context.getResources()
                            .getQuantityString(
                                    resId, moreOptionCount, optionSummary, moreOptionCount);
        }

        return summary;
    }
}
